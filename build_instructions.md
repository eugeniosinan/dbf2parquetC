# Instruções de compilação — dbf2parquetC

## Linux

### Detectar versão do Ubuntu/Debian
```bash
. /etc/os-release
echo "Você está usando: $NAME $VERSION_CODENAME ($VERSION_ID)"
```
> Use o valor de `$VERSION_CODENAME` (ex.: `jammy`, `focal`, `bookworm`) no passo de habilitar o repositório do Apache Arrow.

---

### 1. Habilitar repositório do Apache Arrow

```bash
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://apache.github.io/arrow/apt/source.asc \
  | sudo tee /etc/apt/keyrings/apache-arrow-apt-source.asc >/dev/null

cat <<EOF | sudo tee /etc/apt/sources.list.d/apache-arrow.sources
Types: deb
URIs: https://packages.apache.org/artifactory/arrow/ubuntu/
Suites: $VERSION_CODENAME
Components: main
Signed-By: /etc/apt/keyrings/apache-arrow-apt-source.asc
EOF

sudo apt update
```

---

### 2. Instalar dependências

```bash
sudo apt install -y \
  build-essential cmake pkg-config \
  libglib2.0-dev libshp-dev \
  libarrow-glib-dev libparquet-glib-dev
```

---

### 3. Compilar

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j"$(nproc)"
```

---

### 4. Testar

```bash
./dbf2parquet --help
```

---

## Windows (64-bits) — MSYS2 UCRT64

### 1. Instalar MSYS2

Baixe de https://www.msys2.org/ e abra o atalho **MSYS2 UCRT64**.

Atualize:
```bash
pacman -Syu
# Feche e reabra
pacman -Syu
```

---

### 2. Ferramentas e dependências

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-arrow \
  mingw-w64-ucrt-x86_64-glib2 \
  mingw-w64-ucrt-x86_64-shapelib \
  mingw-w64-ucrt-x86_64-libiconv \
  mingw-w64-ucrt-x86_64-ntldd
```

---

### 3. Checar dependências

```bash
pkg-config --modversion arrow-glib
pkg-config --modversion parquet-glib
pkg-config --modversion glib-2.0
pkg-config --modversion shapelib
```
Se todos retornarem uma versão, está OK.

---

### 4. Compilar

```bash
cd /caminho/para/dbf2parquetC
rm -rf build
mkdir build && cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja
```

---

### 5. Empacotar para distribuição

```bash
mkdir -p dist
cp dbf2parquet.exe dbc2dbf.exe dist/

# copiar DLLs necessárias
ntldd -R ./dbf2parquet.exe | grep -i 'ucrt64\\bin' | \
  sed -E 's@.*ucrt64\\bin\\([^ >]+).*@\1@' | sort -u | \
  while read -r dll; do cp -u "/c/msys64/ucrt64/bin/$dll" dist/; done

ntldd -R ./dbc2dbf.exe | grep -i 'ucrt64\\bin' | \
  sed -E 's@.*ucrt64\\bin\\([^ >]+).*@\1@' | sort -u | \
  while read -r dll; do cp -u "/c/msys64/ucrt64/bin/$dll" dist/; done
```

Agora a pasta `dist/` tem o `.exe` e todas as DLLs.  
Zip essa pasta para distribuir.

---

## Observações

- Para Windows 32-bits, use o shell **MSYS2 MINGW32** e pacotes `mingw-w64-i686-*`.
- Binários do MSYS2 UCRT64 funcionam em **Windows 10/11 x64**.  
  Não há suporte oficial para Windows Vista/7/8.
- Licenciamento: mantenha as licenças das bibliotecas junto ao binário distribuído.
