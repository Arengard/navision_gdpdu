#pragma once

#include "duckdb.hpp"
#include <string>
#include <vector>

namespace duckdb {

// Result of importing a single Buchungsstapel file
struct BuchungsstapelImportResult {
    std::string table_name;
    std::string file_name;
    int64_t row_count;
    std::string status;  // "OK" or error message

    BuchungsstapelImportResult() : row_count(0), status("") {}
};

// Import all DATEV Buchungsstapel EXTF CSV files from a folder
// For each file:
//   1. Parses header row (row 1) to extract year from Datum-von field
//   2. Parses column names from row 2
//   3. Creates a DuckDB table named after the file (without .csv extension)
//   4. Loads data with type conversions:
//      - Belegdatum: DDMM compact format -> DATE
//      - Umsatz/Basis-Umsatz: German decimal (comma) -> DECIMAL(18,2)
//      - Kurs: German decimal -> DECIMAL(18,6)
//      - All other columns: VARCHAR
//   5. Adds file_name column with the source filename
std::vector<BuchungsstapelImportResult> import_buchungsstapel(
    Connection& conn,
    const std::string& folder_path
);

} // namespace duckdb
