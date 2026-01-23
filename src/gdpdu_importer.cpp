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

std::vector<ImportResult> import_gdpdu(Connection& conn, const std::string& directory_path, const std::string& column_name_field) {
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
        std::ostringstream sql;
        sql << "INSERT INTO \"" << table.name << "\" (" << build_column_list(table) << ") ";
        sql << "SELECT " << build_select_clause(table) << " ";
        sql << "FROM read_csv('" << escape_sql(data_path) << "', ";
        sql << "delim=';', ";
        sql << "header=false, ";
        sql << "quote='\"', ";
        sql << "all_varchar=true, ";
        sql << "columns={";
        
        // Define expected columns with explicit names
        for (size_t j = 0; j < table.columns.size(); ++j) {
            if (j > 0) sql << ", ";
            sql << "'column" << j << "': 'VARCHAR'";
        }
        sql << "})";
        
        try {
            auto query_result = conn.Query(sql.str());
            if (query_result->HasError()) {
                result.row_count = 0;
                result.status = "Load failed: " + query_result->GetError();
            } else {
                // Get row count
                auto count_result = conn.Query("SELECT COUNT(*) FROM \"" + table.name + "\"");
                if (!count_result->HasError() && count_result->RowCount() > 0) {
                    result.row_count = count_result->GetValue(0, 0).GetValue<int64_t>();
                }
                result.status = "OK";
            }
        } catch (const std::exception& e) {
            result.row_count = 0;
            result.status = std::string("Load failed: ") + e.what();
        }
        
        results.push_back(result);
    }
    
    return results;
}

} // namespace duckdb
