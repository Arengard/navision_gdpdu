#pragma once

#include "xml_parser_config.hpp"
#include <memory>
#include <map>
#include <functional>

namespace duckdb {

// Factory for creating XML parsers
class XmlParserFactory {
public:
    using ParserCreator = std::function<std::unique_ptr<XmlParser>()>;
    
    // Register a parser type
    static void register_parser(const std::string& type, ParserCreator creator);
    
    // Create a parser by type
    static std::unique_ptr<XmlParser> create_parser(const std::string& type);
    
    // Get list of available parser types
    static std::vector<std::string> get_available_parsers();
    
private:
    static std::map<std::string, ParserCreator>& get_registry();
};

} // namespace duckdb
