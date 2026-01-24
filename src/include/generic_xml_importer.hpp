#pragma once

#include "xml_parser_config.hpp"
#include "gdpdu_importer.hpp"
#include "duckdb.hpp"
#include <string>
#include <vector>

namespace duckdb {

// Generic XML data importer that can handle different XML formats
// Uses the parser factory to select the appropriate parser
std::vector<ImportResult> import_xml_data(
    Connection& conn,
    const std::string& directory_path,
    const std::string& parser_type = "gdpdu",
    const XmlParserConfig& config = XmlParserConfig()
);

} // namespace duckdb
