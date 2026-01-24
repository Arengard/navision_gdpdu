#include "folder_importer.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <sys/stat.h>
#include <dirent.h>

namespace duckdb {

// Convert string to snake_case (reuse logic from parser)
static std::string to_snake_case(const std::string& input) {
    if (input.empty()) {
        return input;
    }
    
    std::string result;
    result.reserve(input.size() * 2);
    
    bool prev_was_lower = false;
    bool prev_was_upper = false;
    bool prev_was_underscore = true;
    
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        
        // Handle German umlauts (UTF-8 encoded: 2 bytes each)
        if (c == 0xC3 && i + 1 < input.size()) {
            unsigned char next = static_cast<unsigned char>(input[i + 1]);
            char replacement = 0;
            bool is_upper = false;
            
            switch (next) {
                case 0xA4: replacement = 'a'; break;  // ä
                case 0xB6: replacement = 'o'; break;  // ö
                case 0xBC: replacement = 'u'; break;  // ü
                case 0x84: replacement = 'a'; is_upper = true; break;  // Ä
                case 0x96: replacement = 'o'; is_upper = true; break;  // Ö
                case 0x9C: replacement = 'u'; is_upper = true; break;  // Ü
                case 0x9F:  // ß -> ss
                    result += "ss";
                    i++;
                    prev_was_lower = true;
                    prev_was_upper = false;
                    prev_was_underscore = false;
                    continue;
                default:
                    break;
            }
            
            if (replacement != 0) {
                if (is_upper && prev_was_lower && !prev_was_underscore) {
                    result += '_';
                }
                result += replacement;
                i++;
                prev_was_lower = !is_upper;
                prev_was_upper = is_upper;
                prev_was_underscore = false;
                continue;
            }
        }
        
        if (std::isupper(c)) {
            if (prev_was_lower && !prev_was_underscore) {
                result += '_';
            } else if (prev_was_upper && !prev_was_underscore && i + 1 < input.size()) {
                unsigned char next_c = static_cast<unsigned char>(input[i + 1]);
                if (std::islower(next_c)) {
                    result += '_';
                }
            }
            result += static_cast<char>(std::tolower(c));
            prev_was_lower = false;
            prev_was_upper = true;
            prev_was_underscore = false;
        } else if (std::islower(c) || std::isdigit(c)) {
            result += c;
            prev_was_lower = std::islower(c) != 0;
            prev_was_upper = false;
            prev_was_underscore = false;
        } else {
            if (!prev_was_underscore && !result.empty()) {
                result += '_';
                prev_was_underscore = true;
            }
            prev_was_lower = false;
            prev_was_upper = false;
        }
    }
    
    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    
    return result;
}

// Normalize filename to table name (remove extension, convert to snake_case)
static std::string normalize_filename_to_table_name(const std::string& filename) {
    // Remove extension
    size_t last_dot = filename.find_last_of('.');
    std::string name_without_ext = (last_dot != std::string::npos) 
        ? filename.substr(0, last_dot) 
        : filename;
    
    // Convert to snake_case
    return to_snake_case(name_without_ext);
}

// Escape SQL string
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

// Normalize path
static std::string normalize_path(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    while (!result.empty() && result.back() == '/') {
        result.pop_back();
    }
    return result;
}

// Join paths
static std::string join_path(const std::string& dir, const std::string& file) {
    std::string norm_dir = normalize_path(dir);
    if (norm_dir.empty()) {
        return file;
    }
    return norm_dir + "/" + file;
}

// Get file extension (lowercase)
static std::string get_file_extension(const std::string& filename) {
    size_t last_dot = filename.find_last_of('.');
    if (last_dot == std::string::npos) {
        return "";
    }
    std::string ext = filename.substr(last_dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

// Check if file matches the requested type
static bool matches_file_type(const std::string& filename, const std::string& file_type) {
    std::string ext = get_file_extension(filename);
    std::string type_lower = file_type;
    std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(), ::tolower);
    
    if (type_lower == "csv") {
        return ext == "csv" || ext == "txt";
    } else if (type_lower == "parquet") {
        return ext == "parquet";
    } else if (type_lower == "xlsx" || type_lower == "excel") {
        return ext == "xlsx" || ext == "xls";
    } else if (type_lower == "json") {
        return ext == "json" || ext == "jsonl";
    } else if (type_lower == "tsv") {
        return ext == "tsv";
    }
    
    // Default: match extension exactly
    return ext == type_lower;
}

// Get DuckDB read function name for file type
static std::string get_read_function(const std::string& file_type) {
    std::string type_lower = file_type;
    std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(), ::tolower);
    
    if (type_lower == "csv" || type_lower == "txt") {
        return "read_csv";
    } else if (type_lower == "parquet") {
        return "read_parquet";
    } else if (type_lower == "xlsx" || type_lower == "excel") {
        return "read_xlsx";
    } else if (type_lower == "json" || type_lower == "jsonl") {
        return "read_json";
    } else if (type_lower == "tsv") {
        return "read_csv";  // TSV is CSV with tab delimiter
    }
    
    // Default to read_csv
    return "read_csv";
}

// Get read function options based on file type
static std::string get_read_options(const std::string& file_type) {
    std::string type_lower = file_type;
    std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(), ::tolower);
    
    std::ostringstream opts;
    
    if (type_lower == "csv" || type_lower == "txt") {
        opts << "auto_detect=true, header=true";
    } else if (type_lower == "tsv") {
        opts << "auto_detect=true, header=true, delim='\t'";
    } else if (type_lower == "parquet") {
        // read_parquet doesn't need options, it auto-detects schema
        opts.str("");  // Clear options for Parquet
    } else if (type_lower == "xlsx" || type_lower == "excel") {
        // read_xlsx doesn't need options, it auto-detects
        opts.str("");  // Clear options for Excel
    } else if (type_lower == "json" || type_lower == "jsonl") {
        opts << "auto_detect=true";
    } else {
        opts << "auto_detect=true, header=true";
    }
    
    return opts.str();
}

// Get list of files in directory matching the file type
static std::vector<std::string> get_matching_files(const std::string& folder_path, const std::string& file_type) {
    std::vector<std::string> files;
    
    DIR* dir = opendir(folder_path.c_str());
    if (!dir) {
        return files;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        
        // Skip hidden files and directories
        if (filename[0] == '.' || filename == ".." || filename == ".") {
            continue;
        }
        
        // Check if it's a regular file
        std::string full_path = join_path(folder_path, filename);
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            if (matches_file_type(filename, file_type)) {
                files.push_back(filename);
            }
        }
    }
    
    closedir(dir);
    return files;
}

// Get column names from a read function result
static std::vector<std::string> get_column_names(Connection& conn, const std::string& read_sql) {
    std::vector<std::string> columns;
    
    // Execute the read function with LIMIT 0 to get schema without data
    auto result = conn.Query(read_sql + " LIMIT 0");
    
    if (!result->HasError()) {
        for (idx_t i = 0; i < result->ColumnCount(); ++i) {
            columns.push_back(result->ColumnName(i));
        }
    }
    
    return columns;
}

// Clean and trim all VARCHAR columns in a table
static void clean_and_trim_columns(Connection& conn, const std::string& table_name) {
    // Get column information
    auto desc_result = conn.Query("DESCRIBE \"" + table_name + "\"");
    if (desc_result->HasError()) {
        return;
    }
    
    std::vector<std::string> varchar_columns;
    for (idx_t i = 0; i < desc_result->RowCount(); ++i) {
        std::string col_name = desc_result->GetValue(0, i).GetValue<std::string>();
        std::string col_type = desc_result->GetValue(1, i).GetValue<std::string>();
        
        // Check if it's a VARCHAR type
        if (col_type.find("VARCHAR") != std::string::npos || 
            col_type.find("TEXT") != std::string::npos ||
            col_type.find("CHAR") != std::string::npos) {
            varchar_columns.push_back(col_name);
        }
    }
    
    if (varchar_columns.empty()) {
        return;
    }
    
    // Build UPDATE statement to clean and trim all VARCHAR columns
    std::ostringstream update_sql;
    update_sql << "UPDATE \"" << table_name << "\" SET ";
    
    for (size_t i = 0; i < varchar_columns.size(); ++i) {
        if (i > 0) update_sql << ", ";
        std::string col = escape_sql(varchar_columns[i]);
        // Remove control characters and trim whitespace
        update_sql << "\"" << col << "\" = TRIM(REGEXP_REPLACE(\"" << col << "\", '[\\x00-\\x1F\\x7F-\\x9F]', ''))";
    }
    
    try {
        conn.Query(update_sql.str());
    } catch (const std::exception& e) {
        // Ignore errors in cleaning - data is already imported
    }
}

// Infer and convert column types based on data
static void infer_and_convert_types(Connection& conn, const std::string& table_name) {
    // Get column information
    auto desc_result = conn.Query("DESCRIBE \"" + table_name + "\"");
    if (desc_result->HasError()) {
        return;
    }
    
    std::vector<std::pair<std::string, std::string>> varchar_columns;
    for (idx_t i = 0; i < desc_result->RowCount(); ++i) {
        std::string col_name = desc_result->GetValue(0, i).GetValue<std::string>();
        std::string col_type = desc_result->GetValue(1, i).GetValue<std::string>();
        
        // Only process VARCHAR columns
        if (col_type.find("VARCHAR") != std::string::npos || 
            col_type.find("TEXT") != std::string::npos ||
            col_type.find("CHAR") != std::string::npos) {
            varchar_columns.push_back({col_name, col_type});
        }
    }
    
    if (varchar_columns.empty()) {
        return;
    }
    
    // For each VARCHAR column, try to infer type
    for (const auto& col_pair : varchar_columns) {
        std::string col_name = escape_sql(col_pair.first);
        
        // Check if column can be converted to different types
        // Try INTEGER first (most restrictive)
        std::ostringstream check_int;
        check_int << "SELECT COUNT(*) FROM \"" << table_name << "\" WHERE \"";
        check_int << col_name << "\" IS NOT NULL AND \"";
        check_int << col_name << "\" != '' AND TRY_CAST(\"" << col_name << "\" AS BIGINT) IS NULL";
        
        auto int_check = conn.Query(check_int.str());
        bool can_be_int = false;
        if (!int_check->HasError() && int_check->RowCount() > 0) {
            int64_t non_int_count = int_check->GetValue(0, 0).GetValue<int64_t>();
            // If all non-null values can be cast to integer, it's an integer
            std::ostringstream count_non_null;
            count_non_null << "SELECT COUNT(*) FROM \"" << table_name << "\" WHERE \"";
            count_non_null << col_name << "\" IS NOT NULL AND \"" << col_name << "\" != ''";
            auto non_null_result = conn.Query(count_non_null.str());
            if (!non_null_result->HasError() && non_null_result->RowCount() > 0) {
                int64_t non_null_count = non_null_result->GetValue(0, 0).GetValue<int64_t>();
                if (non_null_count > 0 && non_int_count == 0) {
                    can_be_int = true;
                }
            }
        }
        
        if (can_be_int) {
            // Convert to BIGINT
            std::ostringstream alter_sql;
            alter_sql << "ALTER TABLE \"" << table_name << "\" ALTER COLUMN \"";
            alter_sql << col_name << "\" TYPE BIGINT USING TRY_CAST(\"" << col_name << "\" AS BIGINT)";
            try {
                conn.Query(alter_sql.str());
                continue; // Move to next column
            } catch (const std::exception& e) {
                // If conversion fails, try other types
            }
        }
        
        // Try DECIMAL (handle German format with comma)
        std::ostringstream check_decimal;
        check_decimal << "SELECT COUNT(*) FROM \"" << table_name << "\" WHERE \"";
        check_decimal << col_name << "\" IS NOT NULL AND \"";
        check_decimal << col_name << "\" != '' AND TRY_CAST(REPLACE(REPLACE(\"";
        check_decimal << col_name << "\", '.', ''), ',', '.') AS DOUBLE) IS NULL";
        
        auto decimal_check = conn.Query(check_decimal.str());
        bool can_be_decimal = false;
        if (!decimal_check->HasError() && decimal_check->RowCount() > 0) {
            int64_t non_decimal_count = decimal_check->GetValue(0, 0).GetValue<int64_t>();
            std::ostringstream count_non_null;
            count_non_null << "SELECT COUNT(*) FROM \"" << table_name << "\" WHERE \"";
            count_non_null << col_name << "\" IS NOT NULL AND \"" << col_name << "\" != ''";
            auto non_null_result = conn.Query(count_non_null.str());
            if (!non_null_result->HasError() && non_null_result->RowCount() > 0) {
                int64_t non_null_count = non_null_result->GetValue(0, 0).GetValue<int64_t>();
                if (non_null_count > 0 && non_decimal_count == 0) {
                    can_be_decimal = true;
                }
            }
        }
        
        if (can_be_decimal) {
            // Convert to DOUBLE (we'll use DOUBLE for flexibility)
            std::ostringstream alter_sql;
            alter_sql << "ALTER TABLE \"" << table_name << "\" ALTER COLUMN \"";
            alter_sql << col_name << "\" TYPE DOUBLE USING TRY_CAST(REPLACE(REPLACE(\"";
            alter_sql << col_name << "\", '.', ''), ',', '.') AS DOUBLE)";
            try {
                conn.Query(alter_sql.str());
                continue;
            } catch (const std::exception& e) {
                // If conversion fails, keep as VARCHAR
            }
        }
        
        // Try DATE (German format DD.MM.YYYY)
        std::ostringstream check_date;
        check_date << "SELECT COUNT(*) FROM \"" << table_name << "\" WHERE \"";
        check_date << col_name << "\" IS NOT NULL AND \"";
        check_date << col_name << "\" != '' AND TRY_CAST(strptime(\"";
        check_date << col_name << "\", '%d.%m.%Y') AS DATE) IS NULL";
        
        auto date_check = conn.Query(check_date.str());
        bool can_be_date = false;
        if (!date_check->HasError() && date_check->RowCount() > 0) {
            int64_t non_date_count = date_check->GetValue(0, 0).GetValue<int64_t>();
            std::ostringstream count_non_null;
            count_non_null << "SELECT COUNT(*) FROM \"" << table_name << "\" WHERE \"";
            count_non_null << col_name << "\" IS NOT NULL AND \"" << col_name << "\" != ''";
            auto non_null_result = conn.Query(count_non_null.str());
            if (!non_null_result->HasError() && non_null_result->RowCount() > 0) {
                int64_t non_null_count = non_null_result->GetValue(0, 0).GetValue<int64_t>();
                if (non_null_count > 0 && non_date_count == 0) {
                    can_be_date = true;
                }
            }
        }
        
        if (can_be_date) {
            // Convert to DATE
            std::ostringstream alter_sql;
            alter_sql << "ALTER TABLE \"" << table_name << "\" ALTER COLUMN \"";
            alter_sql << col_name << "\" TYPE DATE USING TRY_CAST(strptime(\"";
            alter_sql << col_name << "\", '%d.%m.%Y') AS DATE)";
            try {
                conn.Query(alter_sql.str());
                continue;
            } catch (const std::exception& e) {
                // If conversion fails, keep as VARCHAR
            }
        }
        
        // Try DATE (ISO format YYYY-MM-DD)
        std::ostringstream check_date_iso;
        check_date_iso << "SELECT COUNT(*) FROM \"" << table_name << "\" WHERE \"";
        check_date_iso << col_name << "\" IS NOT NULL AND \"";
        check_date_iso << col_name << "\" != '' AND TRY_CAST(\"";
        check_date_iso << col_name << "\" AS DATE) IS NULL";
        
        auto date_iso_check = conn.Query(check_date_iso.str());
        bool can_be_date_iso = false;
        if (!date_iso_check->HasError() && date_iso_check->RowCount() > 0) {
            int64_t non_date_count = date_iso_check->GetValue(0, 0).GetValue<int64_t>();
            std::ostringstream count_non_null;
            count_non_null << "SELECT COUNT(*) FROM \"" << table_name << "\" WHERE \"";
            count_non_null << col_name << "\" IS NOT NULL AND \"" << col_name << "\" != ''";
            auto non_null_result = conn.Query(count_non_null.str());
            if (!non_null_result->HasError() && non_null_result->RowCount() > 0) {
                int64_t non_null_count = non_null_result->GetValue(0, 0).GetValue<int64_t>();
                if (non_null_count > 0 && non_date_count == 0) {
                    can_be_date_iso = true;
                }
            }
        }
        
        if (can_be_date_iso) {
            // Convert to DATE
            std::ostringstream alter_sql;
            alter_sql << "ALTER TABLE \"" << table_name << "\" ALTER COLUMN \"";
            alter_sql << col_name << "\" TYPE DATE USING TRY_CAST(\"" << col_name << "\" AS DATE)";
            try {
                conn.Query(alter_sql.str());
                continue;
            } catch (const std::exception& e) {
                // If conversion fails, keep as VARCHAR
            }
        }
        
        // If none of the above, keep as VARCHAR
    }
}

std::vector<FileImportResult> import_folder(
    Connection& conn,
    const std::string& folder_path,
    const std::string& file_type) {
    
    std::vector<FileImportResult> results;
    
    // Normalize folder path
    std::string norm_folder = normalize_path(folder_path);
    
    // Get list of matching files
    std::vector<std::string> files = get_matching_files(norm_folder, file_type);
    
    if (files.empty()) {
        FileImportResult r;
        r.table_name = "(no files)";
        r.file_name = "";
        r.row_count = 0;
        r.column_count = 0;
        r.status = "No matching files found for type: " + file_type;
        results.push_back(r);
        return results;
    }
    
    std::string read_func = get_read_function(file_type);
    
    // Import each file
    for (const auto& filename : files) {
        FileImportResult result;
        result.file_name = filename;
        result.table_name = normalize_filename_to_table_name(filename);
        
        std::string file_path = join_path(norm_folder, filename);
        std::string read_opts = get_read_options(file_type);
        
        try {
            // Drop existing table if it exists
            std::string drop_sql = "DROP TABLE IF EXISTS \"" + result.table_name + "\"";
            conn.Query(drop_sql);
            
            // For Excel/Parquet/JSON files, read directly without encoding detection
            // For CSV/TXT/TSV files, try different encodings
            bool success = false;
            std::string final_read_query;
            std::vector<std::string> orig_cols;
            
            std::string type_lower = file_type;
            std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(), ::tolower);
            
            if (type_lower == "xlsx" || type_lower == "excel" || 
                type_lower == "parquet" || type_lower == "json" || type_lower == "jsonl") {
                // For these file types, read directly without encoding or extra options
                std::ostringstream read_query;
                read_query << read_func << "('" << escape_sql(file_path) << "')";
                
                // Try to get column names
                std::string test_query = "SELECT * FROM " + read_query.str() + " LIMIT 0";
                auto test_result = conn.Query(test_query);
                
                if (!test_result->HasError()) {
                    // Success! Get column names
                    for (idx_t i = 0; i < test_result->ColumnCount(); ++i) {
                        orig_cols.push_back(test_result->ColumnName(i));
                    }
                    final_read_query = read_query.str();
                    success = true;
                } else {
                    result.row_count = 0;
                    result.column_count = 0;
                    result.status = "Load failed: " + test_result->GetError();
                    results.push_back(result);
                    continue;
                }
            } else {
                // For CSV/TXT/TSV, try different encodings
                std::vector<std::string> encodings_to_try = {
                    "UTF-8",                    // Most common modern encoding
                    "ISO-8859-1",              // Latin-1, common for German
                    "Windows-1252",            // Windows Western European
                    "CP1252",                   // Windows-1252 alias
                    "ISO_8859_1",              // ISO-8859-1 alias
                    "8859_1",                  // ISO-8859-1 alias
                    "latin-1",                 // ISO-8859-1 alias
                    "ISO8859_1",               // ISO-8859-1 alias
                    "windows-1252-2000",       // Windows-1252 variant
                    "CP1250",                  // Windows Central European
                    "ISO-8859-15",             // Latin-9
                    "ISO_8859_15",             // ISO-8859-15 alias
                    "8859_15",                 // ISO-8859-15 alias
                    "ISO8859_15",              // ISO-8859-15 alias
                    "Windows-1250",            // Windows Central European
                    "windows-1250-2000",       // Windows-1250 variant
                    "CP850",                   // DOS Western European
                    "IBM_850",                 // CP850 alias
                    "cp850",                   // CP850 lowercase
                    "CP437",                   // DOS US
                    "cp437",                   // CP437 lowercase
                    "UTF-16",                  // UTF-16 (less common for CSV)
                    "utf-16"                   // UTF-16 lowercase
                };
                
                for (const auto& enc : encodings_to_try) {
                    std::ostringstream read_query;
                    read_query << read_func << "('" << escape_sql(file_path) << "', " << read_opts;
                    read_query << ", encoding='" << enc << "')";
                    
                    // Try to get column names with this encoding
                    std::string test_query = "SELECT * FROM " + read_query.str() + " LIMIT 0";
                    auto test_result = conn.Query(test_query);
                    
                    if (!test_result->HasError()) {
                        // Success! Get column names
                        for (idx_t i = 0; i < test_result->ColumnCount(); ++i) {
                            orig_cols.push_back(test_result->ColumnName(i));
                        }
                        final_read_query = read_query.str();
                        success = true;
                        break;
                    }
                    
                    // Check if it's an encoding error
                    std::string error = test_result->GetError();
                    if (error.find("unicode") == std::string::npos && 
                        error.find("encoding") == std::string::npos &&
                        error.find("utf-8") == std::string::npos) {
                        // Not an encoding error, break and report
                        break;
                    }
                }
                
                // If all encodings failed, try with ignore_errors as last resort
                if (!success) {
                    std::vector<std::string> fallback_encodings = {
                        "ISO-8859-1", "Windows-1252", "CP1252", "UTF-8", "CP850"
                    };
                    
                    for (const auto& enc : fallback_encodings) {
                        std::ostringstream read_query;
                        read_query << read_func << "('" << escape_sql(file_path) << "', " << read_opts;
                        read_query << ", encoding='" << enc << "', ignore_errors=true)";
                        
                        // Try to get columns even with errors
                        std::string test_query = "SELECT * FROM " + read_query.str() + " LIMIT 0";
                        auto test_result = conn.Query(test_query);
                        if (!test_result->HasError()) {
                            for (idx_t i = 0; i < test_result->ColumnCount(); ++i) {
                                orig_cols.push_back(test_result->ColumnName(i));
                            }
                            final_read_query = read_query.str();
                            success = true;
                            break;
                        }
                    }
                }
                
                if (!success) {
                    result.row_count = 0;
                    result.column_count = 0;
                    result.status = "Load failed: Could not read file with any encoding";
                    results.push_back(result);
                    continue;
                }
            }
            
            // Build CREATE TABLE AS SELECT with normalized column names
            std::ostringstream sql;
            sql << "CREATE TABLE \"" << result.table_name << "\" AS ";
            sql << "SELECT ";
            
            if (!orig_cols.empty()) {
                // Use column aliases to normalize names
                for (size_t i = 0; i < orig_cols.size(); ++i) {
                    if (i > 0) sql << ", ";
                    std::string normalized_name = to_snake_case(orig_cols[i]);
                    sql << "\"" << escape_sql(orig_cols[i]) << "\" AS \"" << escape_sql(normalized_name) << "\"";
                }
            } else {
                sql << "*";
            }
            
            sql << " FROM " << final_read_query;
            
            auto query_result = conn.Query(sql.str());
            
            if (query_result->HasError()) {
                result.row_count = 0;
                result.column_count = 0;
                result.status = "Load failed: " + query_result->GetError();
            } else {
                // Clean, trim, and infer types for all columns
                clean_and_trim_columns(conn, result.table_name);
                infer_and_convert_types(conn, result.table_name);
                
                // Get row and column counts
                auto count_result = conn.Query("SELECT COUNT(*) FROM \"" + result.table_name + "\"");
                if (!count_result->HasError() && count_result->RowCount() > 0) {
                    result.row_count = count_result->GetValue(0, 0).GetValue<int64_t>();
                }
                
                auto desc_result = conn.Query("DESCRIBE \"" + result.table_name + "\"");
                if (!desc_result->HasError()) {
                    result.column_count = static_cast<int>(desc_result->RowCount());
                } else {
                    result.column_count = static_cast<int>(orig_cols.size());
                }
                
                result.status = "OK";
            }
        } catch (const std::exception& e) {
            result.row_count = 0;
            result.column_count = 0;
            result.status = std::string("Load failed: ") + e.what();
        }
        
        results.push_back(result);
    }
    
    return results;
}

} // namespace duckdb
