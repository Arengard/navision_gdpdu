#include "buchungsstapel_importer.hpp"
#include <sstream>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <fileapi.h>
#else
#include <dirent.h>
#endif

namespace duckdb {

// ============================================================================
// Section 1: Utility functions
// ============================================================================

// Path helper: normalize Windows/Unix paths
static std::string normalize_path(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    while (!result.empty() && result.back() == '/') {
        result.pop_back();
    }
    return result;
}

// Path helper: join directory and filename
static std::string join_path(const std::string& dir, const std::string& file) {
    std::string norm_dir = normalize_path(dir);
    if (norm_dir.empty()) {
        return file;
    }
    return norm_dir + "/" + file;
}

// Escape single quotes for SQL string literals
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

// Convert UTF-8 string to wide string (Windows)
#ifdef _WIN32
static std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    if (size_needed <= 0) return std::wstring();
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size_needed);
    return result;
}

// Convert wide string to UTF-8 string (Windows)
static std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return std::string();
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &result[0], size_needed, NULL, NULL);
    return result;
}
#endif

// Scan folder for files starting with "EXTF_Buchungsstapel" and ending with ".csv"
static std::vector<std::string> get_matching_buchungsstapel_files(const std::string& folder_path) {
    std::vector<std::string> files;

#ifdef _WIN32
    // Windows implementation using wide-char FindFirstFileW/FindNextFileW
    // to properly handle UTF-8 paths with non-ASCII characters (e.g. umlauts)
    std::string search_path = join_path(folder_path, "*");
    std::wstring wide_search = utf8_to_wide(search_path);
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle = FindFirstFileW(wide_search.c_str(), &find_data);

    if (find_handle == INVALID_HANDLE_VALUE) {
        return files;
    }

    do {
        std::string filename = wide_to_utf8(find_data.cFileName);

        // Skip hidden files and directories
        if (filename.empty() || filename[0] == '.' || filename == ".." || filename == ".") {
            continue;
        }

        // Check if it's a regular file (not a directory)
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // Check prefix: starts with "EXTF_Buchungsstapel"
            if (filename.size() >= 19 && filename.substr(0, 19) == "EXTF_Buchungsstapel") {
                // Check suffix: ends with ".csv" (case-insensitive)
                if (filename.size() >= 4) {
                    std::string ext = filename.substr(filename.size() - 4);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".csv") {
                        files.push_back(filename);
                    }
                }
            }
        }
    } while (FindNextFileW(find_handle, &find_data) != 0);

    FindClose(find_handle);
#else
    // Unix/Linux/macOS implementation using opendir/readdir
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
            // Check prefix: starts with "EXTF_Buchungsstapel"
            if (filename.size() >= 19 && filename.substr(0, 19) == "EXTF_Buchungsstapel") {
                // Check suffix: ends with ".csv" (case-insensitive)
                if (filename.size() >= 4) {
                    std::string ext = filename.substr(filename.size() - 4);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".csv") {
                        files.push_back(filename);
                    }
                }
            }
        }
    }

    closedir(dir);
#endif

    return files;
}

// ============================================================================
// Section 2: CSV line parser
// ============================================================================

// Split a semicolon-delimited line respecting quoted fields
static std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (in_quotes) {
            if (c == '"') {
                // Check for escaped quote ("" inside quoted field)
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    current += '"';
                    ++i;  // skip the second quote
                } else {
                    // End of quoted field
                    in_quotes = false;
                }
            } else {
                current += c;
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ';') {
                fields.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
    }

    // Push the last field
    fields.push_back(current);

    return fields;
}

// ============================================================================
// Section 3: Header parser
// ============================================================================

// Strip surrounding quotes from a field
static std::string strip_quotes(const std::string& field) {
    if (field.size() >= 2 && field.front() == '"' && field.back() == '"') {
        return field.substr(1, field.size() - 2);
    }
    return field;
}

// Parse the first line (header/metadata row) to extract the year from Datum-von field
// Returns the 4-digit year as an integer, or -1 on parse failure
static int parse_buchungsstapel_header(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return -1;
    }

    std::string first_line;
    if (!std::getline(file, first_line)) {
        return -1;
    }

    // Split by semicolons
    std::vector<std::string> fields = split_csv_line(first_line);

    // Strip quotes from each field
    for (size_t i = 0; i < fields.size(); ++i) {
        fields[i] = strip_quotes(fields[i]);
    }

    // Field 14 (0-indexed) is Datum-von in YYYYMMDD format
    if (fields.size() <= 14) {
        return -1;
    }

    std::string datum_von = fields[14];
    // Trim whitespace
    while (!datum_von.empty() && (datum_von.front() == ' ' || datum_von.front() == '\t')) {
        datum_von.erase(datum_von.begin());
    }
    while (!datum_von.empty() && (datum_von.back() == ' ' || datum_von.back() == '\t' || datum_von.back() == '\r')) {
        datum_von.pop_back();
    }

    // Extract 4-digit year (first 4 characters of YYYYMMDD)
    if (datum_von.size() < 4) {
        return -1;
    }

    // Verify the first 4 characters are digits
    for (int i = 0; i < 4; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(datum_von[i]))) {
            return -1;
        }
    }

    int year = std::stoi(datum_von.substr(0, 4));
    return year;
}

// ============================================================================
// Section 4: Column name parser
// ============================================================================

// Parse the second line of the CSV file to get column names
static std::vector<std::string> parse_buchungsstapel_columns(const std::string& file_path) {
    std::vector<std::string> columns;

    std::ifstream file(file_path);
    if (!file.is_open()) {
        return columns;
    }

    // Skip the first line (metadata header)
    std::string line;
    if (!std::getline(file, line)) {
        return columns;
    }

    // Read the second line (column names)
    if (!std::getline(file, line)) {
        return columns;
    }

    // Split by semicolons
    std::vector<std::string> fields = split_csv_line(line);

    // Strip surrounding quotes from each column name
    for (size_t i = 0; i < fields.size(); ++i) {
        std::string col = strip_quotes(fields[i]);
        // Trim whitespace and carriage return
        while (!col.empty() && (col.back() == ' ' || col.back() == '\t' || col.back() == '\r')) {
            col.pop_back();
        }
        while (!col.empty() && (col.front() == ' ' || col.front() == '\t')) {
            col.erase(col.begin());
        }
        columns.push_back(col);
    }

    return columns;
}

// ============================================================================
// Section 5: SQL builders
// ============================================================================

// Returns SQL type based on column name
static std::string get_column_sql_type(const std::string& col_name) {
    if (col_name == "Belegdatum") {
        return "DATE";
    }
    if (col_name == "Umsatz (ohne Soll/Haben-Kz)") {
        return "DECIMAL(18,2)";
    }
    if (col_name == "Basis-Umsatz") {
        return "DECIMAL(18,2)";
    }
    if (col_name == "Kurs") {
        return "DECIMAL(18,6)";
    }
    return "VARCHAR";
}

// Returns SQL expression for a column in the SELECT clause
static std::string build_column_select_expr(const std::string& col_name, size_t col_index, int year) {
    std::string col_ref = "column" + std::to_string(col_index);

    if (col_name == "Belegdatum") {
        // Belegdatum has compact DDMM format (e.g. "2001" = 20th Jan, "103" = 1st Mar)
        // Extract month from last 2 chars, day from remaining leading chars
        std::ostringstream ss;
        ss << "CASE WHEN " << col_ref << " IS NOT NULL AND TRIM(" << col_ref << ") != '' THEN "
           << "MAKE_DATE(" << year << ", "
           << "CAST(RIGHT(TRIM(" << col_ref << "), 2) AS INTEGER), "
           << "CAST(LEFT(TRIM(" << col_ref << "), LENGTH(TRIM(" << col_ref << ")) - 2) AS INTEGER)) "
           << "ELSE NULL END";
        return ss.str();
    }

    if (col_name == "Umsatz (ohne Soll/Haben-Kz)" || col_name == "Basis-Umsatz") {
        // German decimal format: dot = thousands separator, comma = decimal separator
        std::ostringstream ss;
        ss << "CASE WHEN " << col_ref << " IS NOT NULL AND TRIM(" << col_ref << ") != '' THEN "
           << "CAST(REPLACE(REPLACE(TRIM(" << col_ref << "), '.', ''), ',', '.') AS DECIMAL(18,2)) "
           << "ELSE NULL END";
        return ss.str();
    }

    if (col_name == "Kurs") {
        // Same German decimal format but with higher precision
        std::ostringstream ss;
        ss << "CASE WHEN " << col_ref << " IS NOT NULL AND TRIM(" << col_ref << ") != '' THEN "
           << "CAST(REPLACE(REPLACE(TRIM(" << col_ref << "), '.', ''), ',', '.') AS DECIMAL(18,6)) "
           << "ELSE NULL END";
        return ss.str();
    }

    // All other columns: pass-through
    return col_ref;
}

// Build CREATE OR REPLACE TABLE statement with typed columns + file_name VARCHAR
static std::string build_create_table_sql(const std::string& table_name, const std::vector<std::string>& columns) {
    std::ostringstream sql;
    sql << "CREATE OR REPLACE TABLE \"" << table_name << "\" (";

    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) sql << ", ";
        sql << "\"" << escape_sql(columns[i]) << "\" " << get_column_sql_type(columns[i]);
    }

    sql << ", \"file_name\" VARCHAR)";
    return sql.str();
}

// Build INSERT INTO ... SELECT ... FROM read_csv(...) statement
static std::string build_insert_sql(const std::string& table_name,
                                     const std::vector<std::string>& columns,
                                     const std::string& file_path,
                                     const std::string& file_name,
                                     int year,
                                     const std::string& encoding,
                                     bool ignore_errors) {
    std::ostringstream sql;

    // INSERT INTO ... (columns..., file_name)
    sql << "INSERT INTO \"" << table_name << "\" (";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) sql << ", ";
        sql << "\"" << escape_sql(columns[i]) << "\"";
    }
    sql << ", \"file_name\") ";

    // SELECT expressions..., filename literal
    sql << "SELECT ";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) sql << ", ";
        sql << build_column_select_expr(columns[i], i, year);
    }
    sql << ", '" << escape_sql(file_name) << "' AS \"file_name\" ";

    // FROM read_csv(...)
    sql << "FROM read_csv('" << escape_sql(file_path) << "', ";
    sql << "skip=2, ";
    sql << "delim=';', ";
    sql << "quote='\"', ";
    sql << "all_varchar=true, ";
    sql << "auto_detect=false, ";
    sql << "strict_mode=false, ";
    sql << "null_padding=true, ";
    sql << "encoding='" << encoding << "', ";
    if (ignore_errors) {
        sql << "ignore_errors=true, ";
    }

    // Column definitions: columns={'column0': 'VARCHAR', 'column1': 'VARCHAR', ...}
    sql << "columns={";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) sql << ", ";
        sql << "'column" << i << "': 'VARCHAR'";
    }
    sql << "})";

    return sql.str();
}

// ============================================================================
// Section 6: Main function
// ============================================================================

std::vector<BuchungsstapelImportResult> import_buchungsstapel(
    Connection& conn,
    const std::string& folder_path) {

    std::vector<BuchungsstapelImportResult> results;

    // Normalize the folder path
    std::string norm_folder = normalize_path(folder_path);

    // Get matching files
    std::vector<std::string> files = get_matching_buchungsstapel_files(norm_folder);

    if (files.empty()) {
        BuchungsstapelImportResult r;
        r.table_name = "(no files)";
        r.file_name = "";
        r.row_count = 0;
        r.status = "No Buchungsstapel files found (expected EXTF_Buchungsstapel*.csv)";
        results.push_back(r);
        return results;
    }

    // Process each file
    for (const auto& filename : files) {
        BuchungsstapelImportResult result;
        result.file_name = filename;

        // Derive table name from filename (remove .csv extension)
        size_t last_dot = filename.find_last_of('.');
        std::string table_name = (last_dot != std::string::npos)
            ? filename.substr(0, last_dot)
            : filename;
        result.table_name = table_name;

        std::string file_path = join_path(norm_folder, filename);

        // Parse header -> get year
        int year = parse_buchungsstapel_header(file_path);
        if (year < 0) {
            result.row_count = 0;
            result.status = "Failed to parse header (could not extract year from Datum-von field)";
            results.push_back(result);
            continue;
        }

        // Parse column names
        std::vector<std::string> columns = parse_buchungsstapel_columns(file_path);
        if (columns.empty()) {
            result.row_count = 0;
            result.status = "Failed to parse column names from row 2";
            results.push_back(result);
            continue;
        }

        // Build and execute CREATE TABLE
        std::string create_sql = build_create_table_sql(table_name, columns);
        try {
            auto create_result = conn.Query(create_sql);
            if (create_result->HasError()) {
                result.row_count = 0;
                result.status = "Create table failed: " + create_result->GetError();
                results.push_back(result);
                continue;
            }
        } catch (const std::exception& e) {
            result.row_count = 0;
            result.status = std::string("Create table failed: ") + e.what();
            results.push_back(result);
            continue;
        }

        // Try encoding fallback loop
        std::vector<std::string> encodings_to_try = {
            "UTF-8",
            "ISO-8859-1",
            "Windows-1252",
            "CP1252",
            "CP850"
        };

        bool success = false;
        std::string load_error;

        // First pass: try each encoding without ignore_errors
        for (const auto& encoding : encodings_to_try) {
            std::string insert_sql = build_insert_sql(table_name, columns, file_path, filename, year, encoding, false);

            try {
                auto query_result = conn.Query(insert_sql);
                if (!query_result->HasError()) {
                    success = true;
                    break;
                }
                // If error contains encoding info, try next encoding
                std::string error = query_result->GetError();
                if (error.find("unicode") != std::string::npos ||
                    error.find("encoding") != std::string::npos ||
                    error.find("utf") != std::string::npos) {
                    continue;  // Try next encoding
                }
                // Other error, stop trying
                load_error = error;
                break;
            } catch (const std::exception& e) {
                std::string error = e.what();
                if (error.find("unicode") != std::string::npos ||
                    error.find("encoding") != std::string::npos ||
                    error.find("utf") != std::string::npos) {
                    continue;  // Try next encoding
                }
                // Other error
                load_error = error;
                break;
            }
        }

        // Second pass: if all failed, retry same encodings with ignore_errors=true
        if (!success) {
            // Clear the table before retrying (it may have partial data from failed attempts)
            conn.Query("DELETE FROM \"" + table_name + "\"");

            for (const auto& encoding : encodings_to_try) {
                std::string insert_sql = build_insert_sql(table_name, columns, file_path, filename, year, encoding, true);

                try {
                    auto query_result = conn.Query(insert_sql);
                    if (!query_result->HasError()) {
                        success = true;
                        break;
                    }
                } catch (const std::exception& e) {
                    // Try next encoding
                    continue;
                }
            }

            if (!success && load_error.empty()) {
                load_error = "Could not read file with any encoding";
            }
        }

        if (success) {
            try {
                // Get row count
                auto count_result = conn.Query("SELECT COUNT(*) FROM \"" + table_name + "\"");
                if (!count_result->HasError() && count_result->RowCount() > 0) {
                    result.row_count = count_result->GetValue(0, 0).GetValue<int64_t>();
                }
                result.status = "OK";
            } catch (const std::exception& e) {
                result.row_count = 0;
                result.status = std::string("Load failed: ") + e.what();
            }
        } else {
            result.row_count = 0;
            result.status = "Load failed: " + load_error;
        }

        // Drop table if 0 rows
        if (result.row_count == 0) {
            conn.Query("DROP TABLE IF EXISTS \"" + table_name + "\"");
        }

        results.push_back(result);
    }

    return results;
}

} // namespace duckdb
