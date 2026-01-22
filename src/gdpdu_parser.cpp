#include "gdpdu_parser.hpp"
#include "gdpdu_schema.hpp"
#include <pugixml.hpp>
#include <stdexcept>
#include <algorithm>

namespace duckdb {

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
static ColumnDef parse_column(const pugi::xml_node& node, bool is_primary_key) {
    ColumnDef col;
    col.name = node.child_value("Name");
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
static TableDef parse_table(const pugi::xml_node& table_node) {
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
        ColumnDef col = parse_column(pk, true);
        table.columns.push_back(col);
        table.primary_key_columns.push_back(col.name);
    }
    
    // Parse VariableColumn elements (in document order)
    for (pugi::xml_node vc : var_length.children("VariableColumn")) {
        ColumnDef col = parse_column(vc, false);
        table.columns.push_back(col);
    }
    
    return table;
}

// Main parser function
GdpduSchema parse_index_xml(const std::string& directory_path) {
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
        TableDef table = parse_table(table_node);
        schema.tables.push_back(table);
    }
    
    return schema;
}

} // namespace duckdb
