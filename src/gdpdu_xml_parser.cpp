#include "gdpdu_xml_parser.hpp"
#include "gdpdu_parser.hpp"
#include "gdpdu_schema.hpp"
#include <sstream>

namespace duckdb {

std::string GdpduXmlParser::gdpdu_type_to_duckdb_type_string(GdpduType type, int precision) {
    switch (type) {
        case GdpduType::AlphaNumeric:
            return "VARCHAR";
        case GdpduType::Numeric:
            if (precision > 0) {
                return "DECIMAL(18," + std::to_string(precision) + ")";
            }
            return "BIGINT";
        case GdpduType::Date:
            return "DATE";
        default:
            return "VARCHAR";
    }
}

XmlSchema GdpduXmlParser::convert_schema(const GdpduSchema& gdpdu_schema) {
    XmlSchema xml_schema;
    xml_schema.media_name = gdpdu_schema.media_name;
    
    for (const auto& gdpdu_table : gdpdu_schema.tables) {
        XmlTableSchema xml_table;
        xml_table.name = gdpdu_table.name;
        xml_table.url = gdpdu_table.url;
        xml_table.description = gdpdu_table.description;
        xml_table.is_utf8 = gdpdu_table.is_utf8;
        xml_table.decimal_symbol = gdpdu_table.decimal_symbol;
        xml_table.digit_grouping = gdpdu_table.digit_grouping;
        xml_table.skip_lines = gdpdu_table.skip_lines;
        xml_table.primary_key_columns = gdpdu_table.primary_key_columns;
        
        for (const auto& gdpdu_col : gdpdu_table.columns) {
            XmlTableSchema::Column xml_col;
            xml_col.name = gdpdu_col.name;
            xml_col.duckdb_type = gdpdu_type_to_duckdb_type_string(gdpdu_col.type, gdpdu_col.precision);
            xml_col.is_primary_key = gdpdu_col.is_primary_key;
            xml_col.precision = gdpdu_col.precision;
            
            xml_table.columns.push_back(xml_col);
        }
        
        xml_schema.tables.push_back(xml_table);
    }
    
    return xml_schema;
}

XmlSchema GdpduXmlParser::parse(const std::string& directory_path, const XmlParserConfig& config) {
    // Use the existing GDPdU parser
    std::string column_name_field = config.column_mapping.name_field;
    if (column_name_field.empty()) {
        column_name_field = "Name";
    }
    
    GdpduSchema gdpdu_schema = parse_index_xml(directory_path, column_name_field);
    
    // Convert to generic XmlSchema
    return convert_schema(gdpdu_schema);
}

} // namespace duckdb
