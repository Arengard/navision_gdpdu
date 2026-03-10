# GDPdU Navision Extension for DuckDB

A DuckDB extension for importing GDPdU (Grundsätze zum Datenzugriff und zur Prüfbarkeit digitaler Unterlagen) exports from Microsoft Dynamics NAV (Navision).

## Installation

```sql
LOAD '/path/to/gdpdu.duckdb_extension';
```

## Functions

### `import_gdpdu_navision(path [, column_source])`

Imports all tables from a GDPdU Navision export directory by parsing its `index.xml` and loading the `.txt` data files.

**Parameters:**

| # | Parameter | Type | Required | Default | Description |
|---|-----------|------|----------|---------|-------------|
| 1 | `path` | VARCHAR | Yes | — | Path to the directory containing `index.xml` and `.txt` data files |
| 2 | `column_source` | VARCHAR | No | `'Name'` | Which XML element to use for column names: `'Name'` (field identifiers) or `'Description'` (German labels) |

**Returns:**

| Column | Type | Description |
|--------|------|-------------|
| `table_name` | VARCHAR | Name of the imported table |
| `row_count` | BIGINT | Number of rows imported |
| `status` | VARCHAR | `"OK"` or error message |

**Example:**

```sql
-- Import using field names (default)
SELECT * FROM import_gdpdu_navision('/data/gdpdu_export');

-- Import using German description labels
SELECT * FROM import_gdpdu_navision('/data/gdpdu_export', 'Description');

-- Result:
-- ┌──────────────┬───────────┬────────┐
-- │  table_name  │ row_count │ status │
-- ├──────────────┼───────────┼────────┤
-- │ LandRegion   │       245 │ OK     │
-- │ Sachkonto    │      1823 │ OK     │
-- │ Sachposten   │     45672 │ OK     │
-- └──────────────┴───────────┴────────┘

-- Query imported data
SELECT * FROM Sachkonto LIMIT 10;
```

**Notes:**
- Column names are converted to `snake_case` (e.g. `EUCountryRegionCode` → `eu_country_region_code`)
- German umlauts are transliterated: `ä`→`a`, `ö`→`o`, `ü`→`u`, `ß`→`ss`
- Supports German number format (comma decimal, dot grouping) and date format (`DD.MM.YYYY`)
- Respects `<Range><From>` elements to skip header lines in CSV files
- Type mapping: `AlphaNumeric` → `VARCHAR`, `Numeric` → `BIGINT`/`DECIMAL`, `Date` → `DATE`

---

### `import_folder(path [, file_type [, options]])`

Imports all files of a given type from a directory into separate DuckDB tables.

**Parameters:**

| # | Parameter | Type | Required | Default | Description |
|---|-----------|------|----------|---------|-------------|
| 1 | `path` | VARCHAR | Yes | — | Path to the folder containing files to import |
| 2 | `file_type` | VARCHAR | No | `'csv'` | File type to import: `'csv'`, `'parquet'`, `'xlsx'`/`'excel'`, `'json'`, `'tsv'` |
| 3 | `options` | VARCHAR | No | — | DuckDB read options passed through to the underlying read function (e.g. `'all_varchar=true'`, `'delimiter='';'''`) |

**Returns:**

| Column | Type | Description |
|--------|------|-------------|
| `table_name` | VARCHAR | Normalized table name (snake_case, from filename) |
| `file_name` | VARCHAR | Original filename |
| `row_count` | BIGINT | Number of rows imported |
| `column_count` | INTEGER | Number of columns |
| `status` | VARCHAR | `"OK"` or error message |

**Example:**

```sql
-- Import all CSV files (default)
SELECT * FROM import_folder('/data/reports/');

-- Import all Parquet files
SELECT * FROM import_folder('/data/reports/', 'parquet');

-- Import Excel files, forcing all columns to text
SELECT * FROM import_folder('/data/reports/', 'xlsx', 'all_varchar=true');

-- Import CSV with custom delimiter
SELECT * FROM import_folder('/data/reports/', 'csv', 'delimiter='';''');

-- Result:
-- ┌──────────────┬──────────────────┬───────────┬──────────────┬────────┐
-- │  table_name  │   file_name      │ row_count │ column_count │ status │
-- ├──────────────┼──────────────────┼───────────┼──────────────┼────────┤
-- │ sales_data   │ Sales-Data.csv   │      1523 │           12 │ OK     │
-- │ customers    │ Customers.csv    │       456 │            8 │ OK     │
-- └──────────────┴──────────────────┴───────────┴──────────────┴────────┘

-- Query imported tables
SELECT * FROM sales_data LIMIT 10;
```

**Notes:**
- Table and column names are automatically converted to `snake_case`
- Excel files automatically retry with `all_varchar=true` if type detection fails on mixed-type columns
- Existing tables with the same name are dropped before import
- If one file fails, the remaining files still import

---

### `import_xml_data(path [, parser_type])`

Generic XML import using a configurable parser system.

**Parameters:**

| # | Parameter | Type | Required | Default | Description |
|---|-----------|------|----------|---------|-------------|
| 1 | `path` | VARCHAR | Yes | — | Path to the directory containing XML and data files |
| 2 | `parser_type` | VARCHAR | No | `'gdpdu'` | Parser to use: `'gdpdu'` (GDPdU Navision format) or `'generic'` (custom XML formats) |

**Returns:** Same as `import_gdpdu_navision`.

**Example:**

```sql
-- Default GDPdU parser
SELECT * FROM import_xml_data('/data/gdpdu_export');

-- Generic parser for custom XML
SELECT * FROM import_xml_data('/data/custom_xml/', 'generic');
```

---

### `import_gdpdu_nextcloud(url, username, password)`

Batch-imports GDPdU exports from a Nextcloud server via WebDAV. Downloads all `.zip` files from the folder, extracts them, and imports the GDPdU data. Tables are prefixed with the zip filename to avoid collisions.

**Parameters:**

| # | Parameter | Type | Required | Description |
|---|-----------|------|----------|-------------|
| 1 | `nextcloud_url` | VARCHAR | Yes | Full Nextcloud WebDAV path (e.g. `https://cloud.example.com/remote.php/dav/files/user/exports/`) |
| 2 | `username` | VARCHAR | Yes | Nextcloud username |
| 3 | `password` | VARCHAR | Yes | Nextcloud password |

**Returns:**

| Column | Type | Description |
|--------|------|-------------|
| `table_name` | VARCHAR | Prefixed table name (e.g. `Export_2024_Sachkonto`) |
| `row_count` | BIGINT | Number of rows imported (0 on error) |
| `status` | VARCHAR | `"OK"` or error message |
| `source_zip` | VARCHAR | Original zip filename |

**Example:**

```sql
SELECT * FROM import_gdpdu_nextcloud(
    'https://nextcloud.mycompany.com/remote.php/dav/files/accounting/gdpdu/',
    'accounting_user',
    'secure_password'
);

-- Result:
-- ┌───────────────────────────┬───────────┬────────┬──────────────────┐
-- │        table_name         │ row_count │ status │    source_zip    │
-- ├───────────────────────────┼───────────┼────────┼──────────────────┤
-- │ Export_2024_Sachkonto     │      1823 │ OK     │ Export 2024.zip  │
-- │ Export_2024_Sachposten    │     45672 │ OK     │ Export 2024.zip  │
-- │ Export_2023_Sachkonto     │      1650 │ OK     │ Export 2023.zip  │
-- └───────────────────────────┴───────────┴────────┴──────────────────┘

-- Query imported tables
SELECT * FROM Export_2024_Sachposten LIMIT 10;
```

**Notes:**
- Uses HTTP Basic Auth; supports self-signed SSL certificates
- If one zip fails, the remaining zips still import

---

### `export_gdpdu(path, table_name)`

Exports a DuckDB table to GDPdU-compliant format (`index.xml` + semicolon-delimited `.txt` file).

**Parameters:**

| # | Parameter | Type | Required | Description |
|---|-----------|------|----------|-------------|
| 1 | `path` | VARCHAR | Yes | Export directory (created automatically if it doesn't exist) |
| 2 | `table_name` | VARCHAR | Yes | Name of the DuckDB table to export |

**Returns:**

| Column | Type | Description |
|--------|------|-------------|
| `table_name` | VARCHAR | Name of the exported table |
| `file_path` | VARCHAR | Export directory path |
| `row_count` | BIGINT | Number of rows exported |
| `status` | VARCHAR | `"OK"` or error message |

**Example:**

```sql
SELECT * FROM export_gdpdu('/data/gdpdu_export', 'Sachkonto');

-- Result:
-- ┌──────────────┬──────────────────────┬───────────┬────────┐
-- │  table_name  │     file_path        │ row_count │ status │
-- ├──────────────┼──────────────────────┼───────────┼────────┤
-- │ Sachkonto    │ /data/gdpdu_export   │      1823 │ OK     │
-- └──────────────┴──────────────────────┴───────────┴────────┘

-- Creates: /data/gdpdu_export/index.xml
--          /data/gdpdu_export/Sachkonto.txt
```

**Notes:**
- Output uses German formatting: comma decimal separator, `DD.MM.YYYY` dates, semicolon delimiter
- Type mapping: `VARCHAR`/`TEXT` → `AlphaNumeric`, `BIGINT`/`INTEGER` → `Numeric` (precision=0), `DECIMAL` → `Numeric` (with precision), `DOUBLE`/`FLOAT` → `Numeric` (precision=2), `DATE` → `Date`
- Existing files are overwritten

## License

MIT
