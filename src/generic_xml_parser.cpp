#include "generic_xml_parser.hpp"
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace duckdb {

// Reuse snake_case conversion from gdpdu_parser (simplified version)
std::string GenericXmlParser::to_snake_case(const std::string& input) {
    if (input.empty()) {
        return input;
    }
    
    std::string result;
    result.reserve(input.size() * 2);
    
    bool prev_was_lower = false;
    bool prev_was_upper = false;
    bool prev_was_underscore = true;
    
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        
        // Handle German umlauts (UTF-8 encoded: 2 bytes each)
        if (c == 0xC3 && i + 1 < input.size()) {
            unsigned char next = static_cast<unsigned char>(input[i + 1]);
            char replacement = 0;
            bool is_upper = false;
            
            switch (next) {
                case 0xA4: replacement = 'a'; break;  // ä
                case 0xB6: replacement = 'o'; break;  // ö
                case 0xBC: replacement = 'u'; break;  // ü
                case 0x84: replacement = 'a'; is_upper = true; break;  // Ä
                case 0x96: replacement = 'o'; is_upper = true; break;  // Ö
                case 0x9C: replacement = 'u'; is_upper = true; break;  // Ü
                case 0x9F:  // ß -> ss
                    result += "ss";
                    i++;
                    prev_was_lower = true;
                    prev_was_upper = false;
                    prev_was_underscore = false;
                    continue;
                default:
                    break;
            }
            
            if (replacement != 0) {
                if (is_upper && prev_was_lower && !prev_was_underscore) {
                    result += '_';
                }
                result += replacement;
                i++;
                prev_was_lower = !is_upper;
                prev_was_upper = is_upper;
                prev_was_underscore = false;
                continue;
            }
        }
        
        if (std::isupper(c)) {
            if (prev_was_lower && !prev_was_underscore) {
                result += '_';
            } else if (prev_was_upper && !prev_was_underscore && i + 1 < input.size()) {
                unsigned char next_c = static_cast<unsigned char>(input[i + 1]);
                if (std::islower(next_c)) {
                    result += '_';
                }
            }
            result += static_cast<char>(std::tolower(c));
            prev_was_lower = false;
            prev_was_upper = true;
            prev_was_underscore = false;
        } else if (std::islower(c) || std::isdigit(c)) {
            result += c;
            prev_was_lower = std::islower(c) != 0;
            prev_was_upper = false;
            prev_was_underscore = false;
        } else {
            if (!prev_was_underscore && !result.empty()) {
                result += '_';
                prev_was_underscore = true;
            }
            prev_was_lower = false;
            prev_was_upper = false;
        }
    }
    
    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    
    return result;
}

std::string GenericXmlParser::normalize_path(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    while (!result.empty() && result.back() == '/') {
        result.pop_back();
    }
    return result;
}

std::string GenericXmlParser::join_path(const std::string& dir, const std::string& file) {
    std::string norm_dir = normalize_path(dir);
    if (norm_dir.empty()) {
        return file;
    }
    return norm_dir + "/" + file;
}

std::string GenericXmlParser::get_child_value(const pugi::xml_node& node, const std::string& field_name, const std::string& fallback) {
    const char* value = node.child_value(field_name.c_str());
    if (value && value[0] != '\0') {
        return std::string(value);
    }
    return fallback;
}

std::string GenericXmlParser::map_type_to_duckdb(const std::string& xml_type, const XmlParserConfig& config, int precision) {
    // Check custom type mappings first
    auto it = config.type_mappings.find(xml_type);
    if (it != config.type_mappings.end()) {
        std::string duckdb_type = it->second;
        // If it's DECIMAL and we have precision, add it
        if (duckdb_type == "DECIMAL" && precision > 0) {
            return "DECIMAL(18," + std::to_string(precision) + ")";
        }
        return duckdb_type;
    }
    
    // Default mappings
    if (xml_type == "AlphaNumeric" || xml_type == "VARCHAR" || xml_type == "STRING") {
        return "VARCHAR";
    } else if (xml_type == "Numeric" || xml_type == "NUMBER" || xml_type == "INTEGER") {
        if (precision > 0) {
            return "DECIMAL(18," + std::to_string(precision) + ")";
        }
        return "BIGINT";
    } else if (xml_type == "Date" || xml_type == "DATE") {
        return "DATE";
    }
    
    // Default to VARCHAR
    return "VARCHAR";
}

XmlTableSchema::Column GenericXmlParser::parse_column(const pugi::xml_node& node, const XmlParserConfig& config, bool is_primary_key) {
    XmlTableSchema::Column col;
    col.is_primary_key = is_primary_key;
    
    // Get column name
    std::string raw_name = get_child_value(node, config.column_mapping.name_field);
    if (raw_name.empty() && config.column_mapping.name_field != "Name") {
        raw_name = get_child_value(node, "Name");
    }
    col.name = to_snake_case(raw_name);
    
    // Determine type
    std::string xml_type = "";
    int precision = 0;
    
    // Check for type child elements (common patterns)
    if (!node.child("AlphaNumeric").empty()) {
        xml_type = "AlphaNumeric";
    } else if (!node.child("Numeric").empty()) {
        xml_type = "Numeric";
        pugi::xml_node numeric = node.child("Numeric");
        pugi::xml_node accuracy = numeric.child("Accuracy");
        if (!accuracy.empty()) {
            const char* accuracy_text = accuracy.child_value();
            if (accuracy_text && accuracy_text[0] != '\0') {
                precision = std::stoi(accuracy_text);
            }
        }
    } else if (!node.child("Date").empty()) {
        xml_type = "Date";
    } else {
        // Try to get type from config.type_field
        if (!config.column_mapping.type_field.empty()) {
            xml_type = get_child_value(node, config.column_mapping.type_field);
        }
        
        // Try to get precision from config.precision_field
        if (!config.column_mapping.precision_field.empty()) {
            std::string prec_str = get_child_value(node, config.column_mapping.precision_field);
            if (!prec_str.empty()) {
                precision = std::stoi(prec_str);
            }
        }
    }
    
    col.precision = precision;
    col.duckdb_type = map_type_to_duckdb(xml_type, config, precision);
    
    return col;
}

XmlTableSchema GenericXmlParser::parse_table(const pugi::xml_node& table_node, const XmlParserConfig& config) {
    XmlTableSchema table;
    
    // Extract basic metadata
    table.url = get_child_value(table_node, config.table_mapping.url_field, "URL");
    table.name = get_child_value(table_node, config.table_mapping.name_field, "Name");
    table.description = get_child_value(table_node, config.table_mapping.description_field, "Description");
    table.is_utf8 = !table_node.child("UTF8").empty();
    
    // Extract locale settings
    table.decimal_symbol = config.decimal_symbol;
    table.digit_grouping = config.digit_grouping;
    table.skip_lines = 0;
    
    pugi::xml_node decimal_node = table_node.child("DecimalSymbol");
    if (!decimal_node.empty()) {
        const char* decimal_text = decimal_node.child_value();
        if (decimal_text && decimal_text[0] != '\0') {
            table.decimal_symbol = decimal_text[0];
        }
    }
    
    pugi::xml_node grouping_node = table_node.child("DigitGroupingSymbol");
    if (!grouping_node.empty()) {
        const char* grouping_text = grouping_node.child_value();
        if (grouping_text && grouping_text[0] != '\0') {
            table.digit_grouping = grouping_text[0];
        }
    }
    
    // Extract Range/From to determine how many lines to skip
    pugi::xml_node range_node = table_node.child("Range");
    if (!range_node.empty()) {
        pugi::xml_node from_node = range_node.child("From");
        if (!from_node.empty()) {
            const char* from_text = from_node.child_value();
            if (from_text && from_text[0] != '\0') {
                // Range/From is 1-based, but we need to skip (From - 1) lines
                // e.g., From=3 means skip 2 lines (lines 1 and 2)
                int from_value = std::stoi(from_text);
                if (from_value > 1) {
                    table.skip_lines = from_value - 1;
                }
            }
        }
    }
    
    // Parse primary key columns first
    for (pugi::xml_node pk : table_node.children(config.primary_key_element.c_str())) {
        XmlTableSchema::Column col = parse_column(pk, config, true);
        table.columns.push_back(col);
        table.primary_key_columns.push_back(col.name);
    }
    
    // Parse regular columns
    for (pugi::xml_node col_node : table_node.children(config.column_element.c_str())) {
        XmlTableSchema::Column col = parse_column(col_node, config, false);
        table.columns.push_back(col);
    }
    
    return table;
}

XmlSchema GenericXmlParser::parse(const std::string& directory_path, const XmlParserConfig& config) {
    XmlSchema schema;
    
    // Build path to index file
    std::string index_path = join_path(directory_path, config.index_file);
    
    // Load XML document
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(index_path.c_str());
    
    if (!result) {
        throw std::runtime_error("Failed to parse " + config.index_file + " at '" + index_path + "': " + 
                                 std::string(result.description()));
    }
    
    // Navigate to root element
    pugi::xml_node root = doc.first_child();
    
    // If root_element is specified, navigate to it
    if (!config.root_element.empty()) {
        std::istringstream path_stream(config.root_element);
        std::string segment;
        while (std::getline(path_stream, segment, '/')) {
            if (segment.empty()) continue;
            root = root.child(segment.c_str());
            if (root.empty()) {
                throw std::runtime_error("Invalid XML format: missing element '" + segment + "' in path '" + config.root_element + "'");
            }
        }
    }
    
    // Extract media/schema name if available
    pugi::xml_node name_node = root.child("Name");
    if (!name_node.empty()) {
        schema.media_name = name_node.child_value();
    }
    
    // Iterate over table elements
    for (pugi::xml_node table_node : root.children(config.table_element.c_str())) {
        XmlTableSchema table = parse_table(table_node, config);
        schema.tables.push_back(table);
    }
    
    return schema;
}

} // namespace duckdb
