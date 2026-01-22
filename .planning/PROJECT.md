# GDPdU DuckDB Extension

## What This Is

A DuckDB C++ extension that imports GDPdU (Grundsätze zum Datenzugriff und zur Prüfbarkeit digitaler Unterlagen) tax audit data exports into DuckDB. Users call a table function with a directory path, and all tables defined in the `index.xml` are created and populated in the database.

## Core Value

Read any valid GDPdU export into DuckDB with a single function call — no manual schema definition or data wrangling required.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] Table function `gdpdu_import(path)` that triggers the full import workflow
- [ ] Parse `index.xml` to extract table definitions (names, columns, types, primary keys)
- [ ] Map GDPdU types to DuckDB types (AlphaNumeric→VARCHAR, Numeric→DECIMAL with precision, Date→DATE)
- [ ] Read semicolon-delimited `.txt` files with German locale (comma decimal separator, dot digit grouping, DD.MM.YYYY dates)
- [ ] Create tables in default schema with correct column definitions
- [ ] Overwrite existing tables if they already exist (DROP + CREATE)
- [ ] Return summary result set (table name, row count, status) from the import function
- [ ] Handle UTF-8 encoded files (as specified in index.xml)
- [ ] Support composite primary keys (multiple VariablePrimaryKey elements)

### Out of Scope

- Fixed-length record format — GDPdU supports this but real-world exports use VariableLength (semicolon-delimited)
- Foreign key relationship creation — index.xml doesn't define relationships, just primary keys
- Incremental/delta imports — always full overwrite for simplicity
- Non-file media types — GDPdU spec allows CD/DVD media references, we assume local filesystem paths

## Context

**GDPdU Standard:**
- German regulation for digital tax audit data access
- Defines XML schema (`gdpdu-01-09-2004.dtd`) for describing exported data
- Used by ERP systems (SAP, Microsoft Dynamics, DATEV, etc.) to export audit-ready data

**Data Format:**
- `index.xml` — Schema definition with table/column metadata
- `.txt` files — Actual data, semicolon-delimited, German number/date formatting
- Typical exports contain 10-20 tables (GL accounts, entries, customers, vendors, fixed assets, VAT)

**DuckDB Extension Ecosystem:**
- C++ extensions using DuckDB's extension API
- Table functions for data import (similar to `read_csv`, `read_parquet`)
- Build system uses CMake with DuckDB extension template

## Constraints

- **Language**: C++ — Required for DuckDB extensions
- **Build**: CMake with DuckDB extension template
- **Compatibility**: DuckDB 1.0+ extension API
- **Platform**: Windows primary (paths like `c:\gdpdu\`), but cross-platform where possible
- **Dependencies**: Minimal — XML parsing (consider pugixml or built-in), standard C++ for file I/O

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Table function over procedure | Returns useful summary, follows DuckDB patterns (read_csv, etc.) | — Pending |
| Overwrite existing tables | Simpler mental model, matches "re-import" workflow | — Pending |
| Default schema only | Keeps queries simple, avoids schema prefix everywhere | — Pending |
| German locale hardcoded | GDPdU is German standard, all exports use this format | — Pending |

---
*Last updated: 2026-01-22 after initialization*
