#include "xml_parser_factory.hpp"
#include "gdpdu_xml_parser.hpp"
#include "generic_xml_parser.hpp"
#include <memory>

namespace duckdb {

// Register all available parsers
// This function should be called during extension initialization
void register_xml_parsers() {
    // Register GDPdU parser
    XmlParserFactory::register_parser("gdpdu", []() {
        return std::unique_ptr<XmlParser>(new GdpduXmlParser());
    });
    
    // Register generic parser
    XmlParserFactory::register_parser("generic", []() {
        return std::unique_ptr<XmlParser>(new GenericXmlParser());
    });
}

} // namespace duckdb
