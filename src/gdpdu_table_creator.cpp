#include "gdpdu_table_creator.hpp"
#include "gdpdu_schema.hpp"
#include <sstream>

namespace duckdb {

std::string generate_create_table_sql(const TableDef& table) {
    std::ostringstream sql;
    sql << "CREATE TABLE \"" << table.name << "\" (";
    
    bool first = true;
    for (const auto& col : table.columns) {
        if (!first) {
            sql << ", ";
        }
        first = false;
        
        // Quote column name to handle special chars like "VAT%"
        sql << "\"" << col.name << "\" " << gdpdu_type_to_duckdb_type(col);
    }
    
    sql << ")";
    return sql.str();
}

TableCreateResult create_table(Connection& conn, const TableDef& table) {
    TableCreateResult result;
    result.table_name = table.name;
    result.column_count = static_cast<int>(table.columns.size());
    
    try {
        // Drop existing table first
        std::string drop_sql = "DROP TABLE IF EXISTS \"" + table.name + "\"";
        conn.Query(drop_sql);
        
        // Create new table
        std::string create_sql = generate_create_table_sql(table);
        auto query_result = conn.Query(create_sql);
        
        if (query_result->HasError()) {
            result.success = false;
            result.error_message = query_result->GetError();
        } else {
            result.success = true;
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }
    
    return result;
}

std::vector<TableCreateResult> create_tables(Connection& conn, const GdpduSchema& schema) {
    std::vector<TableCreateResult> results;
    results.reserve(schema.tables.size());
    
    for (const auto& table : schema.tables) {
        results.push_back(create_table(conn, table));
    }
    
    return results;
}

} // namespace duckdb
