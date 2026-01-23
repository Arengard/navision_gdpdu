# GDPdU Extension for DuckDB

A DuckDB extension for importing GDPdU (Grundsätze zum Datenzugriff und zur Prüfbarkeit digitaler Unterlagen) exports - the German standard format for tax audit data.

## Installation

Build the extension from source using the DuckDB extension build system.

## Usage

### Basic Import

Import all tables from a GDPdU export directory:

```sql
SELECT * FROM gdpdu_import('C:\path\to\gdpdu\data');
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
SELECT * FROM gdpdu_import('C:\path\to\gdpdu\data');
SELECT * FROM gdpdu_import('C:\path\to\gdpdu\data', 'Name');

-- Use "Description" field for column names (German labels)
SELECT * FROM gdpdu_import('C:\path\to\gdpdu\data', 'Description');
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
-- Import GDPdU data
SELECT * FROM gdpdu_import('C:\gdpdu\Gdpdu Data');

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

## License

MIT
