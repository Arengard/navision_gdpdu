#pragma once

#include <string>
#include <vector>

namespace duckdb {

// GDPdU data types from index.xml
enum class GdpduType {
    AlphaNumeric,  // Maps to VARCHAR
    Numeric,       // Maps to DECIMAL(precision, scale) or BIGINT
    Date           // Maps to DATE
};

// Column definition from VariableColumn or VariablePrimaryKey
struct ColumnDef {
    std::string name;           // From <Name> element
    GdpduType type;             // AlphaNumeric, Numeric, or Date
    int precision;              // From <Accuracy> element, 0 if not specified
    bool is_primary_key;        // true if from VariablePrimaryKey
    
    ColumnDef() : type(GdpduType::AlphaNumeric), precision(0), is_primary_key(false) {}
};

// Table definition from Table element
struct TableDef {
    std::string name;           // From Table/Name element
    std::string url;            // From Table/URL element (data file path)
    std::string description;    // From Table/Description element
    bool is_utf8;               // true if UTF8 element present
    char decimal_symbol;        // From DecimalSymbol (default ',')
    char digit_grouping;        // From DigitGroupingSymbol (default '.')
    int skip_lines;             // From Range/From element (lines to skip, default 0)
    std::vector<ColumnDef> columns;  // All columns in order
    std::vector<std::string> primary_key_columns;  // Names of PK columns
    
    TableDef() : is_utf8(false), decimal_symbol(','), digit_grouping('.'), skip_lines(0) {}
};

// Schema definition from DataSet/Media
struct GdpduSchema {
    std::string media_name;     // From Media/Name
    std::vector<TableDef> tables;
};

// Helper functions

// Convert GdpduType to string for debugging
std::string gdpdu_type_to_string(GdpduType type);

// Convert ColumnDef to DuckDB type string for CREATE TABLE
std::string gdpdu_type_to_duckdb_type(const ColumnDef& col);

} // namespace duckdb
