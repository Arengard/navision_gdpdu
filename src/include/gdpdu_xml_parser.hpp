#pragma once

#include "xml_parser_config.hpp"
#include "xml_parser_factory.hpp"
#include "gdpdu_schema.hpp"
#include <string>

namespace duckdb {

// GDPdU-specific XML parser that implements the generic XmlParser interface
class GdpduXmlParser : public XmlParser {
public:
    GdpduXmlParser() = default;
    ~GdpduXmlParser() override = default;
    
    XmlSchema parse(const std::string& directory_path, const XmlParserConfig& config) override;
    std::string get_parser_type() const override { return "gdpdu"; }
    
    // Convert from GdpduSchema to XmlSchema (for backward compatibility)
    static XmlSchema convert_schema(const GdpduSchema& gdpdu_schema);
    
private:
    // Helper to convert GdpduType to DuckDB type string
    static std::string gdpdu_type_to_duckdb_type_string(GdpduType type, int precision);
};

} // namespace duckdb
