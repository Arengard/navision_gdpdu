#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace duckdb {

class Connection;

// Result of importing a single table from Nextcloud
struct NextcloudImportResult {
    std::string table_name;   // prefixed table name (e.g. "export2024_Buchungen")
    int64_t row_count;        // number of rows imported
    std::string status;       // "OK" or error description
    std::string source_zip;   // original zip filename (e.g. "export2024.zip")

    NextcloudImportResult() : row_count(0), status(""), source_zip("") {}
};

// Import all GDPdU exports from a Nextcloud folder
// 1. Lists zip files via WebDAV
// 2. Downloads each zip to temp directory
// 3. Extracts zip contents
// 4. Imports tables with prefixed names
// 5. Returns aggregated results
// Uses skip-and-continue pattern: failed zips produce error results but don't abort the batch
std::vector<NextcloudImportResult> import_from_nextcloud(
    Connection& conn,
    const std::string& nextcloud_url,
    const std::string& username,
    const std::string& password
);

} // namespace duckdb
