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
)

build_static_extension(gdpdu ${EXTENSION_SOURCES})
build_loadable_extension(gdpdu " " ${EXTENSION_SOURCES})

target_link_libraries(gdpdu_loadable_extension pugixml)
target_link_libraries(gdpdu_extension pugixml)
