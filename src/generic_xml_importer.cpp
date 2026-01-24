#include "generic_xml_importer.hpp"
#include "xml_parser_factory.hpp"
#include "gdpdu_table_creator.hpp"
#include "gdpdu_schema.hpp"
#include "gdpdu_importer.hpp"
#include <sstream>
#include <algorithm>

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

// Build column list for INSERT
static std::string build_column_list(const XmlTableSchema& table) {
    std::ostringstream ss;
    for (size_t i = 0; i < table.columns.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << table.columns[i].name << "\"";
    }
    return ss.str();
}

// Build SELECT clause with type conversions for read_csv
static std::string build_select_clause(const XmlTableSchema& table, const XmlParserConfig& config) {
    std::ostringstream ss;
    for (size_t i = 0; i < table.columns.size(); ++i) {
        if (i > 0) ss << ", ";
        
        std::string col_ref = "column" + std::to_string(i);
        const auto& col = table.columns[i];
        
        // Check if it's a numeric type that needs conversion
        if (col.duckdb_type.find("DECIMAL") == 0 || col.duckdb_type == "BIGINT") {
            if (col.duckdb_type.find("DECIMAL") == 0) {
                // Decimal: replace grouping symbol, replace decimal symbol with dot
                ss << "CAST(REPLACE(REPLACE(" << col_ref << ", '";
                ss << table.digit_grouping << "', ''), '";
                ss << table.decimal_symbol << "', '.')) AS " << col.duckdb_type << ")";
            } else {
                // Integer: remove grouping symbol
                ss << "CAST(REPLACE(" << col_ref << ", '";
                ss << table.digit_grouping << "', '') AS " << col.duckdb_type << ")";
            }
        } else if (col.duckdb_type == "DATE") {
            // Date: handle German format DD.MM.YYYY or other formats
            ss << "CASE WHEN " << col_ref << " IS NULL OR TRIM(" << col_ref << ") = '' THEN NULL ";
            ss << "ELSE strptime(TRIM(" << col_ref << "), '%d.%m.%Y')::DATE END";
        } else {
            // VARCHAR or other types
            ss << col_ref;
        }
    }
    return ss.str();
}

// Convert XmlTableSchema to TableDef for table creation
static TableDef convert_to_table_def(const XmlTableSchema& xml_table) {
    TableDef table;
    table.name = xml_table.name;
    table.url = xml_table.url;
    table.description = xml_table.description;
    table.is_utf8 = xml_table.is_utf8;
    table.decimal_symbol = xml_table.decimal_symbol;
    table.digit_grouping = xml_table.digit_grouping;
    table.skip_lines = xml_table.skip_lines;
    table.primary_key_columns = xml_table.primary_key_columns;
    
    for (const auto& xml_col : xml_table.columns) {
        ColumnDef col;
        col.name = xml_col.name;
        col.is_primary_key = xml_col.is_primary_key;
        col.precision = xml_col.precision;
        
        // Convert DuckDB type string back to GdpduType
        if (xml_col.duckdb_type == "VARCHAR" || xml_col.duckdb_type.find("VARCHAR") == 0) {
            col.type = GdpduType::AlphaNumeric;
        } else if (xml_col.duckdb_type.find("DECIMAL") == 0 || xml_col.duckdb_type == "BIGINT") {
            col.type = GdpduType::Numeric;
        } else if (xml_col.duckdb_type == "DATE") {
            col.type = GdpduType::Date;
        } else {
            col.type = GdpduType::AlphaNumeric;
        }
        
        table.columns.push_back(col);
    }
    
    return table;
}

std::vector<ImportResult> import_xml_data(
    Connection& conn,
    const std::string& directory_path,
    const std::string& parser_type,
    const XmlParserConfig& config) {
    
    std::vector<ImportResult> results;
    
    // Step 1: Get parser from factory
    auto parser = XmlParserFactory::create_parser(parser_type);
    if (!parser) {
        ImportResult r;
        r.table_name = "(parser)";
        r.row_count = 0;
        r.status = "Parser type '" + parser_type + "' not found. Available: " + 
                   [&]() {
                       auto available = XmlParserFactory::get_available_parsers();
                       std::string result;
                       for (size_t i = 0; i < available.size(); ++i) {
                           if (i > 0) result += ", ";
                           result += available[i];
                       }
                       return result.empty() ? "none" : result;
                   }();
        results.push_back(r);
        return results;
    }
    
    // Step 2: Parse XML
    XmlSchema schema;
    try {
        XmlParserConfig parser_config = config;
        parser_config.parser_type = parser_type;
        schema = parser->parse(directory_path, parser_config);
    } catch (const std::exception& e) {
        ImportResult r;
        r.table_name = "(schema)";
        r.row_count = 0;
        r.status = std::string("Parse error: ") + e.what();
        results.push_back(r);
        return results;
    }
    
    // Step 3: Convert XmlSchema to GdpduSchema and create tables
    GdpduSchema gdpdu_schema;
    gdpdu_schema.media_name = schema.media_name;
    
    for (const auto& xml_table : schema.tables) {
        gdpdu_schema.tables.push_back(convert_to_table_def(xml_table));
    }
    
    auto create_results = create_tables(conn, gdpdu_schema);
    
    // Step 4: Load data for each table
    for (size_t i = 0; i < schema.tables.size(); ++i) {
        const auto& xml_table = schema.tables[i];
        ImportResult result;
        result.table_name = xml_table.name;
        
        // Check table creation status
        if (!create_results[i].success) {
            result.row_count = 0;
            result.status = "Create failed: " + create_results[i].error_message;
            results.push_back(result);
            continue;
        }
        
        // Build data file path
        std::string data_path = join_path(directory_path, xml_table.url);
        
        // Use DuckDB's native read_csv
        std::ostringstream sql;
        sql << "INSERT INTO \"" << xml_table.name << "\" (" << build_column_list(xml_table) << ") ";
        sql << "SELECT " << build_select_clause(xml_table, config) << " ";
        sql << "FROM read_csv('" << escape_sql(data_path) << "', ";
        sql << "delim='" << config.delimiter << "', ";
        sql << "header=" << (config.has_header ? "true" : "false") << ", ";
        sql << "quote='\"', ";
        sql << "all_varchar=true, ";
        sql << "auto_detect=false, ";
        sql << "strict_mode=false, ";
        sql << "null_padding=true, ";
        if (xml_table.skip_lines > 0) {
            sql << "skip=" << xml_table.skip_lines << ", ";
        }
        sql << "columns={";
        
        // Define expected columns
        for (size_t j = 0; j < xml_table.columns.size(); ++j) {
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
                // Clean and trim all VARCHAR columns (XML already has proper types from schema)
                clean_and_trim_columns(conn, xml_table.name);
                
                // Get row count
                auto count_result = conn.Query("SELECT COUNT(*) FROM \"" + xml_table.name + "\"");
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
