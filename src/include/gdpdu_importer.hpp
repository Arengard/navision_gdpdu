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
std::vector<ImportResult> import_gdpdu_navision(Connection& conn, const std::string& directory_path, const std::string& column_name_field = "Name");

// Import DATEV GDPdU data from a directory
// Similar to import_gdpdu_navision but optimized for DATEV export format:
// - Uses "Name" element for column names (DATEV standard)
// - Handles DATEV-specific CSV files
// 1. Parses index.xml
// 2. Creates tables in DuckDB
// 3. Loads data from .csv files
// Returns vector of results for each table
std::vector<ImportResult> import_gdpdu_datev(Connection& conn, const std::string& directory_path);

} // namespace duckdb
