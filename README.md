# dbf2parquetC

Conversor **DBF/DBC → Parquet (Snappy)** escrito em C, usando **Arrow-GLib** e **Parquet-GLib**.  
Leitura de DBF via **Shapelib**. Para `.dbc` (DBF compactado Visual FoxPro), o projeto incorpora o **blast** (Mark Adler) para descompactar em um DBF temporário e então converter para Parquet.

---

## Recursos

- Conversão direta **DBF → Parquet** (compressão Snappy)
- Suporte a `.dbc` (Visual FoxPro) embutido
- Mapeamento objetivo de tipos DBF para Arrow/Parquet
- Conversão de encoding configurável (`--encoding`), com modo **strict**
- Processamento em lotes (`--batch-size`) gerando row groups eficientes
- Controle de registros deletados: pular (default) ou manter

---

## Uso rápido (binário pronto)

Baixe o binário para o seu sistema na [página de Releases](../../releases):

```bash
# Linux
./dbf2parquet --input arquivo.dbf --output arquivo.parquet

# Windows (cmd)
dbf2parquet.exe --input arquivo.dbf --output arquivo.parquet
```

Para ver a ajuda:
```bash
dbf2parquet --help
```

---

## Encodings compatíveis

A opção `--encoding` aceita os seguintes valores:

- `auto` *(padrão)* — detecta automaticamente com base no cabeçalho DBF ou assume cp1252.
- `cp1252` — Windows-1252 (Latin-1 modificado, usado no Brasil/Portugal).
- `cp850` — OEM Latin-1 (DOS).
- `cp437` — OEM US (DOS original).
- `cp1250` — Windows-1250 (Europa Central).
- `cp1251` — Windows-1251 (Cirílico).
- `utf-8` — já em UTF-8 (sem conversão).

Use `--encoding-strict` para abortar a conversão no primeiro byte inválido.

---

## Compilação

As instruções detalhadas para Linux e Windows estão em:
[build_instructions.md](build_instructions.md)

---

## Licença

- **dbf2parquetC** — Apache 2.0  
- **Arrow-GLib / Parquet-GLib** — Apache 2.0  
- **Shapelib** — MIT-like  
- **blast.c / blast.h (Mark Adler)** — permissiva

---

## Downloads

Baixe o binário para o seu sistema na [página de Releases](https://github.com/eugeniosinan/dbf2parquetC/releases).
