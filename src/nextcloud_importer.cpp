#include "nextcloud_importer.hpp"
#include "webdav_client.hpp"
#include "zip_extractor.hpp"
#include "gdpdu_importer.hpp"
#include "duckdb.hpp"
#include <algorithm>
#include <cctype>

namespace duckdb {

// Sanitize zip filename to create a table name prefix
// "Export 2024.zip" -> "Export_2024"
static std::string sanitize_zip_prefix(const std::string& zip_filename) {
    std::string prefix = zip_filename;

    // Strip .zip extension (case-insensitive)
    if (prefix.size() >= 4) {
        std::string ext = prefix.substr(prefix.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".zip") {
            prefix = prefix.substr(0, prefix.size() - 4);
        }
    }

    // Replace non-alphanumeric characters with underscore
    for (size_t i = 0; i < prefix.size(); i++) {
        char c = prefix[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            prefix[i] = '_';
        }
    }

    // Collapse consecutive underscores to single underscore
    std::string collapsed;
    bool prev_underscore = false;
    for (char c : prefix) {
        if (c == '_') {
            if (!prev_underscore) {
                collapsed += c;
            }
            prev_underscore = true;
        } else {
            collapsed += c;
            prev_underscore = false;
        }
    }

    // Trim leading/trailing underscores
    size_t start = 0;
    while (start < collapsed.size() && collapsed[start] == '_') {
        start++;
    }
    size_t end = collapsed.size();
    while (end > start && collapsed[end - 1] == '_') {
        end--;
    }

    return collapsed.substr(start, end - start);
}

std::vector<NextcloudImportResult> import_from_nextcloud(
    Connection& conn,
    const std::string& nextcloud_url,
    const std::string& username,
    const std::string& password
) {
    std::vector<NextcloudImportResult> results;

    // Create WebDAV client
    WebDavClient client(nextcloud_url, username, password);

    // List zip files from Nextcloud
    auto list_result = client.list_files(true);
    if (!list_result.success) {
        NextcloudImportResult r;
        r.table_name = "(listing)";
        r.row_count = 0;
        r.status = "Failed to list files: " + list_result.error_message;
        r.source_zip = "";
        results.push_back(r);
        return results;
    }

    // Create temp download directory
    std::string temp_dir;
    try {
        temp_dir = create_temp_download_dir();
    } catch (const std::exception& e) {
        NextcloudImportResult r;
        r.table_name = "(temp_dir)";
        r.row_count = 0;
        r.status = "Failed to create temp directory: " + std::string(e.what());
        r.source_zip = "";
        results.push_back(r);
        return results;
    }

    // Process each zip file (skip-and-continue pattern)
    for (const auto& file : list_result.files) {
        std::string prefix = sanitize_zip_prefix(file.name);

        // Download the zip file
        auto dl = client.download_file(file.href, temp_dir);
        if (!dl.success) {
            NextcloudImportResult r;
            r.table_name = "(download)";
            r.row_count = 0;
            r.status = "Download failed: " + dl.error_message;
            r.source_zip = file.name;
            results.push_back(r);
            continue;
        }

        // Extract the zip file
        auto extract_result = extract_zip(dl.local_path);
        if (!extract_result.success) {
            NextcloudImportResult r;
            r.table_name = "(extract)";
            r.row_count = 0;
            r.status = "Extraction failed: " + extract_result.error_message;
            r.source_zip = file.name;
            results.push_back(r);
            continue;
        }

        // Determine the GDPdU data directory by looking for index.xml
        std::string import_path = extract_result.extract_dir;
        for (const auto& extracted_file : extract_result.extracted_files) {
            if (extracted_file == "index.xml") {
                // index.xml is at root, use extract_dir directly
                import_path = extract_result.extract_dir;
                break;
            } else if (extracted_file.size() > 10 && extracted_file.substr(extracted_file.size() - 10) == "/index.xml") {
                // index.xml is in a subdirectory
                std::string subdir = extracted_file.substr(0, extracted_file.size() - 10);
                import_path = extract_result.extract_dir + "/" + subdir;
                break;
            }
        }

        // Import the GDPdU data
        std::vector<ImportResult> import_results;
        try {
            import_results = import_gdpdu_navision(conn, import_path);
        } catch (const std::exception& e) {
            NextcloudImportResult r;
            r.table_name = "(import)";
            r.row_count = 0;
            r.status = "Import failed: " + std::string(e.what());
            r.source_zip = file.name;
            results.push_back(r);
            cleanup_extract_dir(extract_result.extract_dir);
            continue;
        }

        // Rename tables with prefix and create results
        for (const auto& import_res : import_results) {
            std::string prefixed_name = prefix + "_" + import_res.table_name;

            // Rename the table
            try {
                std::string rename_sql = "ALTER TABLE \"" + import_res.table_name + "\" RENAME TO \"" + prefixed_name + "\"";
                conn.Query(rename_sql);

                // Create success result
                NextcloudImportResult r;
                r.table_name = prefixed_name;
                r.row_count = import_res.row_count;
                r.status = import_res.status;
                r.source_zip = file.name;
                results.push_back(r);
            } catch (const std::exception& e) {
                // Rename failed, report error but continue
                NextcloudImportResult r;
                r.table_name = prefixed_name;
                r.row_count = import_res.row_count;
                r.status = "Rename failed: " + std::string(e.what());
                r.source_zip = file.name;
                results.push_back(r);
            }
        }

        // Clean up extraction directory
        cleanup_extract_dir(extract_result.extract_dir);
    }

    // Clean up temp download directory
    cleanup_temp_dir(temp_dir);

    return results;
}

} // namespace duckdb
