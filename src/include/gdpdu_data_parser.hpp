#pragma once

#include "gdpdu_schema.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

namespace duckdb {

// Result of parsing a single data file
struct DataParseResult {
    std::string table_name;
    std::string file_path;
    int64_t rows_parsed;
    bool success;
    std::string error_message;
    
    DataParseResult() : rows_parsed(0), success(false) {}
};

// Parsed row - vector of string values (conversion happens at insert time)
using ParsedRow = std::vector<std::string>;

// Data parser class for GDPdU .txt files
class GdpduDataParser {
public:
    // Parse a single line into fields
    // Handles semicolon-delimited, quoted CSV format
    static std::vector<std::string> parse_line(
        const std::string& line,
        char delimiter = ';',
        char quote_char = '"'
    );
    
    // Convert German decimal to standard format
    // "45.967,50" -> "45967.50"
    static std::string convert_german_decimal(
        const std::string& value,
        char decimal_symbol = ',',
        char digit_grouping = '.'
    );
    
    // Convert German date to ISO format
    // "01.06.2024" -> "2024-06-01"
    static std::string convert_german_date(const std::string& value);
    
    // Convert a field value based on column type
    static std::string convert_field(
        const std::string& value,
        const ColumnDef& column,
        const TableDef& table
    );
    
    // Parse entire file and return rows
    // Converts fields based on column types from TableDef
    static std::vector<ParsedRow> parse_file(
        const std::string& file_path,
        const TableDef& table,
        DataParseResult& result
    );
};

} // namespace duckdb
