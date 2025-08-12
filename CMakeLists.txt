# ============================================================================
#  dbf2parquetC - Conversor DBF/DBC → Parquet (Snappy) em C
#
#  Este CMakeLists.txt define como compilar o projeto usando CMake.
#
#  DEPENDÊNCIAS:
#    - GLib-2.0 (dev)
#    - Shapelib (dev)
#    - Apache Arrow-GLib (dev)
#    - Apache Parquet-GLib (dev)
#
#  PRINCIPAIS ALVOS (targets):
#    - dbf2parquet : executável principal que converte DBF/DBC em Parquet
#    - dbc2dbf     : utilitário auxiliar para extrair DBF de um DBC (Visual FoxPro)
#
#  NOTAS:
#    - Usa pkg-config para localizar bibliotecas e includes no sistema.
#    - Pode ser compilado em Linux ou Windows (MSYS2/MinGW) desde que as
#      dependências estejam instaladas.
#    - Para Windows, empacote as DLLs necessárias junto do .exe.
#
#  COMO USAR:
#    mkdir build && cd build
#    cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
#    ninja
# ============================================================================
cmake_minimum_required(VERSION 3.16)   # Versão mínima do CMake exigida

project(dbf2parquet_c C)               # Nome do projeto e linguagem (C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # Gera compile_commands.json (para IDEs e clangd)
set(CMAKE_C_STANDARD 11)               # Define padrão C11
set(CMAKE_POSITION_INDEPENDENT_CODE ON)# Código independente de posição (PIC) — útil para libs compartilhadas

# Localiza módulo pkg-config no CMake (obrigatório para detectar as libs externas)
find_package(PkgConfig REQUIRED)

# Pede ao pkg-config para localizar Arrow-GLib e salvar paths/includes/libs em variáveis
pkg_check_modules(ARROW_GLIB REQUIRED arrow-glib)

# Pede ao pkg-config para localizar Parquet-GLib
pkg_check_modules(PARQUET_GLIB REQUIRED parquet-glib)

# Pede ao pkg-config para localizar GLib 2.0
pkg_check_modules(GLIB2 REQUIRED glib-2.0)

# Pede ao pkg-config para localizar Shapelib
pkg_check_modules(SHAPELIB REQUIRED shapelib)

# --- BINÁRIO PRINCIPAL: dbf2parquet ---
add_executable(dbf2parquet             # Cria o executável principal
  src/main.c                           # Entrada principal da aplicação
  src/dbf_reader.c src/dbf_reader.h    # Leitura e parsing do DBF
  src/encoding.c  src/encoding.h       # Conversão de encoding para UTF-8
  src/arrow_writer.c src/arrow_writer.h# Escrita em formato Parquet usando Arrow
)

# Adiciona diretórios de include vindos do pkg-config
target_include_directories(dbf2parquet PRIVATE
  ${ARROW_GLIB_INCLUDE_DIRS}
  ${PARQUET_GLIB_INCLUDE_DIRS}
  ${GLIB2_INCLUDE_DIRS}
  ${SHAPELIB_INCLUDE_DIRS}
)

# Faz o link das bibliotecas detectadas pelo pkg-config
target_link_libraries(dbf2parquet
  ${ARROW_GLIB_LIBRARIES}
  ${PARQUET_GLIB_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${SHAPELIB_LIBRARIES}
)

# --- UTILITÁRIO AUXILIAR: dbc2dbf ---
# Este utilitário converte arquivos .dbc (Visual FoxPro) em .dbf descompactados
add_executable(dbc2dbf
  src/blast-dbf.c                      # Wrapper que chama o "blast"
  src/blast.c                          # Implementação do descompressor "blast" (Mark Adler)
)

# Mensagens para mostrar as versões das libs detectadas
message(STATUS "Arrow-GLib:    ${ARROW_GLIB_VERSION}")
message(STATUS "Parquet-GLib:  ${PARQUET_GLIB_VERSION}")
message(STATUS "GLib-2.0:      ${GLIB2_VERSION}")
message(STATUS "Shapelib:      ${SHAPELIB_VERSION}")
