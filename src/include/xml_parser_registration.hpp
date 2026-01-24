#pragma once

namespace duckdb {

// Register all available XML parsers with the factory
// This should be called during extension initialization
void register_xml_parsers();

} // namespace duckdb
