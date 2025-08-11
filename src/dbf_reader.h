#ifndef DBF_READER_H
#define DBF_READER_H

#include <stddef.h>
#include "shapefil.h"

/* Tipos normalizados para mapear para Arrow */
typedef enum {
    COL_UTF8,
    COL_BOOL,
    COL_INT64,
    COL_FLOAT64,
    COL_DATE32
} ColKind;

typedef struct {
    char     name[12+1]; /* shapelib limita a 11 chars no DBF clássico */
    ColKind  kind;
    int      width;
    int      decimals;
} ColumnSpec;

typedef struct {
    DBFHandle h;
    int nfields;
    int nrecords;

    /* Para flag "deleted": lemos do arquivo bruto (cabeçalho+registros) */
    FILE *raw;
    long header_len;  /* bytes do cabeçalho */
    long record_len;  /* bytes por registro */
} DbfCtx;

/* Abre DBF e o arquivo bruto para checar deletados; detecta schema. */
int dbf_open(const char *path, DbfCtx *ctx, ColumnSpec **cols_out);

/* Fecha alças. */
void dbf_close(DbfCtx *ctx);

/* Retorna 1 se registro está deletado ('*' no 1º byte do registro), 0 caso contrário, -1 erro IO. */
int dbf_is_deleted(const DbfCtx *ctx, int row);

/* Le leitura de valores por coluna/linha:
   Retorna 1 se NULL, 0 se possui valor, -1 erro.
   Saída:
     - para COL_UTF8: *out_str alocado (precisa free), em UTF-8
     - para COL_INT64: *out_i64
     - para COL_FLOAT64: *out_f64
     - para COL_BOOL: *out_bool (0/1)
     - para COL_DATE32: *out_i32 (dias desde 1970-01-01)
*/
int dbf_read_value(const DbfCtx *ctx, const ColumnSpec *col, int col_idx, int row,
                   const char *from_cp, int strict,
                   char **out_str, long long *out_i64, double *out_f64, int *out_bool, int *out_i32);

#endif
