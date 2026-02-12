#pragma once

#include <string>
#include <vector>

namespace duckdb {

struct WebDavFile {
    std::string name;      // filename (e.g. "export2024.zip")
    std::string href;      // full path from PROPFIND response
    bool is_collection;    // true if directory
};

struct WebDavResult {
    bool success;
    std::string error_message;  // empty on success
    std::vector<WebDavFile> files;
};

struct WebDavDownloadResult {
    bool success;
    std::string error_message;
    std::string local_path;     // path to downloaded file
};

class WebDavClient {
public:
    // Construct with Nextcloud base URL (e.g. "https://cloud.example.com/remote.php/dav/files/user/exports/")
    // and credentials for HTTP Basic Auth
    WebDavClient(const std::string& base_url, const std::string& username, const std::string& password);

    // List all files in the WebDAV folder. Returns only .zip files if filter_zips=true.
    WebDavResult list_files(bool filter_zips = true);

    // Download a single file to the specified local directory. Returns path to downloaded file.
    WebDavDownloadResult download_file(const std::string& href, const std::string& local_dir);

private:
    std::string base_url_;
    std::string username_;
    std::string password_;
    std::string proto_host_port_;  // e.g. "https://cloud.example.com"
    std::string base_path_;        // e.g. "/remote.php/dav/files/user/exports/"

    std::string make_auth_header() const;
    void parse_url();
};

// Create a temporary directory for downloads. Returns the path.
// On Windows: uses GetTempPathW + a unique subdirectory name
// Cross-platform fallback: uses platform temp directory
std::string create_temp_download_dir();

// Remove a temporary directory and all its contents
void cleanup_temp_dir(const std::string& dir_path);

} // namespace duckdb
