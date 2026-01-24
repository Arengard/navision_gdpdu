# GDPdU Navision Extension for DuckDB

A DuckDB extension for importing GDPdU (Grundsätze zum Datenzugriff und zur Prüfbarkeit digitaler Unterlagen) exports from Microsoft Dynamics NAV (Navision) - the German standard format for tax audit data.

## Installation

Build the extension from source using the DuckDB extension build system:

```bash
make release
```

Or manually:

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Extension Location

After building, the extension file is located at:

- **Main build location**: `build/_deps/duckdb-build/extension/gdpdu/gdpdu.duckdb_extension`
- **Repository location**: `build/repository/v1.4.3/osx_arm64/gdpdu.duckdb_extension` (platform-specific)

### Loading the Extension

Load the extension in DuckDB:

```sql
LOAD 'build/_deps/duckdb-build/extension/gdpdu/gdpdu.duckdb_extension';
```

Or use the absolute path:

```sql
LOAD '/full/path/to/navision_gdpdu/build/_deps/duckdb-build/extension/gdpdu/gdpdu.duckdb_extension';
```

## Usage

### GDPdU Navision Import

Import all tables from a GDPdU export directory:

```sql
SELECT * FROM import_gdpdu_navision('/path/to/gdpdu/data');
```

This will:
1. Parse the `index.xml` file in the specified directory
2. Create tables in DuckDB based on the schema definitions
3. Load data from the `.txt` files into the tables
4. Return a summary of imported tables with row counts and status

### Column Name Source

By default, column names are taken from the `<Name>` element in the XML. You can optionally use the `<Description>` element instead:

```sql
-- Use "Name" field for column names (default)
SELECT * FROM import_gdpdu_navision('/path/to/gdpdu/data');
SELECT * FROM import_gdpdu_navision('/path/to/gdpdu/data', 'Name');

-- Use "Description" field for column names (German labels)
SELECT * FROM import_gdpdu_navision('/path/to/gdpdu/data', 'Description');
```

### Column Name Conversion

All column names are automatically converted to `snake_case` for consistency:

| Source Type | Original | Converted |
|-------------|----------|-----------|
| PascalCase | `EUCountryRegionCode` | `eu_country_region_code` |
| camelCase | `accountType` | `account_type` |
| German description | `EU-Laender-/Regionscode` | `eu_laender_regionscode` |
| With spaces | `Saldo bis Datum` | `saldo_bis_datum` |
| German umlauts | `Größe` | `grosse` |

German umlauts are transliterated: `ä`→`a`, `ö`→`o`, `ü`→`u`, `ß`→`ss`

### Return Value

The function returns a table with the following columns:

| Column | Type | Description |
|--------|------|-------------|
| `table_name` | VARCHAR | Name of the imported table |
| `row_count` | BIGINT | Number of rows imported |
| `status` | VARCHAR | "OK" or error message |

### Example

```sql
-- Import GDPdU data from Navision export
SELECT * FROM import_gdpdu_navision('/data/gdpdu_export');

-- Result:
-- ┌──────────────┬───────────┬────────┐
-- │  table_name  │ row_count │ status │
-- ├──────────────┼───────────┼────────┤
-- │ LandRegion   │       245 │ OK     │
-- │ Sachkonto    │      1823 │ OK     │
-- │ Sachposten   │     45672 │ OK     │
-- └──────────────┴───────────┴────────┘

-- Query the imported data
SELECT * FROM Sachkonto LIMIT 10;
```

## Supported Data Types

The extension handles the following GDPdU data types:

- **AlphaNumeric** → `VARCHAR`
- **Numeric** → `BIGINT` (integer) or `DECIMAL(18,n)` (with precision)
- **Date** → `DATE` (expects German format DD.MM.YYYY)

German number formatting is automatically handled:
- Decimal separator: `,` (comma)
- Digit grouping: `.` (dot)

### CSV File Handling

The extension automatically handles CSV files with header or metadata lines that need to be skipped. When the XML contains a `<Range><From>` element, the extension will skip the specified number of lines at the beginning of the CSV file.

For example, if the XML specifies:
```xml
<Range><From>3</From></Range>
```

The extension will skip the first 2 lines (lines 1 and 2) and start reading data from line 3. This is useful for CSV files that contain:
- Header rows
- Metadata or comments
- Empty lines before the actual data

The extension automatically:
1. Reads the `<Range><From>` value from the XML
2. Calculates the number of lines to skip (`From - 1`)
3. Passes the `skip` parameter to DuckDB's CSV reader
4. Ensures the correct number of columns are read

### Robust CSV Parsing

The extension uses robust CSV parsing settings to handle various file formats:

- **Auto-detection disabled**: Uses explicit delimiter and column definitions instead of auto-detection
- **Strict mode disabled**: Allows reading rows that don't strictly comply with CSV standards
- **Null padding enabled**: Automatically pads missing columns with NULL values
- **Explicit delimiter**: Uses semicolon (`;`) delimiter as specified in GDPdU format

These settings ensure reliable parsing even when CSV files have:
- Inconsistent formatting
- Missing values
- Extra or missing columns in some rows
- Encoding issues

## Folder Import

The extension includes a general-purpose `import_folder` function that can import all files of a specified type from a directory into DuckDB tables.

### Basic Usage

Import all files of a specific type from a folder:

```sql
-- Import all CSV files (default)
SELECT * FROM import_folder('/path/to/folder/');

-- Import all Parquet files
SELECT * FROM import_folder('/path/to/folder/', 'parquet');

-- Import all Excel files
SELECT * FROM import_folder('/path/to/folder/', 'xlsx');
```

### Supported File Types

The function supports multiple file formats:

| File Type | Extensions | DuckDB Function |
|-----------|------------|-----------------|
| `csv` (default) | `.csv`, `.txt` | `read_csv` |
| `parquet` | `.parquet` | `read_parquet` |
| `xlsx` / `excel` | `.xlsx`, `.xls` | `read_excel` |
| `json` | `.json`, `.jsonl` | `read_json` |
| `tsv` | `.tsv` | `read_csv` (with tab delimiter) |

### Table and Column Naming

- **Table names**: Derived from filenames (without extension), converted to `snake_case`
  - Example: `MyData.csv` → table `my_data`
  - Example: `Sales-Report.xlsx` → table `sales_report`
  
- **Column names**: Automatically normalized to `snake_case`
  - Example: `Customer Name` → `customer_name`
  - Example: `TotalAmount` → `total_amount`
  - Example: `Order Date` → `order_date`

### Return Value

The function returns a table with import results:

| Column | Type | Description |
|--------|------|-------------|
| `table_name` | VARCHAR | Normalized table name (snake_case) |
| `file_name` | VARCHAR | Original filename |
| `row_count` | BIGINT | Number of rows imported |
| `column_count` | INTEGER | Number of columns |
| `status` | VARCHAR | "OK" or error message |

### Example

```sql
-- Import all CSV files from a folder
SELECT * FROM import_folder('/Users/ramonljevo/Downloads/data/');

-- Result:
-- ┌──────────────┬──────────────────┬───────────┬──────────────┬────────┐
-- │  table_name  │   file_name      │ row_count │ column_count │ status │
-- ├──────────────┼──────────────────┼───────────┼──────────────┼────────┤
-- │ sales_data   │ Sales-Data.csv   │      1523 │           12 │ OK     │
-- │ customers    │ Customers.csv    │       456 │            8 │ OK     │
-- │ products     │ Products.csv     │      2341 │           15 │ OK     │
-- └──────────────┴──────────────────┴───────────┴──────────────┴────────┘

-- Query the imported tables
SELECT * FROM sales_data LIMIT 10;
SELECT * FROM customers WHERE country = 'Germany';
```

### Features

- **Automatic file detection**: Scans the folder and imports all matching files
- **Auto-type detection**: Uses DuckDB's native auto-detection for data types
- **Header support**: Automatically detects and uses headers when present
- **Error handling**: Continues importing other files even if one fails
- **Table replacement**: Drops existing tables before importing (idempotent)

### Use Cases

- Import multiple CSV files from a data export
- Load all Parquet files from a data lake directory
- Import Excel reports from a shared folder
- Bulk import JSON files from an API export

## Generic XML Import

The extension includes a flexible XML import system that can handle different XML formats through a parser factory system.

### Basic Generic Import

Use the `import_xml_data` function to import XML data with configurable parsers:

```sql
-- Use default GDPdU parser
SELECT * FROM import_xml_data('/path/to/gdpdu/data');

-- Explicitly specify parser type
SELECT * FROM import_xml_data('/path/to/gdpdu/data', 'gdpdu');

-- Use generic parser for custom XML formats
SELECT * FROM import_xml_data('/path/to/xml/data', 'generic');
```

### Available Parsers

The extension includes two built-in parsers:

1. **`gdpdu`** (default) - Specialized parser for GDPdU Navision format
   - Optimized for GDPdU XML structure
   - Handles German number formatting
   - Supports both "Name" and "Description" column name sources
   - Automatically handles CSV files with `<Range><From>` elements for skipping header lines

2. **`generic`** - Flexible parser for custom XML formats
   - Configurable element paths
   - Customizable type mappings
   - Supports various XML schemas

### Parser Factory System

The extension uses a factory pattern to manage XML parsers, making it easy to add support for new XML formats:

- **Extensible**: Add new parsers by implementing the `XmlParser` interface
- **Configurable**: Specify XML element paths, type mappings, and CSV settings
- **Backward Compatible**: Original `import_gdpdu_navision` function still works

### When to Use Each Function

- **`import_gdpdu_navision`**: Use for standard GDPdU Navision exports (simpler API)
- **`import_xml_data`**: Use when you need:
  - Different parser types
  - Custom XML formats
  - Future extensibility

### Example: Using Generic Parser

```sql
-- Import with generic parser (for custom XML formats)
SELECT * FROM import_xml_data('/path/to/custom/xml/data', 'generic');
```

The generic parser can be configured to handle different XML schemas by specifying:
- Root element paths
- Table and column element names
- Type mappings
- CSV delimiter and formatting options

## GDPdU Export

The extension includes an `export_gdpdu` function that exports DuckDB tables back to GDPdU-compliant format for tax audit purposes.

### Basic Usage

Export a table to GDPdU format:

```sql
SELECT * FROM export_gdpdu('/path/to/export', 'table_name');
```

This will:
1. Create the export directory if it doesn't exist
2. Generate an `index.xml` file with the table schema
3. Export the table data to `{table_name}.txt` with proper GDPdU formatting
4. Return export results with status information

### Export Format

The export function creates a GDPdU-compliant export with:

- **index.xml**: XML schema file describing the table structure
  - Table name and description
  - Column definitions with types (AlphaNumeric, Numeric, Date)
  - Data type precision for numeric columns
  - UTF-8 encoding indicator
  - German number format settings (comma decimal, dot thousands)

- **{table_name}.txt**: Data file with:
  - Semicolon-delimited format (`;`)
  - No header row
  - German number format: comma as decimal separator (e.g., `23,50`)
  - German date format: `DD.MM.YYYY` (e.g., `24.01.2025`)
  - UTF-8 encoding

### Type Inference

The export function automatically infers GDPdU types from DuckDB column types:

| DuckDB Type | GDPdU Type | Notes |
|-------------|------------|-------|
| `VARCHAR`, `TEXT`, `CHAR` | `AlphaNumeric` | Text data |
| `BIGINT`, `INTEGER`, `INT` | `Numeric` | Integer (precision=0) |
| `DECIMAL`, `NUMERIC` | `Numeric` | Decimal with precision |
| `DOUBLE`, `FLOAT`, `REAL` | `Numeric` | Floating point (precision=2) |
| `DATE` | `Date` | Date values |

### Return Value

The function returns a table with the following columns:

| Column | Type | Description |
|--------|------|-------------|
| `table_name` | VARCHAR | Name of the exported table |
| `file_path` | VARCHAR | Export directory path |
| `row_count` | BIGINT | Number of rows exported |
| `status` | VARCHAR | "OK" or error message |

### Example

```sql
-- Export a table to GDPdU format
SELECT * FROM export_gdpdu('/data/gdpdu_export', 'Sachkonto');

-- Result:
-- ┌──────────────┬──────────────────────┬───────────┬────────┐
-- │  table_name  │     file_path        │ row_count │ status │
-- ├──────────────┼──────────────────────┼───────────┼────────┤
-- │ Sachkonto    │ /data/gdpdu_export   │      1823 │ OK     │
-- └──────────────┴──────────────────────┴───────────┴────────┘

-- The export creates:
-- /data/gdpdu_export/index.xml
-- /data/gdpdu_export/Sachkonto.txt
```

### Use Cases

- **Tax Audit Compliance**: Export processed data back to GDPdU format for tax authorities
- **Data Archiving**: Create GDPdU-compliant archives of financial data
- **Data Exchange**: Export data in standard format for sharing with auditors
- **Backup**: Create GDPdU-formatted backups of important tables

### Notes

- The export directory will be created automatically if it doesn't exist
- Existing files in the export directory will be overwritten
- Column names are preserved as-is (not converted from snake_case)
- Primary keys are not detected automatically (all columns exported as VariableColumn)
- The export maintains German number and date formatting standards

## Architecture

### Parser System

The extension implements a flexible parser architecture:

```
XmlParser (interface)
├── GdpduXmlParser (GDPdU-specific)
└── GenericXmlParser (configurable)
```

New parsers can be added by:
1. Implementing the `XmlParser` interface
2. Registering with `XmlParserFactory`
3. Using via `import_xml_data(path, 'parser_type')`

### Extension Functions

| Function | Parser | Use Case |
|----------|--------|----------|
| `import_gdpdu_navision(path)` | GDPdU | Standard Navision exports |
| `import_gdpdu_navision(path, field)` | GDPdU | Navision with column name source |
| `import_xml_data(path)` | GDPdU (default) | Generic import (GDPdU format) |
| `import_xml_data(path, parser)` | Configurable | Custom XML formats |
| `import_folder(path)` | N/A | Import all CSV files from folder |
| `import_folder(path, file_type)` | N/A | Import all files of specified type from folder |
| `export_gdpdu(path, table_name)` | N/A | Export table to GDPdU format |

## License

MIT
