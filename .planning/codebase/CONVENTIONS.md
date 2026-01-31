# Coding Conventions

**Analysis Date:** 2026-01-24

## Naming Patterns

**Files:**
- PascalCase with `.cpp` and `.hpp` extensions
- Descriptive module names: `gdpdu_parser.cpp`, `gdpdu_importer.cpp`, `gdpdu_table_creator.cpp`
- Headers and implementations paired: `gdpdu_parser.hpp` → `gdpdu_parser.cpp`
- All source files in `src/`, all headers in `src/include/`

**Functions:**
- snake_case for static helper functions: `to_snake_case()`, `normalize_path()`, `join_path()`, `parse_line()`, `convert_field()`
- snake_case for public functions: `import_gdpdu()`, `parse_index_xml()`, `create_tables()`, `generate_create_table_sql()`
- Prefixed with module context for internal state functions: `GdpduImportBind()`, `GdpduImportInit()`, `GdpduImportScan()`

**Variables:**
- snake_case for local and member variables: `directory_path`, `column_name_field`, `result`, `current_row`, `is_primary_key`
- Descriptive names reflecting purpose: `delimiter`, `quote_char`, `decimal_symbol`, `digit_grouping`
- Prefix with type hint when helpful: `is_utf8`, `prev_was_lower`, `in_quotes`, `done`

**Types:**
- PascalCase for classes and structs: `GdpduExtension`, `GdpduSchema`, `TableDef`, `ColumnDef`, `GdpduDataParser`
- PascalCase for enums: `GdpduType`
- Enum values UPPERCASE: `AlphaNumeric`, `Numeric`, `Date`
- Result structs named with Result suffix: `ImportResult`, `TableCreateResult`, `DataParseResult`

## Code Style

**Formatting:**
- CMake enforces C++11 standard: `set(CMAKE_CXX_STANDARD 11)`
- No external formatter detected (no .clang-format, .editorconfig, or prettier config)
- Indentation: 4 spaces (observed in all files)
- Line length: No strict limit observed, practical max ~120 chars

**Linting:**
- No linter configuration found (no .eslintrc equivalent for C++)
- No pre-commit hooks or static analysis configuration
- Reliance on manual code review

**Bracing Style:**
- Allman style (opening brace on new line for functions): see `gdpdu_extension.cpp:118`
- Inline braces for short conditions: `if (result.empty()) { return result; }`
- Consistent spacing around operators and control structures

**Memory Management:**
- Modern C++11: `unique_ptr`, `make_uniq()` for heap allocation (DuckDB API)
- RAII pattern observed: `std::ifstream` automatically closed on scope exit
- No raw pointers in high-level code

## Import Organization

**Order:**
1. Local project headers (in quotes): `#include "gdpdu_extension.hpp"`
2. DuckDB headers (angle brackets): `#include "duckdb.hpp"`, `#include "duckdb/main/extension.hpp"`
3. External library headers: `#include <pugixml.hpp>` (XML parsing)
4. Standard library headers (alphabetical): `#include <string>`, `#include <vector>`, `#include <algorithm>`, `#include <stdexcept>`
5. Standard C headers: `#include <cctype>`, `#include <sstream>`, `#include <fstream>`, `#include <cstdint>`

**Namespace Usage:**
- All code in `namespace duckdb { ... }` matching DuckDB extension conventions
- No using directives; fully qualified names (e.g., `std::string`, `std::vector`)
- Exception: local helper functions declared static to limit scope

**Header Guards:**
- `#pragma once` used exclusively (see all `.hpp` files)
- No traditional `#ifndef` guards

## Error Handling

**Patterns:**
- Exceptions for critical parsing failures: `throw std::runtime_error("message")`
- Result structs for recoverable errors: `ImportResult.status`, `TableCreateResult.success + error_message`
- Two-phase result pattern: out parameter `DataParseResult& result` passed to `parse_file()`

**Examples:**
```cpp
// Exception-based for unrecoverable parse errors
if (!result) {
    throw std::runtime_error("Failed to parse index.xml at '" + index_path + "': " +
                             std::string(result.description()));
}

// Result struct pattern for table operations
TableCreateResult result;
result.success = false;
result.error_message = "Create failed: " + error_msg;
return result;

// Try-catch for DuckDB operations
try {
    auto query_result = conn.Query(sql.str());
    if (query_result->HasError()) {
        result.status = "Load failed: " + query_result->GetError();
    } else {
        result.status = "OK";
    }
} catch (const std::exception& e) {
    result.status = std::string("Load failed: ") + e.what();
}
```

## Logging

**Framework:** No logging framework detected. Uses no logging (silent operation expected in extension context).

**Patterns:**
- No logging statements in production code
- Extension runs within DuckDB context; errors communicated via status strings in result structs
- Diagnostics via GitHub Actions workflow steps for CI/CD

## Comments

**When to Comment:**
- High-level block comments for algorithm explanation: `// Convert a string to snake_case` (line 10 in gdpdu_parser.cpp)
- Comments before complex sections: `// Handle German umlauts (UTF-8 encoded: 2 bytes each)` (line 30 in gdpdu_parser.cpp)
- Minimal inline comments; code should be self-documenting
- Comments for non-obvious behavior: UTF-8 character handling, German locale assumptions

**Documentation Comments:**
- Simple inline comments above public functions and classes
- Parameter descriptions in function signatures: `// column_name_field: "Name" or "Description" - which element to use for column names`
- No JSDoc/Doxygen style used; manual comments only

**Example patterns:**
```cpp
// Path helper: normalize Windows/Unix paths
static std::string normalize_path(const std::string& path) { ... }

// Escape single quotes for SQL string literals
static std::string escape_sql(const std::string& value) { ... }

// Build SELECT clause with type conversions for read_csv
static std::string build_select_clause(const TableDef& table) { ... }
```

## Function Design

**Size Guidelines:**
- Functions are concise, typically 10-50 lines
- Larger functions (100+ lines) handle multiple logical steps with clear comments: `parse_index_xml()` is 35 lines
- Helper functions extracted for reusability: `normalize_path()`, `join_path()`, `escape_sql()`

**Parameters:**
- Pass-by-const-reference for large objects: `const std::string&`, `const pugi::xml_node&`, `const TableDef&`
- Pass-by-mutable-reference for output: `vector<LogicalType>&`, `DataParseResult&`
- Mutable references used for output parameters, not return values (DuckDB convention)
- Small values passed by value: `bool`, `char`, `idx_t`

**Return Values:**
- Single responsibility: each function returns one primary type
- Result structs for multiple return values: `ImportResult`, `TableCreateResult`
- Vectors for collections: `std::vector<ImportResult>`, `std::vector<ColumnDef>`
- Exceptions for error conditions (not null/invalid returns)

**Return patterns:**
```cpp
// Move semantics for heap-allocated results
return std::move(bind_data);
return std::move(state);

// Direct return for vectors (copy optimization)
return rows;

// Result structs for complex operations
TableCreateResult result;
result.success = true;
return result;
```

## Module Design

**Exports:**
- Public functions declared in `.hpp` headers
- Static functions in `.cpp` files for internal helpers (not exported)
- One primary public function per module: `import_gdpdu()`, `parse_index_xml()`, `create_tables()`
- Supporting result/data structs exported via headers

**Barrel Files:**
- Not used; single responsibility principle per file
- Each module has clear purpose: parser, importer, table creator, schema, data parser

**Module Dependencies:**
```
gdpdu_extension.cpp
  ├─ gdpdu_importer.hpp
  ├─ duckdb.hpp
  └─ duckdb/main/extension.hpp

gdpdu_importer.cpp
  ├─ gdpdu_parser.hpp
  ├─ gdpdu_table_creator.hpp
  └─ duckdb.hpp

gdpdu_parser.cpp
  ├─ gdpdu_schema.hpp
  ├─ pugixml.hpp (XML parsing)
  └─ standard library

gdpdu_schema.cpp
  └─ gdpdu_schema.hpp

gdpdu_table_creator.cpp
  ├─ gdpdu_schema.hpp
  └─ duckdb.hpp

gdpdu_data_parser.cpp
  └─ gdpdu_schema.hpp
```

**Visibility Pattern:**
- Public in `.hpp`: result structs, public functions, class definitions
- Internal in `.cpp`: static helper functions, implementation details
- Example: `to_snake_case()`, `normalize_path()`, `parse_line()` are static and file-scoped

---

*Convention analysis: 2026-01-24*
