#pragma once

#include "gdpdu_schema.hpp"
#include "duckdb.hpp"

namespace duckdb {

// Result of creating a single table
struct TableCreateResult {
    std::string table_name;
    int column_count;
    bool success;
    std::string error_message;  // Empty if success
    
    TableCreateResult() : column_count(0), success(false) {}
};

// Create all tables from schema in the given connection
// Drops existing tables before creating new ones
// Returns vector of results for each table
std::vector<TableCreateResult> create_tables(Connection& conn, const GdpduSchema& schema);

// Create a single table from TableDef
// Returns result with success/error info
TableCreateResult create_table(Connection& conn, const TableDef& table);

// Generate CREATE TABLE SQL statement for a table
std::string generate_create_table_sql(const TableDef& table);

} // namespace duckdb
