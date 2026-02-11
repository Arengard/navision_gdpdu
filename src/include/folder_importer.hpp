#pragma once

#include "duckdb.hpp"
#include <string>
#include <vector>

namespace duckdb {

// Result of importing a single file
struct FileImportResult {
    std::string table_name;
    std::string file_name;
    int64_t row_count;
    int column_count;
    std::string status;  // "OK" or error message
    
    FileImportResult() : row_count(0), column_count(0), status("") {}
};

// Import all files from a folder
// options: optional DuckDB read options passed through to the reader
//   e.g. "all_varchar=true" for xlsx, "delimiter=';'" for csv
std::vector<FileImportResult> import_folder(
    Connection& conn,
    const std::string& folder_path,
    const std::string& file_type = "csv",
    const std::string& options = ""
);

} // namespace duckdb
