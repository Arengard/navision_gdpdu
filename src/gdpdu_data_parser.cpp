#include "gdpdu_data_parser.hpp"

namespace duckdb {

std::vector<std::string> GdpduDataParser::parse_line(
    const std::string& line,
    char delimiter,
    char quote_char
) {
    std::vector<std::string> fields;
    std::string current_field;
    bool in_quotes = false;
    
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        
        if (in_quotes) {
            if (c == quote_char) {
                // Check for escaped quote (doubled quote character)
                if (i + 1 < line.size() && line[i + 1] == quote_char) {
                    current_field += quote_char;
                    ++i; // Skip next quote
                } else {
                    in_quotes = false;
                }
            } else {
                current_field += c;
            }
        } else {
            if (c == quote_char) {
                in_quotes = true;
            } else if (c == delimiter) {
                fields.push_back(current_field);
                current_field.clear();
            } else {
                current_field += c;
            }
        }
    }
    
    // Don't forget last field
    fields.push_back(current_field);
    
    return fields;
}

std::string GdpduDataParser::convert_german_decimal(
    const std::string& value,
    char decimal_symbol,
    char digit_grouping
) {
    if (value.empty()) {
        return value;
    }
    
    std::string result;
    result.reserve(value.size());
    
    for (char c : value) {
        if (c == digit_grouping) {
            // Skip thousands separator
            continue;
        } else if (c == decimal_symbol) {
            // Convert decimal separator to dot
            result += '.';
        } else {
            result += c;
        }
    }
    
    return result;
}

std::string GdpduDataParser::convert_german_date(const std::string& value) {
    if (value.empty()) {
        return value;
    }
    
    // Expected format: DD.MM.YYYY (10 chars)
    if (value.size() != 10 || value[2] != '.' || value[5] != '.') {
        // Return as-is if not in expected format
        return value;
    }
    
    // Extract parts
    std::string day = value.substr(0, 2);
    std::string month = value.substr(3, 2);
    std::string year = value.substr(6, 4);
    
    // Return ISO format: YYYY-MM-DD
    return year + "-" + month + "-" + day;
}

std::string GdpduDataParser::convert_field(
    const std::string& value,
    const ColumnDef& column,
    const TableDef& table
) {
    if (value.empty()) {
        return value;
    }
    
    switch (column.type) {
        case GdpduType::Numeric:
            return convert_german_decimal(value, table.decimal_symbol, table.digit_grouping);
            
        case GdpduType::Date:
            return convert_german_date(value);
            
        case GdpduType::AlphaNumeric:
        default:
            return value; // No conversion needed for strings
    }
}

std::vector<ParsedRow> GdpduDataParser::parse_file(
    const std::string& file_path,
    const TableDef& table,
    DataParseResult& result
) {
    std::vector<ParsedRow> rows;
    result.table_name = table.name;
    result.file_path = file_path;
    
    std::ifstream file(file_path);
    if (!file.is_open()) {
        result.success = false;
        result.error_message = "Failed to open file: " + file_path;
        return rows;
    }
    
    std::string line;
    int64_t line_number = 0;
    
    while (std::getline(file, line)) {
        ++line_number;
        
        // Skip empty lines
        if (line.empty()) {
            continue;
        }
        
        // Handle Windows line endings (CRLF)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Skip if line became empty after removing \r
        if (line.empty()) {
            continue;
        }
        
        // Parse line into fields
        auto fields = parse_line(line);
        
        // Validate field count matches column count
        if (fields.size() != table.columns.size()) {
            result.success = false;
            result.error_message = "Line " + std::to_string(line_number) + 
                ": expected " + std::to_string(table.columns.size()) + 
                " fields, got " + std::to_string(fields.size());
            return rows;
        }
        
        // Convert each field based on column type
        ParsedRow row;
        row.reserve(fields.size());
        
        for (size_t i = 0; i < fields.size(); ++i) {
            row.push_back(convert_field(fields[i], table.columns[i], table));
        }
        
        rows.push_back(std::move(row));
    }
    
    result.rows_parsed = static_cast<int64_t>(rows.size());
    result.success = true;
    
    return rows;
}

} // namespace duckdb
