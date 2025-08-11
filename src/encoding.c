#include "encoding.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iconv.h>

/* --- LDID --- */
int read_ldid_byte(const char *dbf_path, unsigned char *out_ldid) {
    FILE *f = fopen(dbf_path, "rb");
    if (!f) return -1;
    unsigned char header[32];
    size_t n = fread(header, 1, sizeof(header), f);
    fclose(f);
    if (n < sizeof(header)) return -1;
    *out_ldid = header[0x1D]; /* 29 decimal */
    return 0;
}

const char* ldid_to_codepage(unsigned char id) {
    switch (id) {
        case 0x57: return "CP1252"; /* dBase IV ANSI */
        case 0x03: return "CP1252"; /* dBase III (na prática, muito ANSI em Windows) */
        case 0x7D: return "CP850";  /* OEM Multilingual */
        case 0x7B: return "CP437";  /* OEM US */
        case 0x64: return "CP1250";
        case 0x65: return "CP1251";
        case 0x66: case 0x67: case 0x68: case 0x69: return "CP1252";
        default: return NULL;
    }
}

/* --- iconv --- */
int to_utf8(const char *from_cp,
            const char *in, size_t inlen,
            char **out_utf8, size_t *outlen,
            int strict)
{
    const char *src_cp = from_cp ? from_cp : "CP1252";
    iconv_t cd = iconv_open("UTF-8//TRANSLIT", src_cp);
    if (cd == (iconv_t)-1) return -1;

    size_t cap = inlen * 4 + 8;
    char *out = (char*)malloc(cap);
    if (!out) { iconv_close(cd); return -1; }

    char *pin = (char*)in;
    char *pout = out;
    size_t inleft = inlen, outleft = cap;

    while (inleft > 0) {
        size_t r = iconv(cd, &pin, &inleft, &pout, &outleft);
        if (r == (size_t)-1) {
            if (!strict) {
                /* substitui por '?' e avança 1 byte */
                if (outleft == 0) { free(out); iconv_close(cd); return -1; }
                *pout++ = '?'; outleft--;
                pin++; inleft--;
                continue;
            } else {
                free(out); iconv_close(cd); return -2;
            }
        }
    }
    *outlen = (size_t)(pout - out);
    /* garanta NUL no fim (Arrow-GLib recebe const gchar*) */
    if (outleft == 0) {
        /* realoca se necessário para caber o NUL */
        size_t used = *outlen;
        size_t newcap = used + 1;
        char *tmp = (char*)realloc(out, newcap);
        if (!tmp) { free(out); iconv_close(cd); return -1; }
        out = tmp;
        pout = out + used;
        outleft = 1;
    }
    *pout = '\0';

    *out_utf8 = out;
    iconv_close(cd);
    return 0;
}
