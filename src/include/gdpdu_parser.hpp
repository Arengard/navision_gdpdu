#pragma once

#include "gdpdu_schema.hpp"
#include <string>

namespace duckdb {

// Parse GDPdU index.xml from a directory path
// Returns GdpduSchema with all table definitions
// Throws std::runtime_error on parse failure
GdpduSchema parse_index_xml(const std::string& directory_path);

} // namespace duckdb
