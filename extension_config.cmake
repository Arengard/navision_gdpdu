# GDPdU Extension Config
# This tells DuckDB to build our extension

duckdb_extension_load(gdpdu
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/src
    INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/src/include
    DONT_LINK
)
