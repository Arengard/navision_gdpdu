#include "gdpdu_exporter.hpp"
#include "gdpdu_schema.hpp"
#include <pugixml.hpp>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <cstdlib>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir _mkdir
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace duckdb {

// Helper to escape SQL strings
static std::string escape_sql(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 10);
    for (char c : value) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    return result;
}

// Helper to normalize paths
static std::string normalize_path(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    while (!result.empty() && result.back() == '/') {
        result.pop_back();
    }
    return result;
}

// Helper to join paths
static std::string join_path(const std::string& dir, const std::string& file) {
    std::string norm_dir = normalize_path(dir);
    if (norm_dir.empty()) {
        return file;
    }
    return norm_dir + "/" + file;
}

// Helper to create directory recursively if it doesn't exist
static bool ensure_directory_exists(const std::string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
        return true;  // Directory exists
    }
    
    // Normalize path
    std::string norm_path = normalize_path(path);
    if (norm_path.empty()) {
        return false;
    }
    
    // Split path into components and create each directory
    std::string current_path;
    bool is_absolute = false;
    
    #ifdef _WIN32
        // On Windows, check for drive letter (C:, D:, etc.)
        if (norm_path.length() >= 2 && norm_path[1] == ':') {
            is_absolute = true;
            current_path = norm_path.substr(0, 2);  // "C:"
            if (norm_path.length() > 2 && norm_path[2] == '/') {
                current_path += "/";
            }
        } else if (norm_path[0] == '/') {
            is_absolute = true;
        }
    #else
        is_absolute = (norm_path[0] == '/');
    #endif
    
    size_t pos = 0;
    #ifdef _WIN32
        // Skip drive letter and initial slash on Windows
        if (norm_path.length() >= 2 && norm_path[1] == ':') {
            pos = (norm_path.length() > 2 && norm_path[2] == '/') ? 3 : 2;
        } else if (norm_path[0] == '/') {
            pos = 1;
        }
    #else
        if (is_absolute) {
            pos = 1;  // Skip leading slash
        }
    #endif
    
    while (pos < norm_path.length()) {
        size_t next_slash = norm_path.find('/', pos);
        if (next_slash == std::string::npos) {
            next_slash = norm_path.length();
        }
        
        std::string component = norm_path.substr(pos, next_slash - pos);
        if (!component.empty()) {
            // Build current path
            if (current_path.empty()) {
                current_path = component;
            } else {
                #ifdef _WIN32
                    current_path += "\\" + component;
                #else
                    current_path += "/" + component;
                #endif
            }
            
            // Check if this directory exists
            struct stat dir_info;
            if (stat(current_path.c_str(), &dir_info) != 0 || !S_ISDIR(dir_info.st_mode)) {
                // Directory doesn't exist, create it
                #ifdef _WIN32
                    if (_mkdir(current_path.c_str()) != 0 && errno != EEXIST) {
                        return false;
                    }
                #else
                    if (mkdir(current_path.c_str(), 0755) != 0 && errno != EEXIST) {
                        return false;
                    }
                #endif
            }
        }
        
        pos = next_slash + 1;
    }
    
    return true;
}

// Convert DuckDB type to GDPdU type
static GdpduType duckdb_type_to_gdpdu_type(const std::string& duckdb_type, int& precision) {
    precision = 0;
    
    if (duckdb_type.find("VARCHAR") != std::string::npos || 
        duckdb_type.find("TEXT") != std::string::npos ||
        duckdb_type.find("CHAR") != std::string::npos) {
        return GdpduType::AlphaNumeric;
    } else if (duckdb_type.find("DATE") != std::string::npos) {
        return GdpduType::Date;
    } else if (duckdb_type.find("DECIMAL") != std::string::npos || 
               duckdb_type.find("NUMERIC") != std::string::npos) {
        // Extract precision from DECIMAL(18,2) format
        size_t open_paren = duckdb_type.find('(');
        if (open_paren != std::string::npos) {
            size_t comma = duckdb_type.find(',', open_paren);
            if (comma != std::string::npos) {
                std::string prec_str = duckdb_type.substr(comma + 1);
                size_t close_paren = prec_str.find(')');
                if (close_paren != std::string::npos) {
                    prec_str = prec_str.substr(0, close_paren);
                    try {
                        precision = std::stoi(prec_str);
                    } catch (...) {
                        precision = 2;  // Default
                    }
                }
            }
        }
        return GdpduType::Numeric;
    } else if (duckdb_type.find("BIGINT") != std::string::npos ||
               duckdb_type.find("INTEGER") != std::string::npos ||
               duckdb_type.find("INT") != std::string::npos) {
        precision = 0;  // Integer
        return GdpduType::Numeric;
    } else if (duckdb_type.find("DOUBLE") != std::string::npos ||
               duckdb_type.find("FLOAT") != std::string::npos ||
               duckdb_type.find("REAL") != std::string::npos) {
        precision = 2;  // Default for floating point
        return GdpduType::Numeric;
    }
    
    // Default to AlphaNumeric
    return GdpduType::AlphaNumeric;
}

// Convert GDPdU type to XML string
static std::string gdpdu_type_to_xml_string(GdpduType type) {
    switch (type) {
        case GdpduType::AlphaNumeric:
            return "AlphaNumeric";
        case GdpduType::Numeric:
            return "Numeric";
        case GdpduType::Date:
            return "Date";
        default:
            return "AlphaNumeric";
    }
}

// Convert snake_case back to PascalCase for GDPdU export
static std::string to_pascal_case(const std::string& input) {
    if (input.empty()) {
        return input;
    }
    
    std::string result;
    result.reserve(input.size());
    bool capitalize_next = true;
    
    for (char c : input) {
        if (c == '_') {
            capitalize_next = true;
        } else {
            if (capitalize_next) {
                result += static_cast<char>(std::toupper(c));
                capitalize_next = false;
            } else {
                result += c;
            }
        }
    }
    
    return result;
}

// Convert path to absolute path
static std::string to_absolute_path(const std::string& path) {
    if (path.empty()) {
        return path;
    }
    
    std::string norm_path = normalize_path(path);
    
    // If already absolute (starts with / on Unix or has drive letter on Windows), return as is
    #ifdef _WIN32
        if (norm_path.length() >= 2 && norm_path[1] == ':') {
            return norm_path;
        }
    #else
        if (norm_path[0] == '/') {
            return norm_path;
        }
    #endif
    
    // Handle paths that look like they should be absolute (e.g., "Users/..." on macOS)
    // If path starts with "Users/" or "home/", assume it should be absolute
    #ifndef _WIN32
        // Check before normalization to catch paths like "Users/ramonljevo/..."
        std::string original_lower = path;
        std::transform(original_lower.begin(), original_lower.end(), original_lower.begin(), ::tolower);
        if (norm_path.find("Users/") == 0 || norm_path.find("users/") == 0 || 
            norm_path.find("home/") == 0 || norm_path.find("Home/") == 0) {
            return "/" + norm_path;
        }
    #endif
    
    // Relative path - convert to absolute using current working directory
    char cwd[PATH_MAX];
    #ifdef _WIN32
        if (_getcwd(cwd, sizeof(cwd)) != nullptr) {
            std::string abs_path = std::string(cwd);
            if (abs_path.back() != '\\' && abs_path.back() != '/') {
                abs_path += "\\";
            }
            abs_path += norm_path;
            return normalize_path(abs_path);
        }
    #else
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            std::string cwd_str = std::string(cwd);
            // If the relative path starts with a component that matches the last component of cwd,
            // it might be a mistake - but we'll still create the path
            // However, if norm_path already starts with "Users/", we should have caught it above
            std::string abs_path = cwd_str;
            if (abs_path.back() != '/') {
                abs_path += "/";
            }
            abs_path += norm_path;
            std::string result = normalize_path(abs_path);
            // Double-check: if result contains "Users/Users/", something went wrong
            // This can happen if cwd is /Users/ramonljevo/... and path is Users/ramonljevo/...
            size_t double_users = result.find("/Users/Users/");
            if (double_users != std::string::npos) {
                // Remove the duplicate: /Users/ramonljevo/Users/ramonljevo/... -> /Users/ramonljevo/...
                result = result.substr(0, double_users + 7) + result.substr(double_users + 20);
            }
            return result;
        }
    #endif
    
    // Fallback: return normalized original path
    return norm_path;
}

ExportResult export_gdpdu(Connection& conn, const std::string& export_path, const std::string& table_name) {
    ExportResult result;
    result.table_name = table_name;
    
    try {
        // Convert to absolute path and ensure export directory exists
        std::string abs_path = to_absolute_path(export_path);
        result.file_path = abs_path;
        
        // Debug: log the path being used
        // std::cerr << "Export path: " << export_path << " -> " << abs_path << std::endl;
        
        if (!ensure_directory_exists(abs_path)) {
            result.status = "Failed to create export directory: " + abs_path;
            return result;
        }
        
        // Get table schema
        auto desc_result = conn.Query("DESCRIBE \"" + escape_sql(table_name) + "\"");
        if (desc_result->HasError()) {
            result.status = "Failed to describe table: " + desc_result->GetError();
            return result;
        }
        
        if (desc_result->RowCount() == 0) {
            result.status = "Table has no columns";
            return result;
        }
        
        // Get row count
        auto count_result = conn.Query("SELECT COUNT(*) FROM \"" + escape_sql(table_name) + "\"");
        if (!count_result->HasError() && count_result->RowCount() > 0) {
            result.row_count = count_result->GetValue(0, 0).GetValue<int64_t>();
        }
        
        // Build column definitions
        std::vector<ColumnDef> columns;
        std::vector<std::string> primary_key_columns;
        
        for (idx_t i = 0; i < desc_result->RowCount(); ++i) {
            std::string col_name = desc_result->GetValue(0, i).GetValue<std::string>();
            std::string col_type = desc_result->GetValue(1, i).GetValue<std::string>();
            
            ColumnDef col;
            col.name = col_name;  // Keep original name
            int precision = 0;
            col.type = duckdb_type_to_gdpdu_type(col_type, precision);
            col.precision = precision;
            col.is_primary_key = false;  // We don't know PK from schema, assume none
            
            columns.push_back(col);
        }
        
        // Create data file name
        std::string data_file = table_name + ".txt";
        std::string data_path = join_path(abs_path, data_file);
        
        // Export data to CSV with German formatting
        // First, create a temporary view with formatted data
        std::ostringstream create_view_sql;
        create_view_sql << "CREATE OR REPLACE TEMP VIEW export_temp AS SELECT ";
        
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) create_view_sql << ", ";
            std::string col_name = escape_sql(columns[i].name);
            
            if (columns[i].type == GdpduType::Numeric) {
                if (columns[i].precision > 0) {
                    // Decimal: format with comma as decimal separator
                    // DuckDB uses dot for decimal, we need comma
                    // Format: convert to string, replace dot with comma
                    create_view_sql << "REPLACE(CAST(\"" << col_name << "\" AS VARCHAR), '.', ',')";
                    create_view_sql << " AS \"" << col_name << "\"";
                } else {
                    // Integer: keep as is (no formatting needed for integers)
                    create_view_sql << "CAST(\"" << col_name << "\" AS VARCHAR)";
                    create_view_sql << " AS \"" << col_name << "\"";
                }
            } else if (columns[i].type == GdpduType::Date) {
                // Date: format as DD.MM.YYYY
                create_view_sql << "strftime(\"" << col_name << "\", '%d.%m.%Y') AS \"" << col_name << "\"";
            } else {
                // AlphaNumeric: keep as is
                create_view_sql << "\"" << col_name << "\"";
            }
        }
        
        create_view_sql << " FROM \"" << escape_sql(table_name) << "\"";
        
        auto view_result = conn.Query(create_view_sql.str());
        if (view_result->HasError()) {
            result.status = "Failed to create export view: " + view_result->GetError();
            return result;
        }
        
        // Export to CSV with semicolon delimiter, no header
        // Use absolute path for COPY command (DuckDB accepts forward slashes on Windows)
        std::ostringstream copy_sql;
        copy_sql << "COPY export_temp TO '" << escape_sql(data_path) << "' ";
        copy_sql << "(FORMAT CSV, DELIMITER ';', HEADER false, QUOTE '\"')";
        
        auto copy_result = conn.Query(copy_sql.str());
        if (copy_result->HasError()) {
            result.status = "Failed to export data: " + copy_result->GetError() + " (path: " + data_path + ")";
            return result;
        }
        
        // Verify file was created
        struct stat file_info;
        if (stat(data_path.c_str(), &file_info) != 0) {
            result.status = "File was not created: " + data_path;
            return result;
        }
        
        // File is already correctly formatted (decimal comma, no thousands separator for now)
        // Note: Adding thousands separators would require more complex formatting
        
        // Create index.xml
        pugi::xml_document doc;
        
        // Create DataSet root
        pugi::xml_node dataset = doc.append_child("DataSet");
        
        // Create Media element
        pugi::xml_node media = dataset.append_child("Media");
        media.append_child("Name").text().set(table_name.c_str());
        
        // Create Table element
        pugi::xml_node table_node = media.append_child("Table");
        table_node.append_child("Name").text().set(table_name.c_str());
        table_node.append_child("Description").text().set(table_name.c_str());
        table_node.append_child("URL").text().set(data_file.c_str());
        table_node.append_child("UTF8");
        table_node.append_child("DecimalSymbol").text().set(",");
        table_node.append_child("DigitGroupingSymbol").text().set(".");
        
        // Create Range element
        pugi::xml_node range = table_node.append_child("Range");
        range.append_child("From").text().set("1");
        
        // Create VariableLength element
        pugi::xml_node var_length = table_node.append_child("VariableLength");
        
        // Add columns (all as VariableColumn, no primary keys detected)
        for (const auto& col : columns) {
            pugi::xml_node col_node = var_length.append_child("VariableColumn");
            col_node.append_child("Name").text().set(col.name.c_str());
            
            // Add type element
            pugi::xml_node type_node = col_node.append_child(gdpdu_type_to_xml_string(col.type).c_str());
            if (col.type == GdpduType::Numeric && col.precision > 0) {
                type_node.append_child("Accuracy").text().set(std::to_string(col.precision).c_str());
            }
        }
        
        // Save index.xml
        std::string index_path = join_path(abs_path, "index.xml");
        if (!doc.save_file(index_path.c_str())) {
            result.status = "Failed to write index.xml to: " + index_path;
            return result;
        }
        
        // Verify index.xml was created
        struct stat index_info;
        if (stat(index_path.c_str(), &index_info) != 0) {
            result.status = "index.xml was not created: " + index_path;
            return result;
        }
        
        result.status = "OK";
        
    } catch (const std::exception& e) {
        result.status = std::string("Export failed: ") + e.what();
    }
    
    return result;
}

} // namespace duckdb
