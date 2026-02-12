#include "webdav_client.hpp"
#include "httplib.hpp"
#include "pugixml.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace duckdb {

// Base64 encoding implementation
static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static std::string base64_encode(const std::string& input) {
    std::string result;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = input.size();
    const unsigned char* bytes_to_encode = reinterpret_cast<const unsigned char*>(input.c_str());

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                result += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++)
            result += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            result += '=';
    }

    return result;
}

// URL percent-decode implementation
static std::string url_decode(const std::string& encoded) {
    std::string result;
    result.reserve(encoded.size());

    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            int value = 0;
            std::istringstream hex_stream(encoded.substr(i + 1, 2));
            hex_stream >> std::hex >> value;
            if (!hex_stream.fail()) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += encoded[i];
            }
        } else if (encoded[i] == '+') {
            result += ' ';
        } else {
            result += encoded[i];
        }
    }

    return result;
}

// Extract filename from path
static std::string extract_filename(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    if (last_slash != std::string::npos && last_slash + 1 < path.size()) {
        return path.substr(last_slash + 1);
    }
    return path;
}

// Case-insensitive string ending check
static bool ends_with_ignore_case(const std::string& str, const std::string& suffix) {
    if (suffix.size() > str.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(),
                     [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

WebDavClient::WebDavClient(const std::string& base_url, const std::string& username, const std::string& password)
    : base_url_(base_url), username_(username), password_(password) {
    parse_url();
}

void WebDavClient::parse_url() {
    std::string url = base_url_;

    // Find protocol
    size_t proto_end = url.find("://");
    if (proto_end == std::string::npos) {
        // No protocol specified, assume https
        url = "https://" + url;
        proto_end = 5; // length of "https"
    }

    // Find start of path
    size_t path_start = url.find('/', proto_end + 3);

    if (path_start != std::string::npos) {
        proto_host_port_ = url.substr(0, path_start);
        base_path_ = url.substr(path_start);
    } else {
        proto_host_port_ = url;
        base_path_ = "/";
    }

    // Ensure base_path_ ends with /
    if (!base_path_.empty() && base_path_.back() != '/') {
        base_path_ += '/';
    }
}

std::string WebDavClient::make_auth_header() const {
    std::string credentials = username_ + ":" + password_;
    return "Basic " + base64_encode(credentials);
}

WebDavResult WebDavClient::list_files(bool filter_zips) {
    WebDavResult result;
    result.success = false;

    try {
        // Create HTTP client
        duckdb_httplib::Client client(proto_host_port_.c_str());

        // Configure SSL and timeouts
        client.enable_server_certificate_verification(false);
        client.set_connection_timeout(30, 0); // 30 seconds
        client.set_read_timeout(30, 0);
        client.set_write_timeout(30, 0);

        // Build PROPFIND request
        duckdb_httplib::Request req;
        req.method = "PROPFIND";
        req.path = base_path_;
        req.set_header("Authorization", make_auth_header().c_str());
        req.set_header("Depth", "1");
        req.set_header("Content-Type", "application/xml");
        req.body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                   "<d:propfind xmlns:d=\"DAV:\">"
                   "<d:prop><d:resourcetype/></d:prop>"
                   "</d:propfind>";

        // Send request
        auto res = client.send(req);

        // Check for connection errors
        if (!res) {
            result.error_message = "Connection failed to " + proto_host_port_ + " - check URL and network connectivity";
            return result;
        }

        // Check HTTP status
        if (res->status == 401) {
            result.error_message = "Authentication failed: check username and password";
            return result;
        }

        if (res->status != 207) {
            std::ostringstream oss;
            oss << "PROPFIND request failed with status " << res->status;
            if (!res->body.empty()) {
                oss << ": " << res->body.substr(0, 200);
            }
            result.error_message = oss.str();
            return result;
        }

        // Parse XML response
        pugi::xml_document doc;
        pugi::xml_parse_result parse_result = doc.load_string(res->body.c_str());

        if (!parse_result) {
            result.error_message = "Failed to parse PROPFIND XML response: " + std::string(parse_result.description());
            return result;
        }

        // Find all response elements (try different namespace prefixes)
        std::vector<pugi::xml_node> response_nodes;

        // Try common namespace prefixes
        for (auto node : doc.children()) {
            for (auto response : node.children()) {
                std::string name = response.name();
                // Check if this is a response element (handles d:response, D:response, response, etc.)
                if (name.find("response") != std::string::npos ||
                    name == "response" ||
                    name == "d:response" ||
                    name == "D:response") {
                    response_nodes.push_back(response);
                }
            }
        }

        // Process each response
        for (const auto& response : response_nodes) {
            WebDavFile file;

            // Extract href
            pugi::xml_node href_node;
            for (auto child : response.children()) {
                std::string child_name = child.name();
                if (child_name.find("href") != std::string::npos) {
                    href_node = child;
                    break;
                }
            }

            if (!href_node) continue;

            file.href = href_node.child_value();

            // Skip the folder itself (base_path)
            if (file.href == base_path_ || file.href == base_path_.substr(0, base_path_.size() - 1)) {
                continue;
            }

            // Check if it's a collection
            file.is_collection = false;
            for (auto propstat : response.children()) {
                std::string propstat_name = propstat.name();
                if (propstat_name.find("propstat") != std::string::npos) {
                    for (auto prop : propstat.children()) {
                        std::string prop_name = prop.name();
                        if (prop_name.find("prop") != std::string::npos) {
                            for (auto resourcetype : prop.children()) {
                                std::string rt_name = resourcetype.name();
                                if (rt_name.find("resourcetype") != std::string::npos) {
                                    for (auto collection : resourcetype.children()) {
                                        std::string coll_name = collection.name();
                                        if (coll_name.find("collection") != std::string::npos) {
                                            file.is_collection = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Extract and decode filename
            std::string decoded_href = url_decode(file.href);
            file.name = extract_filename(decoded_href);

            // Filter if needed
            if (file.is_collection) {
                continue; // Skip directories
            }

            if (filter_zips && !ends_with_ignore_case(file.name, ".zip")) {
                continue; // Skip non-zip files
            }

            result.files.push_back(file);
        }

        result.success = true;

    } catch (const std::exception& e) {
        result.error_message = "Exception during PROPFIND: " + std::string(e.what());
    } catch (...) {
        result.error_message = "Unknown exception during PROPFIND";
    }

    return result;
}

WebDavDownloadResult WebDavClient::download_file(const std::string& href, const std::string& local_dir) {
    WebDavDownloadResult result;
    result.success = false;

    try {
        // Create HTTP client
        duckdb_httplib::Client client(proto_host_port_.c_str());

        // Configure SSL and timeouts
        client.enable_server_certificate_verification(false);
        client.set_connection_timeout(30, 0);
        client.set_read_timeout(60, 0); // Longer timeout for downloads
        client.set_write_timeout(30, 0);

        // Prepare headers
        duckdb_httplib::Headers headers = {
            {"Authorization", make_auth_header()}
        };

        // Send GET request
        auto res = client.Get(href.c_str(), headers);

        // Check for errors
        if (!res) {
            result.error_message = "Connection failed while downloading " + href;
            return result;
        }

        if (res->status == 401) {
            result.error_message = "Authentication failed while downloading " + href;
            return result;
        }

        if (res->status == 404) {
            result.error_message = "File not found: " + href;
            return result;
        }

        if (res->status != 200) {
            std::ostringstream oss;
            oss << "Download failed with status " << res->status << " for " << href;
            result.error_message = oss.str();
            return result;
        }

        // Extract filename and construct local path
        std::string decoded_href = url_decode(href);
        std::string filename = extract_filename(decoded_href);

        std::string local_path = local_dir;
        if (!local_path.empty() && local_path.back() != '/' && local_path.back() != '\\') {
            local_path += '/';
        }
        local_path += filename;

        // Write to file
        std::ofstream outfile(local_path, std::ios::binary);
        if (!outfile) {
            result.error_message = "Failed to open local file for writing: " + local_path;
            return result;
        }

        outfile.write(res->body.c_str(), res->body.size());
        outfile.close();

        if (!outfile) {
            result.error_message = "Failed to write downloaded data to: " + local_path;
            return result;
        }

        result.success = true;
        result.local_path = local_path;

    } catch (const std::exception& e) {
        result.error_message = "Exception during download: " + std::string(e.what());
    } catch (...) {
        result.error_message = "Unknown exception during download";
    }

    return result;
}

std::string create_temp_download_dir() {
    std::string temp_base;

#ifdef _WIN32
    // Get Windows temp directory
    wchar_t temp_path[MAX_PATH];
    DWORD result = GetTempPathW(MAX_PATH, temp_path);
    if (result == 0 || result > MAX_PATH) {
        return ""; // Failed to get temp path
    }

    // Convert wide string to narrow string
    char narrow_path[MAX_PATH];
    size_t converted;
    wcstombs_s(&converted, narrow_path, MAX_PATH, temp_path, MAX_PATH - 1);
    temp_base = narrow_path;
#else
    // Unix-like systems
    const char* temp = getenv("TMPDIR");
    if (!temp) temp = getenv("TEMP");
    if (!temp) temp = getenv("TMP");
    if (!temp) temp = "/tmp";
    temp_base = temp;
#endif

    // Ensure temp_base ends with separator
    if (!temp_base.empty() && temp_base.back() != '/' && temp_base.back() != '\\') {
#ifdef _WIN32
        temp_base += '\\';
#else
        temp_base += '/';
#endif
    }

    // Create unique subdirectory name
    std::ostringstream oss;
    oss << "gdpdu_webdav_" << std::time(nullptr) << "_" << (rand() % 10000);
    std::string dir_name = oss.str();

    std::string full_path = temp_base + dir_name;

    // Create directory
    int mkdir_result = mkdir(full_path.c_str(), 0755);
    if (mkdir_result != 0) {
        return ""; // Failed to create directory
    }

    return full_path;
}

void cleanup_temp_dir(const std::string& dir_path) {
    if (dir_path.empty()) return;

#ifdef _WIN32
    // Windows: use system command to remove directory recursively
    std::string command = "rmdir /S /Q \"" + dir_path + "\"";
    system(command.c_str());
#else
    // Unix: use system command
    std::string command = "rm -rf \"" + dir_path + "\"";
    system(command.c_str());
#endif
}

} // namespace duckdb
