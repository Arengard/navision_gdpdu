#include "gdpdu_parser.hpp"
#include "gdpdu_schema.hpp"
#include <pugixml.hpp>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace duckdb {

// Convert a string to snake_case
// Handles both:
// - PascalCase/camelCase: "EUCountryRegionCode" -> "eu_country_region_code"
// - German descriptions: "EU-Laender-/Regionscode" -> "eu_laender_regionscode"
// - German umlauts: ä->a, ö->o, ü->u, ß->ss
static std::string to_snake_case(const std::string& input) {
    if (input.empty()) {
        return input;
    }
    
    std::string result;
    result.reserve(input.size() * 2);
    
    bool prev_was_lower = false;
    bool prev_was_upper = false;
    bool prev_was_underscore = true;  // Start as true to avoid leading underscore
    
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        
        // Handle German umlauts (UTF-8 encoded: 2 bytes each)
        // ä = C3 A4, ö = C3 B6, ü = C3 BC, Ä = C3 84, Ö = C3 96, Ü = C3 9C, ß = C3 9F
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
                    if (!prev_was_underscore) {
                        // No underscore needed before ss
                    }
                    result += "ss";
                    i++;  // Skip the second byte
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
                i++;  // Skip the second byte
                prev_was_lower = !is_upper;
                prev_was_upper = is_upper;
                prev_was_underscore = false;
                continue;
            }
        }
        
        // Handle ASCII letters
        if (std::isupper(c)) {
            // Add underscore before uppercase if preceded by lowercase
            // or if this is not the first char of a sequence of uppercase followed by lowercase
            if (prev_was_lower && !prev_was_underscore) {
                result += '_';
            }
            // Handle sequences like "EU" in "EUCountry" -> "eu_country"
            // Look ahead: if next char is lowercase and we had uppercase before, add underscore
            else if (prev_was_upper && !prev_was_underscore && i + 1 < input.size()) {
                unsigned char next_c = static_cast<unsigned char>(input[i + 1]);
                if (std::islower(next_c)) {
                    result += '_';
                }
            }
            result += static_cast<char>(std::tolower(c));
            prev_was_lower = false;
            prev_was_upper = true;
            prev_was_underscore = false;
        }
        else if (std::islower(c)) {
            result += c;
            prev_was_lower = true;
            prev_was_upper = false;
            prev_was_underscore = false;
        }
        else if (std::isdigit(c)) {
            result += c;
            prev_was_lower = false;
            prev_was_upper = false;
            prev_was_underscore = false;
        }
        else {
            // Non-alphanumeric characters (-, /, space, etc.) become underscores
            // but avoid consecutive underscores
            if (!prev_was_underscore && !result.empty()) {
                result += '_';
                prev_was_underscore = true;
            }
            prev_was_lower = false;
            prev_was_upper = false;
        }
    }
    
    // Remove trailing underscore if any
    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    
    return result;
}

// Path helper: normalize Windows/Unix paths
static std::string normalize_path(const std::string& path) {
    std::string result = path;
    // Normalize backslashes to forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');
    // Remove trailing slash
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

// Parse a single column (VariableColumn or VariablePrimaryKey)
// column_name_field: "Name" or "Description" - which element to use for column name
static ColumnDef parse_column(const pugi::xml_node& node, bool is_primary_key, const std::string& column_name_field) {
    ColumnDef col;
    std::string raw_name = node.child_value(column_name_field.c_str());
    // If the requested field is empty, fall back to "Name"
    if (raw_name.empty() && column_name_field != "Name") {
        raw_name = node.child_value("Name");
    }
    // Convert to snake_case
    col.name = to_snake_case(raw_name);
    col.is_primary_key = is_primary_key;
    
    // Determine type by checking for type child elements
    if (!node.child("AlphaNumeric").empty()) {
        col.type = GdpduType::AlphaNumeric;
        col.precision = 0;
    } else if (!node.child("Numeric").empty()) {
        col.type = GdpduType::Numeric;
        // Check for Accuracy sub-element
        pugi::xml_node numeric = node.child("Numeric");
        pugi::xml_node accuracy = numeric.child("Accuracy");
        if (!accuracy.empty()) {
            const char* accuracy_text = accuracy.child_value();
            if (accuracy_text && accuracy_text[0] != '\0') {
                col.precision = std::stoi(accuracy_text);
            } else {
                col.precision = 0;
            }
        } else {
            col.precision = 0;  // Integer (no decimal places)
        }
    } else if (!node.child("Date").empty()) {
        col.type = GdpduType::Date;
        col.precision = 0;
    } else {
        // Default to AlphaNumeric if type not found
        col.type = GdpduType::AlphaNumeric;
        col.precision = 0;
    }
    
    return col;
}

// Parse a single table definition
// column_name_field: "Name" or "Description" - which element to use for column names
static TableDef parse_table(const pugi::xml_node& table_node, const std::string& column_name_field) {
    TableDef table;
    
    // Extract basic metadata
    table.url = table_node.child_value("URL");
    table.name = table_node.child_value("Name");
    table.description = table_node.child_value("Description");
    table.is_utf8 = !table_node.child("UTF8").empty();
    
    // Extract locale settings (with defaults for German locale)
    table.decimal_symbol = ',';
    table.digit_grouping = '.';
    
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
    
    // Navigate to VariableLength element
    pugi::xml_node var_length = table_node.child("VariableLength");
    if (var_length.empty()) {
        // No columns defined - return empty table
        return table;
    }
    
    // Parse VariablePrimaryKey elements first (in document order)
    for (pugi::xml_node pk : var_length.children("VariablePrimaryKey")) {
        ColumnDef col = parse_column(pk, true, column_name_field);
        table.columns.push_back(col);
        table.primary_key_columns.push_back(col.name);
    }
    
    // Parse VariableColumn elements (in document order)
    for (pugi::xml_node vc : var_length.children("VariableColumn")) {
        ColumnDef col = parse_column(vc, false, column_name_field);
        table.columns.push_back(col);
    }
    
    return table;
}

// Main parser function
GdpduSchema parse_index_xml(const std::string& directory_path, const std::string& column_name_field) {
    GdpduSchema schema;
    
    // Build path to index.xml
    std::string index_path = join_path(directory_path, "index.xml");
    
    // Load XML document
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(index_path.c_str());
    
    if (!result) {
        throw std::runtime_error("Failed to parse index.xml at '" + index_path + "': " + 
                                 std::string(result.description()));
    }
    
    // Navigate to DataSet/Media
    pugi::xml_node dataset = doc.child("DataSet");
    if (dataset.empty()) {
        throw std::runtime_error("Invalid GDPdU format: missing DataSet element");
    }
    
    pugi::xml_node media = dataset.child("Media");
    if (media.empty()) {
        throw std::runtime_error("Invalid GDPdU format: missing Media element");
    }
    
    // Extract Media name
    schema.media_name = media.child_value("Name");
    
    // Iterate over Table elements
    for (pugi::xml_node table_node : media.children("Table")) {
        TableDef table = parse_table(table_node, column_name_field);
        schema.tables.push_back(table);
    }
    
    return schema;
}

} // namespace duckdb
