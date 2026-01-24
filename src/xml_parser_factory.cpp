#include "xml_parser_factory.hpp"
#include <mutex>
#include <vector>
#include <string>

namespace duckdb {

std::map<std::string, XmlParserFactory::ParserCreator>& XmlParserFactory::get_registry() {
    static std::map<std::string, ParserCreator> registry;
    static std::once_flag flag;
    std::call_once(flag, []() {
        // Registry will be populated by parser implementations
    });
    return registry;
}

void XmlParserFactory::register_parser(const std::string& type, ParserCreator creator) {
    get_registry()[type] = creator;
}

std::unique_ptr<XmlParser> XmlParserFactory::create_parser(const std::string& type) {
    auto& registry = get_registry();
    auto it = registry.find(type);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

std::vector<std::string> XmlParserFactory::get_available_parsers() {
    std::vector<std::string> parsers;
    auto& registry = get_registry();
    for (const auto& pair : registry) {
        parsers.push_back(pair.first);
    }
    return parsers;
}

} // namespace duckdb
