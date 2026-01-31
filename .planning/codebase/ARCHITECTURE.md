# Architecture

**Analysis Date:** 2026-01-24

## Pattern Overview

**Overall:** Layered Data Import Pipeline with DuckDB Extension Architecture

**Key Characteristics:**
- DuckDB C++ extension pattern (DuckDB 1.4+ CPP Extension API)
- Multi-stage processing pipeline: XML parsing → schema validation → table creation → data loading
- Separation of concerns: parsing, schema modeling, table management, data loading
- Table function API for user-facing operations
- Native DuckDB CSV reading for performance

## Layers

**Extension Interface Layer:**
- Purpose: Expose `gdpdu_import()` table function to DuckDB SQL
- Location: `src/gdpdu_extension.cpp` and `src/include/gdpdu_extension.hpp`
- Contains: DuckDB extension registration, table function binding, state management, and scanning
- Depends on: DuckDB core, GdpduImporter for actual logic
- Used by: DuckDB SQL engine directly

**Import Orchestration Layer:**
- Purpose: Coordinate the complete import workflow
- Location: `src/gdpdu_importer.cpp` and `src/include/gdpdu_importer.hpp`
- Contains: Three-phase import process (parse schema, create tables, load data)
- Depends on: XML parser, table creator, data parser
- Used by: Extension interface layer

**Schema Parsing Layer:**
- Purpose: Parse GDPdU index.xml and extract schema definitions
- Location: `src/gdpdu_parser.cpp` and `src/include/gdpdu_parser.hpp`
- Contains: XML DOM navigation, column name conversion, type determination
- Depends on: pugixml, GdpduSchema data structures
- Used by: Import orchestration layer

**Schema Definition Layer:**
- Purpose: Model GDPdU schema objects in memory
- Location: `src/gdpdu_schema.cpp` and `src/include/gdpdu_schema.hpp`
- Contains: Enum types (GdpduType), data structures (ColumnDef, TableDef, GdpduSchema), type conversion utilities
- Depends on: C++ standard library
- Used by: All parsing and creation layers

**Table Creation Layer:**
- Purpose: Generate and execute CREATE TABLE statements
- Location: `src/gdpdu_table_creator.cpp` and `src/include/gdpdu_table_creator.hpp`
- Contains: SQL generation from schema, table creation execution
- Depends on: DuckDB connection, GdpduSchema
- Used by: Import orchestration layer

**Data Parsing Layer:**
- Purpose: Parse GDPdU .txt data files with type conversion
- Location: `src/gdpdu_data_parser.cpp` and `src/include/gdpdu_data_parser.hpp`
- Contains: CSV line parsing, German number/date conversion, field-by-field conversion
- Depends on: GdpduSchema
- Used by: Import orchestration layer (currently unused; data loading handled by DuckDB's read_csv)

## Data Flow

**Import Execution Flow:**

1. **User Input:** `SELECT * FROM gdpdu_import('path/to/gdpdu/data')`
2. **Extension Binding:** `GdpduImportBind()` validates arguments, defines return schema (table_name, row_count, status)
3. **Import Init:** `GdpduImportInit()` invokes `import_gdpdu()` orchestrator
4. **Schema Parse:** `parse_index_xml()` reads `index.xml`, extracts table definitions with column metadata
5. **Name Conversion:** Column names converted from PascalCase/Description to snake_case
6. **Table Creation:** `create_tables()` executes `CREATE TABLE` for each table in the schema
7. **Data Loading:** For each table:
   - Build SQL: `INSERT INTO table SELECT [type-cast columns] FROM read_csv('file.txt', ...)`
   - DuckDB's read_csv parses semicolon-delimited data
   - Column type casting: German decimals (`,` → `.`), German dates (DD.MM.YYYY → DATE)
   - Null handling for empty/whitespace values
8. **Result Generation:** `GdpduImportScan()` outputs one row per table with status

**State Management:**

- **Bind Phase:** `GdpduImportBindData` holds directory path and column name field preference
- **Init Phase:** `GdpduImportGlobalState` holds result vector and current scan position
- **Scan Phase:** Streaming result rows back to DuckDB, tracking completion state

## Key Abstractions

**GdpduSchema (Root Abstract Model):**
- Purpose: Represents complete GDPdU dataset structure
- Examples: `src/include/gdpdu_schema.hpp`
- Pattern: Domain model - pure data structures with helper functions, no side effects

**TableDef (Table Abstraction):**
- Purpose: Single table definition with metadata
- Contains: name, URL (data file), description, encoding, locale settings (decimal/grouping symbols), columns, primary keys
- Pattern: Value type - completely self-contained, immutable after parsing

**ColumnDef (Column Abstraction):**
- Purpose: Single column definition with type information
- Contains: name, GdpduType, precision (for decimals), primary key flag
- Pattern: Value type

**GdpduType (Type Enum):**
- Purpose: Represents GDPdU data types
- Values: AlphaNumeric (→ VARCHAR), Numeric (→ BIGINT or DECIMAL), Date (→ DATE)
- Pattern: Enumeration with type mapping

## Entry Points

**SQL Function Entry Point:**
- Location: `src/gdpdu_extension.cpp` (lines 115-169)
- Triggers: User executes `SELECT * FROM gdpdu_import(...)`
- Responsibilities: Register table function with DuckDB, define multiple function overloads

**C++ Extension Entry Point:**
- Location: `src/gdpdu_extension.cpp` (lines 163-169)
- Triggers: DuckDB loads extension (DUCKDB_CPP_EXTENSION_ENTRY macro)
- Responsibilities: Instantiate GdpduExtension, call Load() to register functions

**Orchestration Entry Point:**
- Location: `src/gdpdu_importer.cpp` `import_gdpdu()`
- Triggers: From GdpduImportInit() when user executes the SQL function
- Responsibilities: Coordinate parsing → creating → loading pipeline

## Error Handling

**Strategy:** Try-catch at boundaries; return error status to user

**Patterns:**

- **XML Parse Errors:** Caught in `import_gdpdu()`, returned as single result row with table_name="(schema)" and status containing error message
- **File I/O Errors:** Caught in data loading SQL execution, returned as result row with status="Load failed: ..."
- **SQL Execution Errors:** DuckDB QueryResult checked with `HasError()`, error text embedded in result status
- **Type Conversion Errors:** Handled by DuckDB's CAST during INSERT (strptime, REPLACE functions)

**Result Structure:** All operations complete with status reporting rather than throwing exceptions to user:
```cpp
struct ImportResult {
    std::string table_name;
    int64_t row_count;
    std::string status;  // "OK" or error message
};
```

## Cross-Cutting Concerns

**Logging:** Not implemented; errors reported through ImportResult status field

**Validation:**
- XML schema structure validated during parsing (missing DataSet/Media elements throw)
- Column count validation during CSV data loading via DuckDB's read_csv
- Type conversion validated by DuckDB CAST operations

**Path Handling:**
- Windows backslashes normalized to forward slashes in `normalize_path()` (used in both parser and importer)
- Path joining with `join_path()` handles edge cases (empty directories, trailing slashes)

**Name Normalization:**
- `to_snake_case()` converts all column names from source (Name or Description field) to snake_case
- Handles PascalCase, camelCase, German descriptions with special characters
- German umlauts (ä, ö, ü, ß) transliterated to ASCII (a, o, u, ss)
- Non-alphanumeric characters → underscores (collapsed, not trailing)

**SQL Injection Prevention:**
- Table/column names quoted with double quotes: `CREATE TABLE "name"`
- File paths escaped for SQL string literals: single quotes doubled
- Column names used directly in quoted identifiers, not string interpolation

**Encoding Handling:**
- GDPdU files detected as UTF-8 via XML `<UTF8>` element flag
- German locale defaults: decimal symbol=`,`, digit grouping=`.`
- Explicitly configurable per table via XML DecimalSymbol/DigitGroupingSymbol elements

---

*Architecture analysis: 2026-01-24*
