#include "gdpdu_importer.hpp"
#include "gdpdu_parser.hpp"
#include "gdpdu_table_creator.hpp"
#include <sstream>
#include <algorithm>

namespace duckdb {

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
    
    // Build UPDATE statement to clean all VARCHAR columns
    // Remove control characters (0x00-0x1F, 0x7F-0x9F) but keep printable characters
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

// Build column list for INSERT
static std::string build_column_list(const TableDef& table) {
    std::ostringstream ss;
    for (size_t i = 0; i < table.columns.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << table.columns[i].name << "\"";
    }
    return ss.str();
}

// Build SELECT clause with type conversions for read_csv
static std::string build_select_clause(const TableDef& table) {
    std::ostringstream ss;
    for (size_t i = 0; i < table.columns.size(); ++i) {
        if (i > 0) ss << ", ";
        
        std::string col_ref = "column" + std::to_string(i);
        const auto& col = table.columns[i];
        
        switch (col.type) {
            case GdpduType::Numeric:
                if (col.precision > 0) {
                    // Decimal: replace German comma with dot, remove grouping dots
                    ss << "CAST(REPLACE(REPLACE(" << col_ref << ", '.', ''), ',', '.') AS DECIMAL(18," << col.precision << "))";
                } else {
                    // Integer: remove grouping dots
                    ss << "CAST(REPLACE(" << col_ref << ", '.', '') AS BIGINT)";
                }
                break;
            case GdpduType::Date:
                // German date DD.MM.YYYY -> DATE (handle empty/whitespace values)
                ss << "CASE WHEN " << col_ref << " IS NULL OR TRIM(" << col_ref << ") = '' THEN NULL ELSE strptime(TRIM(" << col_ref << "), '%d.%m.%Y')::DATE END";
                break;
            case GdpduType::AlphaNumeric:
            default:
                ss << col_ref;
                break;
        }
    }
    return ss.str();
}

std::vector<ImportResult> import_gdpdu_navision(Connection& conn, const std::string& directory_path, const std::string& column_name_field) {
    std::vector<ImportResult> results;
    
    // Step 1: Parse index.xml
    GdpduSchema schema;
    try {
        schema = parse_index_xml(directory_path, column_name_field);
    } catch (const std::exception& e) {
        ImportResult r;
        r.table_name = "(schema)";
        r.row_count = 0;
        r.status = std::string("Parse error: ") + e.what();
        results.push_back(r);
        return results;
    }
    
    // Step 2: Create tables
    auto create_results = create_tables(conn, schema);
    
    // Step 3: For each table, load data using DuckDB's native CSV reader
    for (size_t i = 0; i < schema.tables.size(); ++i) {
        const auto& table = schema.tables[i];
        ImportResult result;
        result.table_name = table.name;
        
        // Check table creation status
        if (!create_results[i].success) {
            result.row_count = 0;
            result.status = "Create failed: " + create_results[i].error_message;
            results.push_back(result);
            continue;
        }
        
        // Build data file path
        std::string data_path = join_path(directory_path, table.url);
        
        // Use DuckDB's native read_csv with:
        // - semicolon delimiter
        // - no header (GDPdU files don't have headers)
        // - explicit column definitions as VARCHAR
        // - skip lines if Range/From is specified
        // - disable strict mode to handle malformed rows
        // - enable null padding for missing columns
        // - try multiple encodings for German files
        
        // Try different encodings in order of likelihood for German/European files
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
        bool success = false;
        std::string load_error;

        for (const auto& encoding : encodings_to_try) {
            std::ostringstream sql;
            sql << "INSERT INTO \"" << table.name << "\" (" << build_column_list(table) << ") ";
            sql << "SELECT " << build_select_clause(table) << " ";
            sql << "FROM read_csv('" << escape_sql(data_path) << "', ";
            sql << "delim=';', ";
            sql << "header=false, ";
            sql << "quote='\"', ";
            sql << "all_varchar=true, ";
            sql << "auto_detect=false, ";
            sql << "strict_mode=false, ";
            sql << "null_padding=true, ";
            sql << "encoding='" << encoding << "', ";
            if (table.skip_lines > 0) {
                sql << "skip=" << table.skip_lines << ", ";
            }
            sql << "columns={";

            // Define expected columns with explicit names
            for (size_t j = 0; j < table.columns.size(); ++j) {
                if (j > 0) sql << ", ";
                sql << "'column" << j << "': 'VARCHAR'";
            }
            sql << "})";

            try {
                auto query_result = conn.Query(sql.str());
                if (!query_result->HasError()) {
                    success = true;
                    break;
                }
                // If error contains encoding info, try next encoding
                std::string error = query_result->GetError();
                if (error.find("unicode") != std::string::npos ||
                    error.find("encoding") != std::string::npos ||
                    error.find("utf-8") != std::string::npos) {
                    continue;  // Try next encoding
                }
                // Other error, stop trying
                load_error = error;
                break;
            } catch (const std::exception& e) {
                std::string error = e.what();
                if (error.find("unicode") != std::string::npos ||
                    error.find("encoding") != std::string::npos) {
                    continue;  // Try next encoding
                }
                // Other error
                load_error = error;
                break;
            }
        }

        // If all encodings failed, try with ignore_errors as last resort
        if (!success) {
            std::vector<std::string> fallback_encodings = {
                "ISO-8859-1", "Windows-1252", "CP1252", "UTF-8", "CP850"
            };

            for (const auto& encoding : fallback_encodings) {
                std::ostringstream sql;
                sql << "INSERT INTO \"" << table.name << "\" (" << build_column_list(table) << ") ";
                sql << "SELECT " << build_select_clause(table) << " ";
                sql << "FROM read_csv('" << escape_sql(data_path) << "', ";
                sql << "delim=';', ";
                sql << "header=false, ";
                sql << "quote='\"', ";
                sql << "all_varchar=true, ";
                sql << "auto_detect=false, ";
                sql << "strict_mode=false, ";
                sql << "null_padding=true, ";
                sql << "encoding='" << encoding << "', ";
                sql << "ignore_errors=true, ";
                if (table.skip_lines > 0) {
                    sql << "skip=" << table.skip_lines << ", ";
                }
                sql << "columns={";

                for (size_t j = 0; j < table.columns.size(); ++j) {
                    if (j > 0) sql << ", ";
                    sql << "'column" << j << "': 'VARCHAR'";
                }
                sql << "})";

                try {
                    auto query_result = conn.Query(sql.str());
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
                load_error = "Could not read file with any encoding (tried " +
                             std::to_string(encodings_to_try.size() + fallback_encodings.size()) + " encodings)";
            }
        }

        if (success) {
            try {
                // Clean and trim all VARCHAR columns (GDPdU already has proper types from XML)
                clean_and_trim_columns(conn, table.name);

                // Get row count
                auto count_result = conn.Query("SELECT COUNT(*) FROM \"" + table.name + "\"");
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

        // Don't keep empty tables in the database
        if (result.row_count == 0) {
            conn.Query("DROP TABLE IF EXISTS \"" + table.name + "\"");
            continue;
        }

        results.push_back(result);
    }
    
    return results;
}

std::vector<ImportResult> import_gdpdu_datev(Connection& conn, const std::string& directory_path) {
    // DATEV GDPdU format uses "Name" element for column names (standard GDPdU)
    // The underlying format is the same as Navision GDPdU, just with different
    // naming conventions (DATEV uses Name, Navision often uses Description)
    return import_gdpdu_navision(conn, directory_path, "Name");
}

} // namespace duckdb
