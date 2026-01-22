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
                // DECIMAL with precision: use precision for scale, and 18 for total precision
                // GDPdU Accuracy represents decimal places (scale)
                return "DECIMAL(18, " + std::to_string(col.precision) + ")";
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
