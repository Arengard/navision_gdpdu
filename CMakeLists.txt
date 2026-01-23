cmake_minimum_required(VERSION 3.12)

project(gdpdu VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include DuckDB via FetchContent
include(FetchContent)

FetchContent_Declare(
    duckdb
    GIT_REPOSITORY https://github.com/duckdb/duckdb.git
    GIT_TAG        v1.4.3
    GIT_SHALLOW    TRUE
)

# Configure DuckDB build options
set(BUILD_UNITTESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHELL ON CACHE BOOL "" FORCE)
set(ENABLE_SANITIZER OFF CACHE BOOL "" FORCE)
set(ENABLE_UBSAN OFF CACHE BOOL "" FORCE)
set(EXTENSION_STATIC_BUILD ON CACHE BOOL "" FORCE)

# Disable built-in extensions we don't need (speeds up build significantly)
set(BUILD_PARQUET_EXTENSION OFF CACHE BOOL "" FORCE)
set(BUILD_ICU_EXTENSION OFF CACHE BOOL "" FORCE)
set(BUILD_TPCH_EXTENSION OFF CACHE BOOL "" FORCE)
set(BUILD_TPCDS_EXTENSION OFF CACHE BOOL "" FORCE)
set(BUILD_FTS_EXTENSION OFF CACHE BOOL "" FORCE)
set(BUILD_HTTPFS_EXTENSION OFF CACHE BOOL "" FORCE)
set(BUILD_JSON_EXTENSION OFF CACHE BOOL "" FORCE)
set(BUILD_JEMALLOC_EXTENSION OFF CACHE BOOL "" FORCE)
set(BUILD_AUTOCOMPLETE_EXTENSION OFF CACHE BOOL "" FORCE)
set(SKIP_EXTENSIONS "parquet;icu;tpch;tpcds;fts;httpfs;json;jemalloc;autocomplete" CACHE STRING "" FORCE)

# Tell DuckDB where our extension config is
set(DUCKDB_EXTENSION_CONFIGS "${CMAKE_CURRENT_SOURCE_DIR}/extension_config.cmake" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(duckdb)

# Add pugixml for XML parsing (to be used in later phases)
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pugixml)
