#ifndef ARROW_WRITER_H
#define ARROW_WRITER_H

#include <arrow-glib/arrow-glib.h>
#include <parquet-glib/parquet-glib.h>
#include "dbf_reader.h"

/* Constrói o schema Arrow a partir das colunas DBF */
GArrowSchema* aw_build_schema(const ColumnSpec *cols, int ncols);

/* Cria array builders conforme o schema (mesma ordem das colunas) */
void aw_make_builders(GArrowSchema *schema, GPtrArray **out_builders);

/* Faz append de uma linha (valores da linha corrente) nos builders */
int aw_append_row(GPtrArray *builders,
                  const ColumnSpec *cols, int ncols,
                  const DbfCtx *ctx, int row,
                  const char *from_cp, int strict);

/* Finaliza builders em arrays e empacota num RecordBatch */
GArrowRecordBatch* aw_finish_batch(GArrowSchema *schema, GPtrArray *builders);

/* Escreve uma lista de RecordBatches em Parquet Snappy */
int aw_write_parquet(const char *out_path, GArrowSchema *schema, GPtrArray *batches);

#endif
