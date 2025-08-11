#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <getopt.h>
#include <glib.h>
#include "dbf_reader.h"
#include "encoding.h"
#include "arrow_writer.h"
#include <limits.h>

/* Concatena base + ext garantindo capacidade; retorna 0 ok, -1 erro */
static int join_with_ext(char *dst, size_t dstsz, const char *base, const char *ext) {
    size_t nb = strlen(base);
    size_t ne = strlen(ext);
    if (nb + ne + 1 > dstsz) return -1;
    memcpy(dst, base, nb);
    memcpy(dst + nb, ext, ne);
    dst[nb + ne] = '\0';
    return 0;
}

/* Cópia simples de arquivo binário */
static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) { fprintf(stderr, "copy_file: fopen('%s'): %s\n", src, strerror(errno)); return -1; }
    FILE *out = fopen(dst, "wb");
    if (!out) { fprintf(stderr, "copy_file: fopen('%s'): %s\n", dst, strerror(errno)); fclose(in); return -1; }

    char buf[1 << 16];
    size_t n;
    int rc = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { rc = -1; fprintf(stderr, "copy_file: erro ao escrever '%s'\n", dst); break; }
    }
    if (ferror(in)) { rc = -1; fprintf(stderr, "copy_file: erro ao ler '%s'\n", src); }
    fclose(out);
    fclose(in);
    if (rc != 0) remove(dst);
    return rc;
}


typedef struct {
    const char *input;
    const char *output;
    const char *encoding;    /* "auto" | "cp1252" | "cp850" | ... */
    int encoding_strict;     /* 0/1 */
    int batch_size;          /* default 100000 */
    int keep_deleted;        /* 0(skip) / 1(keep) */
} Cli;

static void print_help() {
    printf(
"Converte arquivos DBF/DBC para Parquet (Snappy), mapeando tipos diretamente.\n\n"
"Uso:\n"
"  dbf2parquet --input <arquivo.dbf|dbc> --output <arquivo.parquet> [opções]\n\n"
"Opções:\n"
"  --input <PATH>            DBF de entrada (ou DBC; requer .dbt/.fpt para MEMO)\n"
"  --output <PATH>           Parquet de saída\n"
"  --encoding <LABEL>        'auto' (default), cp1252, cp850, cp437, cp1250, cp1251, utf-8\n"
"  --encoding-strict         Falha ao primeiro byte inválido na conversão para UTF-8\n"
"  --batch-size <N>          Linhas por lote/row-group (default: 100000)\n"
"  --deleted <skip|keep>     Ignorar (default) ou incluir registros deletados\n"
"  -h, --help                Mostrar ajuda\n"
    );
}

static int parse_cli(int argc, char **argv, Cli *cli) {
    static struct option long_opts[] = {
        {"input", required_argument, 0, 0},
        {"output", required_argument, 0, 0},
        {"encoding", required_argument, 0, 0},
        {"encoding-strict", no_argument, 0, 0},
        {"batch-size", required_argument, 0, 0},
        {"deleted", required_argument, 0, 0},
        {"help", no_argument, 0, 'h'},
        {0,0,0,0}
    };
    cli->input = NULL;
    cli->output = NULL;
    cli->encoding = "auto";
    cli->encoding_strict = 0;
    cli->batch_size = 100000;
    cli->keep_deleted = 0;

    int opt, idx;
    while ((opt = getopt_long(argc, argv, "h", long_opts, &idx)) != -1) {
        if (opt == 'h') { print_help(); exit(0); }
        if (opt == 0) {
            const char *name = long_opts[idx].name;
            if (strcmp(name, "input")==0) cli->input = optarg;
            else if (strcmp(name, "output")==0) cli->output = optarg;
            else if (strcmp(name, "encoding")==0) cli->encoding = optarg;
            else if (strcmp(name, "encoding-strict")==0) cli->encoding_strict = 1;
            else if (strcmp(name, "batch-size")==0) cli->batch_size = atoi(optarg);
            else if (strcmp(name, "deleted")==0) {
                if (strcmp(optarg, "keep")==0) cli->keep_deleted = 1;
                else if (strcmp(optarg, "skip")==0) cli->keep_deleted = 0;
                else { fprintf(stderr, "Valor inválido para --deleted: %s\n", optarg); return -1; }
            }
        }
    }

    if (!cli->input || !cli->output) {
        print_help();
        return -1;
    }
    return 0;
}

static const char* resolve_codepage(const char *dbf_path, const char *encoding_cli) {
    if (encoding_cli && strcasecmp(encoding_cli, "auto") != 0) return encoding_cli;
    unsigned char ldid = 0;
    if (read_ldid_byte(dbf_path, &ldid) == 0) {
        const char *cp = ldid_to_codepage(ldid);
        if (cp) return cp;
    }
    return "CP1252";
}

int main(int argc, char **argv) {
    Cli cli;
    if (parse_cli(argc, argv, &cli) != 0) return 2;

    char tmp_dbf[4096] = {0};
    const char *in_path = cli.input;

    /* Detecta extensão .dbc (case-insensitive) */
    /* Detecta .dbc e extrai */
    const char *dot = strrchr(cli.input, '.');
    if (dot) {
        char ext[8]; size_t n = 0;
        for (const char *p = dot + 1; *p && n < sizeof(ext)-1; ++p)
            ext[n++] = (char)tolower((unsigned char)*p);
        ext[n] = '\0';

        if (strcmp(ext, "dbc") == 0) {
            /* Sempre salve temporário com .dbf (shapelib gosta de extensão certa) */
            if (join_with_ext(tmp_dbf, sizeof(tmp_dbf), cli.output, ".dbf") != 0) {
                fprintf(stderr, "Caminho muito longo ao criar temporário .dbf\n");
                return 3;
            }
            fprintf(stderr, "Detectado .dbc, descompactando para: %s\n", tmp_dbf);

            /* 1) Tenta usar dbc2dbf local */
            char cmd[8192];
            int rc;
            rc = snprintf(cmd, sizeof(cmd), "./dbc2dbf '%s' '%s'", cli.input, tmp_dbf);
            if (rc < 0 || (size_t)rc >= sizeof(cmd)) { fprintf(stderr, "Comando muito longo\n"); return 3; }
            rc = system(cmd);

            /* 2) Fallback: dbc2dbf do PATH */
            if (rc != 0) {
                rc = snprintf(cmd, sizeof(cmd), "dbc2dbf '%s' '%s'", cli.input, tmp_dbf);
                if (rc < 0 || (size_t)rc >= sizeof(cmd)) { fprintf(stderr, "Comando muito longo\n"); return 3; }
                rc = system(cmd);
            }

            /* 3) Se ainda falhar, tente heurística de “arquivo já extraído” ao lado do .dbc */
            if (rc != 0) {
                char base[PATH_MAX];
                size_t len = (size_t)(dot - cli.input);
                if (len >= sizeof(base)) { fprintf(stderr, "Caminho base muito longo\n"); return 3; }
                memcpy(base, cli.input, len);
                base[len] = '\0';

                char cand1[PATH_MAX], cand2[PATH_MAX];
                int ok1 = (join_with_ext(cand1, sizeof(cand1), base, ".DBF") == 0);
                int ok2 = (join_with_ext(cand2, sizeof(cand2), base, ".dbf") == 0);

                int copied = 0;
                if (ok1 && access(cand1, R_OK) == 0 && copy_file(cand1, tmp_dbf) == 0) copied = 1;
                else if (ok2 && access(cand2, R_OK) == 0 && copy_file(cand2, tmp_dbf) == 0) copied = 1;

                if (!copied) {
                    fprintf(stderr,
                            "Falha ao descompactar .dbc e não encontrei .DBF ao lado.\n"
                            "Verifique se 'dbc2dbf' está no mesmo diretório ou no PATH.\n");
                    return 3;
                }
            }

            in_path = tmp_dbf;
        }
    }


    const char *from_cp = resolve_codepage(in_path, cli.encoding);
    fprintf(stderr, "Encoding: %s (strict=%d)\n", from_cp, cli.encoding_strict);

    DbfCtx ctx;
    ColumnSpec *cols = NULL;
    if (dbf_open(in_path, &ctx, &cols) != 0) {
        fprintf(stderr, "Erro abrindo DBF.\n");
        if (tmp_dbf[0]) remove(tmp_dbf);
        return 4;
    }

    /* Schema Arrow */
    GArrowSchema *schema = aw_build_schema(cols, ctx.nfields);

    /* Loop por lotes → cria builders, append linhas, finish em RecordBatch, acumula e escreve */
    GPtrArray *batches = g_ptr_array_new_with_free_func(g_object_unref);
    int row = 0;
    while (row < ctx.nrecords) {
        GPtrArray *builders = NULL;
        aw_make_builders(schema, &builders);

        int appended = 0;
        for (; row < ctx.nrecords && appended < cli.batch_size; row++) {
            int del = dbf_is_deleted(&ctx, row);
            if (del < 0) { fprintf(stderr, "Erro lendo flag deleted.\n"); dbf_close(&ctx); return 5; }
            if (!cli.keep_deleted && del == 1) continue;

            if (aw_append_row(builders, cols, ctx.nfields, &ctx, row, from_cp, cli.encoding_strict) != 0) {
                fprintf(stderr, "Erro de conversão (encoding strict?) na linha %d.\n", row);
                dbf_close(&ctx);
                if (tmp_dbf[0]) remove(tmp_dbf);
                return 6;
            }
            appended++;
        }

        GArrowRecordBatch *batch = aw_finish_batch(schema, builders);
        g_ptr_array_add(batches, batch);
        g_ptr_array_free(builders, TRUE);
    }

    if (aw_write_parquet(cli.output, schema, batches) != 0) {
        fprintf(stderr, "Falha ao escrever Parquet.\n");
        dbf_close(&ctx);
        if (tmp_dbf[0]) remove(tmp_dbf);
        return 7;
    }

    g_ptr_array_free(batches, TRUE);
    g_object_unref(schema);
    dbf_close(&ctx);
    free(cols);

    if (tmp_dbf[0]) remove(tmp_dbf); /* limpa temporário */
    return 0;
}
