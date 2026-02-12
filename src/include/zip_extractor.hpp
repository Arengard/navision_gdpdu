#pragma once

#include <string>
#include <vector>

namespace duckdb {

struct ZipExtractResult {
    bool success;
    std::string error_message;      // empty on success
    std::string extract_dir;        // path to temp directory with extracted files
    std::vector<std::string> extracted_files;  // list of extracted file paths (relative to extract_dir)
};

// Extract a zip file to a temporary directory.
// Returns result with extract_dir path and list of extracted files on success.
// On error, returns result with success=false and error_message populated.
ZipExtractResult extract_zip(const std::string& zip_path);

// Remove an extraction directory and all its contents.
// Delegates to cleanup_temp_dir() from webdav_client.hpp.
void cleanup_extract_dir(const std::string& dir_path);

} // namespace duckdb
