#include "arrow_writer.h"
#include <stdlib.h>
#include <stddef.h>

GArrowSchema* aw_build_schema(const ColumnSpec *cols, int ncols) {
    GList *fields = NULL;
    for (int i = 0; i < ncols; i++) {
        GArrowDataType *dt = NULL;
        switch (cols[i].kind) {
            case COL_UTF8:    dt = (GArrowDataType*)garrow_string_data_type_new();  break;
            case COL_BOOL:    dt = (GArrowDataType*)garrow_boolean_data_type_new(); break;
            case COL_INT64:   dt = (GArrowDataType*)garrow_int64_data_type_new();   break;
            case COL_FLOAT64: dt = (GArrowDataType*)garrow_double_data_type_new();  break;
            case COL_DATE32:  dt = (GArrowDataType*)garrow_date32_data_type_new();  break;
            default:          dt = (GArrowDataType*)garrow_string_data_type_new();  break;
        }
        /* garrow_field_new assume ownership de dt */
        GArrowField *f = garrow_field_new(cols[i].name, dt);
        fields = g_list_append(fields, f);
        /* NÃO dar unref(dt) aqui. */
    }
    GArrowSchema *schema = garrow_schema_new(fields);
    /* O schema assume ownership dos fields; liberamos só a lista */
    g_list_free(fields);
    return schema;
}



void aw_make_builders(GArrowSchema *schema, GPtrArray **out_builders) {
    int ncols = garrow_schema_n_fields(schema);
    GPtrArray *builders = g_ptr_array_new_with_free_func(g_object_unref);
    for (int i = 0; i < ncols; i++) {
        GArrowField *field = garrow_schema_get_field(schema, i);
        GArrowDataType *dt = garrow_field_get_data_type(field);
        GArrowArrayBuilder *b = NULL;

        if (GARROW_IS_STRING_DATA_TYPE(dt)) {
            b = (GArrowArrayBuilder*)garrow_string_array_builder_new();
        } else if (GARROW_IS_BOOLEAN_DATA_TYPE(dt)) {
            b = (GArrowArrayBuilder*)garrow_boolean_array_builder_new();
        } else if (GARROW_IS_INT64_DATA_TYPE(dt)) {
            b = (GArrowArrayBuilder*)garrow_int64_array_builder_new();
        } else if (GARROW_IS_DOUBLE_DATA_TYPE(dt)) {
            b = (GArrowArrayBuilder*)garrow_double_array_builder_new();
        } else if (GARROW_IS_DATE32_DATA_TYPE(dt)) {
            b = (GArrowArrayBuilder*)garrow_date32_array_builder_new();
        } else {
            b = (GArrowArrayBuilder*)garrow_string_array_builder_new();
        }

        g_ptr_array_add(builders, b);
        /* NÃO dar unref(dt) nem unref(field) – são borrowed. */
    }
    *out_builders = builders;
}


int aw_append_row(GPtrArray *builders,
                  const ColumnSpec *cols, int ncols,
                  const DbfCtx *ctx, int row,
                  const char *from_cp, int strict)
{
    for (int c = 0; c < ncols; c++) {
        GArrowArrayBuilder *b = g_ptr_array_index(builders, c);
        char *s = NULL; long long i64 = 0; double f64 = 0.0; int bval = 0; int i32 = 0;

        int is_null = dbf_read_value(ctx, &cols[c], c, row, from_cp, strict,
                                     &s, &i64, &f64, &bval, &i32);
        if (is_null < 0) return -1;

        switch (cols[c].kind) {
            case COL_UTF8:
            default: {
                GArrowStringArrayBuilder *sb = GARROW_STRING_ARRAY_BUILDER(b);
                if (is_null) garrow_array_builder_append_null(b, NULL);
                else         garrow_string_array_builder_append_string(sb, s, NULL);
                if (s) free(s);
                break;
            }
            case COL_BOOL: {
                GArrowBooleanArrayBuilder *bb = GARROW_BOOLEAN_ARRAY_BUILDER(b);
                if (is_null) garrow_array_builder_append_null(b, NULL);
                else         garrow_boolean_array_builder_append_value(bb, bval, NULL);
                break;
            }
            case COL_INT64: {
                GArrowInt64ArrayBuilder *ib = GARROW_INT64_ARRAY_BUILDER(b);
                if (is_null) garrow_array_builder_append_null(b, NULL);
                else         garrow_int64_array_builder_append_value(ib, i64, NULL);
                break;
            }
            case COL_FLOAT64: {
                GArrowDoubleArrayBuilder *db = GARROW_DOUBLE_ARRAY_BUILDER(b);
                if (is_null) garrow_array_builder_append_null(b, NULL);
                else         garrow_double_array_builder_append_value(db, f64, NULL);
                break;
            }
            case COL_DATE32: {
                GArrowDate32ArrayBuilder *db = GARROW_DATE32_ARRAY_BUILDER(b);
                if (is_null) garrow_array_builder_append_null(b, NULL);
                else         garrow_date32_array_builder_append_value(db, i32, NULL);
                break;
            }
        }
    }
    return 0;
}

GArrowRecordBatch* aw_finish_batch(GArrowSchema *schema, GPtrArray *builders) {
    int ncols = garrow_schema_n_fields(schema);
    GList *arrays = NULL;

    for (int i = 0; i < ncols; i++) {
        GArrowArrayBuilder *b = g_ptr_array_index(builders, i);
        GError *error = NULL;
        GArrowArray *arr = garrow_array_builder_finish(b, &error);
        if (!arr) {
            if (error) g_error_free(error);
            return NULL;
        }
        arrays = g_list_append(arrays, arr);
    }

    gint64 nrows = garrow_array_get_length(GARROW_ARRAY(g_list_last(arrays)->data));
    GError *error = NULL;
    /* API 21.x: recebe schema, nrows, lista de arrays, e GError** */
    GArrowRecordBatch *batch = garrow_record_batch_new(schema, nrows, arrays, &error);
    /* O record batch assume ownership dos arrays; só liberamos a lista */
    g_list_free(arrays);

    if (!batch) {
        if (error) g_error_free(error);
        return NULL;
    }
    return batch;
}

int aw_write_parquet(const char *out_path, GArrowSchema *schema, GPtrArray *batches) {
    GError *error = NULL;

    /* Writer properties (API v21): usa GArrowCompressionType + path (NULL = default global) */
    GParquetWriterProperties *wprops = gparquet_writer_properties_new();
    /* Se sua build não tiver Snappy, troque por GARROW_COMPRESSION_TYPE_ZSTD ou _GZIP */
    gparquet_writer_properties_set_compression(wprops,
                                               GARROW_COMPRESSION_TYPE_SNAPPY,
                                               NULL);

    /* Cria writer com propriedades */
    GParquetArrowFileWriter *writer =
        gparquet_arrow_file_writer_new_path(schema, out_path, wprops, &error);
    g_object_unref(wprops);

    if (!writer) {
        if (error) { g_printerr("parquet writer error: %s\n", error->message); g_error_free(error); }
        return -1;
    }

    /* 1 row group por RecordBatch */
    for (guint i = 0; i < batches->len; i++) {
        GArrowRecordBatch *batch = g_ptr_array_index(batches, i);
        if (!gparquet_arrow_file_writer_write_record_batch(writer, batch, &error)) {
            g_object_unref(writer);
            if (error) { g_printerr("write batch error: %s\n", error->message); g_error_free(error); }
            return -2;
        }
    }

    if (!gparquet_arrow_file_writer_close(writer, &error)) {
        g_object_unref(writer);
        if (error) { g_printerr("close writer error: %s\n", error->message); g_error_free(error); }
        return -3;
    }
    g_object_unref(writer);
    return 0;
}


