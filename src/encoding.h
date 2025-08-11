#ifndef ENCODING_H
#define ENCODING_H

#include <stddef.h>

/* Lê o byte Language Driver ID (LDID) no offset 0x1D do .dbf.
   Retorna 0 em sucesso; -1 em erro de IO. */
int read_ldid_byte(const char *dbf_path, unsigned char *out_ldid);

/* Mapeia LDID comuns para label de codepage (iconv). Retorna NULL se desconhecido. */
const char* ldid_to_codepage(unsigned char ldid);

/* Converte bytes (em `from_cp`) para UTF-8.
   strict=1 → erro ao 1º byte inválido; strict=0 → substitui inválidos por '?'.
   Retorna 0 em sucesso; -1 falha ao abrir iconv; -2 erro de conversão (strict).
   *out_utf8 precisa de free(). */
int to_utf8(const char *from_cp,
            const char *in, size_t inlen,
            char **out_utf8, size_t *outlen,
            int strict);

#endif
