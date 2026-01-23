#pragma once

#include "gdpdu_schema.hpp"
#include "duckdb.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace duckdb {

// Result of importing a single table
struct ImportResult {
    std::string table_name;
    int64_t row_count;
    std::string status;  // "OK" or error message
    
    ImportResult() : row_count(0), status("") {}
};

// Import all GDPdU tables from a directory
// 1. Parses index.xml
// 2. Creates tables in DuckDB
// 3. Loads data from .txt files
// Returns vector of results for each table
// column_name_field: "Name" (default) or "Description" - which XML element to use for column names
std::vector<ImportResult> import_gdpdu(Connection& conn, const std::string& directory_path, const std::string& column_name_field = "Name");

} // namespace duckdb
