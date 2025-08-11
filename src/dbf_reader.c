#include "dbf_reader.h"
#include "encoding.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "shapefil.h" /* DBFOpen, DBFGetFieldInfo, etc */

/* Lê u16 LE do header DBF nos offsets 8 (header len) e 10 (record len) */
static int read_u16_le(const unsigned char *p) {
    return (int)(p[0] | (p[1] << 8));
}

static void dump_header_min(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "dbf_open: fopen('%s') falhou: %s\n", path, strerror(errno));
        return;
    }
    unsigned char h[32] = {0};
    size_t n = fread(h, 1, sizeof(h), f);
    fclose(f);
    if (n < 12) {
        fprintf(stderr, "dbf_open: header curto (%zu bytes)\n", n);
        return;
    }
    int header_len = read_u16_le(&h[8]);
    int record_len = read_u16_le(&h[10]);
    fprintf(stderr,
            "dbf_open: header peek: FirstByte=0x%02X header_len=%d record_len=%d\n",
            (unsigned)h[0], header_len, record_len);
}

int dbf_open(const char *path, DbfCtx *ctx, ColumnSpec **cols_out) {
    memset(ctx, 0, sizeof(*ctx));

    /* shapelib do sistema não tem DBFOpenEx -> usar DBFOpen */
    ctx->h = DBFOpen(path, "rb");
    if (!ctx->h) {
        fprintf(stderr, "dbf_open: DBFOpen('%s') falhou.\n", path);
        dump_header_min(path);
        return -1;
    }

    /* também abre bruto para checar flag 'deleted' e offsets */
    ctx->raw = fopen(path, "rb");
    if (!ctx->raw) {
        fprintf(stderr, "dbf_open: fopen('%s') falhou: %s\n", path, strerror(errno));
        DBFClose(ctx->h);
        ctx->h = NULL;
        return -2;
    }

    unsigned char h32[32];
    if (fread(h32, 1, 32, ctx->raw) != 32) {
        fprintf(stderr, "dbf_open: não consegui ler 32 bytes do header\n");
        fclose(ctx->raw); ctx->raw = NULL;
        DBFClose(ctx->h); ctx->h = NULL;
        return -3;
    }

    ctx->header_len = read_u16_le(&h32[8]);
    ctx->record_len = read_u16_le(&h32[10]);

    ctx->nfields  = DBFGetFieldCount(ctx->h);
    ctx->nrecords = DBFGetRecordCount(ctx->h);

    /* monta ColumnSpec */
    ColumnSpec *cols = (ColumnSpec*)calloc((size_t)ctx->nfields, sizeof(ColumnSpec));
    if (!cols) { dbf_close(ctx); return -4; }

    for (int i = 0; i < ctx->nfields; i++) {
        char name[12]; int width = 0, decimals = 0;
        DBFFieldType t = DBFGetFieldInfo(ctx->h, i, name, &width, &decimals);
        name[11] = '\0';

        cols[i].name[0] = '\0';
        strncpy(cols[i].name, name, sizeof(cols[i].name) - 1);
        cols[i].name[sizeof(cols[i].name) - 1] = '\0';

        cols[i].width    = width;
        cols[i].decimals = decimals;

        switch (t) {
            case FTString:
            case FTInvalid: /* defensivo */
                cols[i].kind = COL_UTF8;    break;

            case FTInteger:
                cols[i].kind = COL_INT64;   break;

            case FTDouble:
                cols[i].kind = (decimals > 0 ? COL_FLOAT64 : COL_INT64);
                break;

            case FTLogical:
                cols[i].kind = COL_BOOL;    break;

            case FTDate:
                cols[i].kind = COL_DATE32;  break;

            default:
                cols[i].kind = COL_UTF8;    break;
        }
    }

    *cols_out = cols;
    return 0;
}

void dbf_close(DbfCtx *ctx) {
    if (!ctx) return;
    if (ctx->h)   { DBFClose(ctx->h); ctx->h = NULL; }
    if (ctx->raw) { fclose(ctx->raw); ctx->raw = NULL; }
    memset(ctx, 0, sizeof(*ctx));
}

int dbf_is_deleted(const DbfCtx *ctx, int row) {
    if (!ctx || !ctx->raw || row < 0 || row >= ctx->nrecords) return -1;
    long off = (long)ctx->header_len + ((long)row) * (long)ctx->record_len;
    if (fseek(ctx->raw, off, SEEK_SET) != 0) return -1;
    int c = fgetc(ctx->raw);
    if (c == EOF) return -1;
    /* '*' (0x2A) = deletado; ' ' (0x20) = ativo */
    return (c == '*') ? 1 : 0;
}

/* parse "YYYYMMDD" -> days since 1970-01-01; retorna 0 em sucesso */
static int yyyymmdd_to_days(const char *s, int *out_days) {
    if (!s || strlen(s) < 8) return -1;

    char buf[9];
    memcpy(buf, s, 8);
    buf[8] = '\0';

    int y = atoi(&buf[0]);
    int m = atoi(&buf[4]);
    int d = atoi(&buf[6]);

    if (y <= 0 || m < 1 || m > 12 || d < 1 || d > 31) return -1;

    int a  = (14 - m) / 12;
    int y1 = y + 4800 - a;
    int m1 = m + 12 * a - 3;

    long jdn = d + (153 * m1 + 2) / 5 + 365L * y1 + y1 / 4 - y1 / 100 + y1 / 400 - 32045;
    *out_days = (int)(jdn - 2440588L); /* 1970-01-01 */
    return 0;
}

int dbf_read_value(const DbfCtx *ctx, const ColumnSpec *col, int col_idx, int row,
                   const char *from_cp, int strict,
                   char **out_str, long long *out_i64, double *out_f64, int *out_bool, int *out_i32)
{
    if (!ctx || !col) return -1;

    if (DBFIsAttributeNULL(ctx->h, row, col_idx)) return 1;

    switch (col->kind) {
        case COL_UTF8: {
            const char *raw = DBFReadStringAttribute(ctx->h, row, col_idx);
            if (!raw || raw[0] == '\0') return 1;

            size_t len = strlen(raw);
            while (len > 0 && (unsigned char)raw[len - 1] <= ' ') len--;

            char *utf8 = NULL;
            size_t outlen = 0;
            int rc = to_utf8(from_cp, raw, len, &utf8, &outlen, strict);
            if (rc == -2) return -1; /* strict: falhou */
            if (rc != 0) {
                utf8 = (char*)malloc(len + 1);
                if (!utf8) return -1;
                memcpy(utf8, raw, len);
                utf8[len] = '\0';
            }
            *out_str = utf8;
            return 0;
        }

        case COL_BOOL: {
            const char *raw = DBFReadStringAttribute(ctx->h, row, col_idx);
            if (!raw || raw[0] == '\0') return 1;
            char c = (char)toupper((unsigned char)raw[0]);
            *out_bool = (c == 'Y' || c == 'T' || c == '1') ? 1 : 0;
            return 0;
        }

        case COL_INT64: {
            int ival = DBFReadIntegerAttribute(ctx->h, row, col_idx);
            *out_i64 = (long long)ival;
            return 0;
        }

        case COL_FLOAT64: {
            double d = DBFReadDoubleAttribute(ctx->h, row, col_idx);
            *out_f64 = d;
            return 0;
        }

        case COL_DATE32: {
            const char *raw = DBFReadStringAttribute(ctx->h, row, col_idx);
            if (!raw || strlen(raw) < 8) return 1;
            int days = 0;
            if (yyyymmdd_to_days(raw, &days) != 0) return 1;
            *out_i32 = days;
            return 0;
        }

        default:
            return 1;
    }
}
