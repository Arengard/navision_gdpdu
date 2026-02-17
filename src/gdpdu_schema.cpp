#include "gdpdu_schema.hpp"

namespace duckdb {

std::string gdpdu_type_to_string(GdpduType type) {
    switch (type) {
        case GdpduType::AlphaNumeric:
            return "AlphaNumeric";
        case GdpduType::Numeric:
            return "Numeric";
        case GdpduType::Date:
            return "Date";
        default:
            return "Unknown";
    }
}

std::string gdpdu_type_to_duckdb_type(const ColumnDef& col) {
    switch (col.type) {
        case GdpduType::AlphaNumeric:
            return "VARCHAR";
        case GdpduType::Numeric:
            if (col.precision > 0) {
                // Use MaxLength for total precision if available, otherwise default to 18
                // Ensure total precision is at least scale + 1 and at most 38 (DuckDB max)
                int total_precision = 18;
                if (col.max_length > 0) {
                    // MaxLength includes digits + decimal separator + sign
                    // Use it directly as total digits, clamped to DuckDB's max of 38
                    total_precision = col.max_length;
                }
                if (total_precision <= col.precision) {
                    total_precision = col.precision + 1;
                }
                if (total_precision > 38) {
                    total_precision = 38;
                }
                return "DECIMAL(" + std::to_string(total_precision) + ", " + std::to_string(col.precision) + ")";
            } else {
                // No precision specified = integer
                return "BIGINT";
            }
        case GdpduType::Date:
            return "DATE";
        default:
            return "VARCHAR";
    }
}

} // namespace duckdb
