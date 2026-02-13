cmake_minimum_required(VERSION 3.12)

include_directories(include)

set(EXTENSION_SOURCES
    gdpdu_extension.cpp
    gdpdu_schema.cpp
    gdpdu_parser.cpp
    gdpdu_table_creator.cpp
    gdpdu_data_parser.cpp
    gdpdu_importer.cpp
    gdpdu_exporter.cpp
    xml_parser_factory.cpp
    xml_parser_registration.cpp
    generic_xml_parser.cpp
    gdpdu_xml_parser.cpp
    generic_xml_importer.cpp
    folder_importer.cpp
    nextcloud_importer.cpp
    webdav_client.cpp
    zip_extractor.cpp
)

build_static_extension(gdpdu ${EXTENSION_SOURCES})
build_loadable_extension(gdpdu " " ${EXTENSION_SOURCES})

target_link_libraries(gdpdu_loadable_extension pugixml)
target_link_libraries(gdpdu_extension pugixml)

# DuckDB's bundled httplib (used by webdav_client.cpp)
target_include_directories(gdpdu_loadable_extension PRIVATE ${duckdb_SOURCE_DIR}/third_party/httplib)
target_include_directories(gdpdu_extension PRIVATE ${duckdb_SOURCE_DIR}/third_party/httplib)

# OpenSSL for HTTPS support in WebDAV client
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
    message(STATUS "OpenSSL found: HTTPS support enabled for WebDAV client")
    message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
    target_compile_definitions(gdpdu_loadable_extension PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(gdpdu_loadable_extension OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(gdpdu_extension PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(gdpdu_extension OpenSSL::SSL OpenSSL::Crypto)
    # Static OpenSSL on Windows needs system crypto and socket libraries
    if(WIN32)
        target_link_libraries(gdpdu_loadable_extension Crypt32 Ws2_32)
        target_link_libraries(gdpdu_extension Crypt32 Ws2_32)
    endif()
else()
    message(WARNING "OpenSSL not found: WebDAV client will only support HTTP (not HTTPS)")
endif()
