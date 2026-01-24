#pragma once

#include "duckdb.hpp"
#include <string>

namespace duckdb {

// Export a table to GDPdU format
// Creates index.xml and data file in the specified directory
// Returns export result with status
struct ExportResult {
    std::string table_name;
    std::string file_path;
    int64_t row_count;
    std::string status;  // "OK" or error message
    
    ExportResult() : row_count(0), status("") {}
};

ExportResult export_gdpdu(Connection& conn, const std::string& export_path, const std::string& table_name);

} // namespace duckdb
