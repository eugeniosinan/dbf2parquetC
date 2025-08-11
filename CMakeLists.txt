cmake_minimum_required(VERSION 3.16)
project(dbf2parquet_c C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ARROW_GLIB REQUIRED arrow-glib)
pkg_check_modules(PARQUET_GLIB REQUIRED parquet-glib)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(SHAPELIB REQUIRED shapelib)

# --- binário principal: dbf2parquet ---
add_executable(dbf2parquet
  src/main.c
  src/dbf_reader.c src/dbf_reader.h
  src/encoding.c  src/encoding.h
  src/arrow_writer.c src/arrow_writer.h
)

target_include_directories(dbf2parquet PRIVATE
  ${ARROW_GLIB_INCLUDE_DIRS}
  ${PARQUET_GLIB_INCLUDE_DIRS}
  ${GLIB2_INCLUDE_DIRS}
  ${SHAPELIB_INCLUDE_DIRS}
)

target_link_libraries(dbf2parquet
  ${ARROW_GLIB_LIBRARIES}
  ${PARQUET_GLIB_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${SHAPELIB_LIBRARIES}
)

# --- utilitário auxiliar: dbc2dbf (blast) ---
# Certifique-se de ter baixado os arquivos:
#   src/blast.c, src/blast.h, src/blast-dbf.c
add_executable(dbc2dbf
  src/blast-dbf.c
  src/blast.c
)

# (Sem libs externas; é C puro)

# Mensagens mais claras se algo faltar
message(STATUS "Arrow-GLib:    ${ARROW_GLIB_VERSION}")
message(STATUS "Parquet-GLib:  ${PARQUET_GLIB_VERSION}")
message(STATUS "GLib-2.0:      ${GLIB2_VERSION}")
message(STATUS "Shapelib:      ${SHAPELIB_VERSION}")
