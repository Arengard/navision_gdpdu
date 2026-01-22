#include "gdpdu_importer.hpp"
#include "gdpdu_parser.hpp"
#include "gdpdu_table_creator.hpp"
#include "gdpdu_data_parser.hpp"
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

// Escape single quotes for SQL
static std::string escape_sql_string(const std::string& value) {
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

// Format a value for SQL INSERT based on column type
static std::string format_sql_value(const std::string& value, const ColumnDef& col) {
    if (value.empty()) {
        return "NULL";
    }
    
    switch (col.type) {
        case GdpduType::Numeric:
            // Numeric values don't need quotes
            return value;
        case GdpduType::Date:
            // Date values need quotes
            return "'" + escape_sql_string(value) + "'";
        case GdpduType::AlphaNumeric:
        default:
            // String values need quotes
            return "'" + escape_sql_string(value) + "'";
    }
}

// Insert rows into table using batch INSERT statements
static void insert_rows(Connection& conn, const TableDef& table, const std::vector<ParsedRow>& rows) {
    if (rows.empty()) {
        return;
    }
    
    const size_t BATCH_SIZE = 1000;
    
    for (size_t batch_start = 0; batch_start < rows.size(); batch_start += BATCH_SIZE) {
        size_t batch_end = std::min(batch_start + BATCH_SIZE, rows.size());
        
        std::ostringstream sql;
        sql << "INSERT INTO \"" << table.name << "\" VALUES ";
        
        bool first_row = true;
        for (size_t i = batch_start; i < batch_end; ++i) {
            const auto& row = rows[i];
            
            if (!first_row) {
                sql << ", ";
            }
            first_row = false;
            
            sql << "(";
            for (size_t j = 0; j < row.size() && j < table.columns.size(); ++j) {
                if (j > 0) {
                    sql << ", ";
                }
                sql << format_sql_value(row[j], table.columns[j]);
            }
            sql << ")";
        }
        
        auto result = conn.Query(sql.str());
        if (result->HasError()) {
            throw std::runtime_error(result->GetError());
        }
    }
}

std::vector<ImportResult> import_gdpdu(Connection& conn, const std::string& directory_path) {
    std::vector<ImportResult> results;
    
    // Step 1: Parse index.xml
    GdpduSchema schema;
    try {
        schema = parse_index_xml(directory_path);
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
    
    // Step 3: For each table, load data
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
        
        // Parse data file
        DataParseResult parse_result;
        auto rows = GdpduDataParser::parse_file(data_path, table, parse_result);
        
        if (!parse_result.success) {
            result.row_count = 0;
            result.status = "Parse failed: " + parse_result.error_message;
            results.push_back(result);
            continue;
        }
        
        // Insert rows into table
        try {
            insert_rows(conn, table, rows);
            result.row_count = static_cast<int64_t>(rows.size());
            result.status = "OK";
        } catch (const std::exception& e) {
            result.row_count = 0;
            result.status = std::string("Insert failed: ") + e.what();
        }
        
        results.push_back(result);
    }
    
    return results;
}

} // namespace duckdb
