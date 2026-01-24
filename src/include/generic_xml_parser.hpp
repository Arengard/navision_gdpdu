#pragma once

#include "xml_parser_config.hpp"
#include "xml_parser_factory.hpp"
#include <pugixml.hpp>
#include <string>

namespace duckdb {

// Generic XML parser that can be configured for different XML formats
class GenericXmlParser : public XmlParser {
public:
    GenericXmlParser() = default;
    ~GenericXmlParser() override = default;
    
    XmlSchema parse(const std::string& directory_path, const XmlParserConfig& config) override;
    std::string get_parser_type() const override { return "generic"; }
    
private:
    // Helper to get child value with fallback
    std::string get_child_value(const pugi::xml_node& node, const std::string& field_name, const std::string& fallback = "");
    
    // Parse a single column
    XmlTableSchema::Column parse_column(const pugi::xml_node& node, const XmlParserConfig& config, bool is_primary_key);
    
    // Parse a single table
    XmlTableSchema parse_table(const pugi::xml_node& table_node, const XmlParserConfig& config);
    
    // Convert XML type to DuckDB type
    std::string map_type_to_duckdb(const std::string& xml_type, const XmlParserConfig& config, int precision = 0);
    
    // Convert string to snake_case (reuse from gdpdu_parser)
    std::string to_snake_case(const std::string& input);
    
    // Path helpers
    std::string normalize_path(const std::string& path);
    std::string join_path(const std::string& dir, const std::string& file);
};

} // namespace duckdb
