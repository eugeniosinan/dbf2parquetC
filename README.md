# dbf2parquetC

Conversor **DBF/DBC → Parquet (Snappy)** escrito em C, usando **Arrow-GLib** e **Parquet-GLib**.  
Leitura de DBF via **Shapelib**. Para `.dbc` (DBF compactado Visual FoxPro), o projeto incorpora o **blast** (Mark Adler) para descompactar em um DBF temporário e então converter para Parquet.

---

## Sumário

- [Recursos](#recursos)
- [Mapeamento de tipos](#mapeamento-de-tipos)
- [Estrutura do projeto](#estrutura-do-projeto)
- [Requisitos de sistema](#requisitos-de-sistema)
- [Linux (Ubuntu/Mint)](#linux-ubuntumint)
  - [Checar dependências](#checar-dependências)
  - [Instalar dependências](#instalar-dependências)
  - [Compilar](#compilar)
  - [Testar e validar](#testar-e-validar)
  - [Resolvendos avisos/erros comuns](#resolvendos-avisoserros-comuns)
- [Windows](#windows)
  - [Opção A — WSL (recomendado)](#opção-a--wsl-recomendado)
  - [Opção B — MSYS2 nativo (intermediárioavançado)](#opção-b--msys2-nativo-intermediárioavançado)
- [Uso](#uso)
  - [DBF → Parquet](#dbf--parquet)
  - [DBC → Parquet (automático)](#dbc--parquet-automático)
  - [Opções da linha de comando](#opções-da-linha-de-comando)
- [Performance e boas práticas](#performance-e-boas-práticas)
- [Limitações conhecidas](#limitações-conhecidas)
- [Licenças e créditos](#licenças-e-créditos)

---

## Recursos

- Conversão direta **DBF → Parquet** (compressão Snappy).
- Suporte a **.dbc** (Visual FoxPro) embutido: descompacta para `.dbf` temporário e prossegue.
- Mapeamento objetivo de tipos DBF para Arrow/Parquet.
- Conversão de **encoding** configurável (`--encoding`), com modo **strict** para garantir UTF-8 limpo.
- Processamento em **lotes** (`--batch-size`) gerando **row groups** eficientes.
- Controle de registros **deletados**: pular (padrão) ou manter (`--deleted keep`).

---

## Mapeamento de tipos

| DBF         | Parquet (Arrow)                                |
|-------------|-------------------------------------------------|
| Character   | Utf8 (String)                                   |
| Logical     | Boolean                                          |
| Integer     | Int64                                            |
| Double      | Float64 (se `decimals > 0`) senão Int64         |
| Date (YYYYMMDD) | Date32 (dias desde 1970-01-01)              |
| Memo        | Utf8 (quando disponível via leitor DBF)         |

> Observação: campos `DateTime` específicos de FoxPro não são tratados como timestamp neste projeto; foco no DBF “clássico”.

---

## Estrutura do projeto

```
dbf2parquetC/
├─ CMakeLists.txt
├─ src/
│  ├─ main.c            # CLI, autodetecção .dbc, pipeline DBF->Parquet
│  ├─ dbf_reader.c/.h   # Leitura de DBF (Shapelib) + metadados/flags
│  ├─ encoding.c/.h     # Conversão para UTF-8, strict/relaxed
│  ├─ arrow_writer.c/.h # Builders Arrow, RecordBatch e escrita Parquet
│  ├─ blast.c/.h        # Descompressor 'blast' (Mark Adler)
│  └─ blast-dbf.c       # “dbc2dbf”: usa blast para gerar DBF do DBC
└─ (build/ ...)         # Saída de build (ignorado no Git)
```

---

## Requisitos de sistema

- **Compilador C** (gcc/clang)
- **CMake**
- **pkg-config**
- **GLib-2.0 (dev)**
- **Shapelib (dev)**
- **Arrow-GLib** e **Parquet-GLib** (dev)

---

## Linux (Ubuntu/Mint)

### Checar dependências

```bash
# GCC
gcc --version || echo "gcc não encontrado"
# CMake
cmake --version || echo "cmake não encontrado"
# pkg-config
pkg-config --version || echo "pkg-config não encontrado"

# GLib e Shapelib
pkg-config --modversion glib-2.0 || echo "glib-2.0 não encontrado"
pkg-config --modversion shapelib || echo "shapelib não encontrado"

# Arrow-GLib e Parquet-GLib
pkg-config --modversion arrow-glib || echo "arrow-glib não encontrado"
pkg-config --modversion parquet-glib || echo "parquet-glib não encontrado"
```

### Instalar dependências

1) **Habilitar repositório oficial do Apache Arrow**:

```bash
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://apache.github.io/arrow/apt/source.asc \
  | sudo tee /etc/apt/keyrings/apache-arrow-apt-source.asc >/dev/null

cat <<'EOF' | sudo tee /etc/apt/sources.list.d/apache-arrow.sources
Types: deb
URIs: https://packages.apache.org/artifactory/arrow/ubuntu/
Suites: jammy
Components: main
Signed-By: /etc/apt/keyrings/apache-arrow-apt-source.asc
EOF

sudo apt update
```

2) **Instalar pacotes**:

```bash
sudo apt install -y \
  build-essential cmake pkg-config \
  libglib2.0-dev libshp-dev \
  libarrow-glib-dev libparquet-glib-dev
```

### Compilar

```bash
mkdir -p build
cd build
cmake ..
make -j"$(nproc)"
```

### Testar e validar

```bash
./dbf2parquet \
  --input /caminho/arquivo.dbf \
  --output /caminho/arquivo.parquet \
  --encoding auto --deleted skip

./dbf2parquet \
  --input /caminho/arquivo.dbc \
  --output /caminho/arquivo.parquet \
  --encoding auto --deleted keep

duckdb -c "SELECT COUNT(*) FROM '/caminho/arquivo.parquet';"
duckdb -c "SELECT * FROM '/caminho/arquivo.parquet' LIMIT 10;"
```

### Resolvendos avisos/erros comuns

- **pkg-config não encontra arrow-glib/parquet-glib**  
  Revise o bloco do repositório do Apache Arrow.
- **UTF-8 inválido**: rode sem `--encoding-strict` ou force encoding correto.

---

## Windows

### Opção A — WSL (recomendado)
- Instale WSL com Ubuntu.
- Siga exatamente os passos de Linux dentro do WSL.

### Opção B — MSYS2 nativo (avançado)
- Instale MSYS2.
- Pacotes:
```bash
pacman -S --needed base-devel git mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake mingw-w64-x86_64-pkg-config \
  mingw-w64-x86_64-glib2 mingw-w64-x86_64-shapelib
```
- Arrow-GLib/Parquet-GLib devem ser compilados manualmente ou usar WSL.

---

## Uso

```bash
./dbf2parquet \
  --input /caminho/arquivo.dbf \
  --output /caminho/arquivo.parquet \
  --encoding auto --deleted skip

./dbf2parquet \
  --input /caminho/arquivo.dbc \
  --output /caminho/arquivo.parquet \
  --encoding auto --deleted keep
```

**Opções:**
- `--input <PATH>`
- `--output <PATH>`
- `--encoding <LABEL>` (`auto`, `cp1252`, `utf-8`, etc.)
- `--encoding-strict`
- `--batch-size <N>`
- `--deleted <skip|keep>`

---

## Performance e boas práticas
- Lotes maiores → arquivos menores e leitura mais rápida.
- Use `--encoding-strict` para UTF-8 limpo (falha em bytes inválidos).

---

## Limitações conhecidas
- `DateTime` de FoxPro não tratado como timestamp.
- `.dbc` raros podem gerar headers não padrão.

---

## Licenças e créditos
- **Shapelib** — MIT-like  
- **Arrow-GLib / Parquet-GLib** — Apache 2.0  
- **blast.c / blast.h (Mark Adler)** — permissiva  
- **dbf2parquetC** — Apache-2.0

---

## Clonar e publicar no GitHub

```bash
git clone git@github.com:<SEU_USUARIO>/dbf2parquetC.git
# ou:
# git init dbf2parquetC && cd dbf2parquetC

cat > .gitignore <<'EOF'
/build/
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile
compile_commands.json
dbf2parquet
dbc2dbf
*.exe
.vscode/
.DS_Store
EOF

git add .
git commit -m "dbf2parquetC: conversor DBF/DBC -> Parquet"
git branch -M main
git remote add origin git@github.com:<SEU_USUARIO>/dbf2parquetC.git
git push -u origin main
```
