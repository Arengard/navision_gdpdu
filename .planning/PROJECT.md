# GDPdU DuckDB Extension

## What This Is

A DuckDB C++ extension that imports GDPdU (Grundsätze zum Datenzugriff und zur Prüfbarkeit digitaler Unterlagen) tax audit data exports into DuckDB. Supports both local import from a directory path and batch import from Nextcloud via WebDAV — download zips, extract, and import with table name prefixing.

## Core Value

Read any valid GDPdU export into DuckDB with a single function call — no manual schema definition or data wrangling required.

## Requirements

### Validated

- ✓ Table function `gdpdu_import(path)` — v1.0
- ✓ Parse `index.xml` to extract table/column definitions — v1.0
- ✓ Map GDPdU types to DuckDB types — v1.0
- ✓ Read semicolon-delimited `.txt` files with German locale — v1.0
- ✓ Create tables with correct column definitions — v1.0
- ✓ Overwrite existing tables (DROP + CREATE) — v1.0
- ✓ Return summary result set (table name, row count, status) — v1.0
- ✓ Handle UTF-8 encoded files — v1.0
- ✓ Support composite primary keys — v1.0
- ✓ Connect to Nextcloud via WebDAV with username/password — v1.1
- ✓ List all `.zip` files in a Nextcloud folder — v1.1
- ✓ Download and extract zip files — v1.1
- ✓ New `import_gdpdu_nextcloud(url, user, pass)` table function — v1.1
- ✓ Prefix table names with zip filename to avoid collisions — v1.1
- ✓ Skip failed zips and continue — report failures in result set — v1.1
- ✓ Return summary with table name, row count, status, and source zip — v1.1

### Active

(None — no active milestone)

### Out of Scope

- Fixed-length record format — GDPdU supports this but real-world exports use VariableLength
- Foreign key relationship creation — index.xml doesn't define relationships
- Incremental/delta imports — always full overwrite for simplicity
- Nextcloud OAuth/SSO — WebDAV with username+password or app tokens is sufficient
- Recursive folder traversal — only list zips in the specified folder
- Streaming zip extraction — download fully then extract (simpler, reliable)
- Data validation — extension imports as-is, validation is user's responsibility
- GUI/CLI wrapper — extension only, tooling built separately if needed

## Context

Shipped v1.0 + v1.1 with ~3,400 LOC C++.
Tech stack: DuckDB extension API, CMake, pugixml, cpp-httplib (bundled), duckdb_miniz (bundled).
Two user-facing functions: `import_gdpdu_navision(path)` for local, `import_gdpdu_nextcloud(url, user, pass)` for cloud.
Extension requires `-unsigned` flag to load (expected for unsigned extensions).

**GDPdU Standard:**
- German regulation for digital tax audit data access
- XML schema (`gdpdu-01-09-2004.dtd`) describes exported data
- Used by ERP systems (SAP, Microsoft Dynamics, DATEV, etc.)

**Data Format:**
- `index.xml` — Schema definition with table/column metadata
- `.txt` files — Semicolon-delimited, German number/date formatting
- Typical exports contain 10-20 tables

## Constraints

- **Language**: C++ — Required for DuckDB extensions
- **Build**: CMake with DuckDB extension template, CI pipeline on GitHub
- **Compatibility**: DuckDB 1.0+ extension API (currently built against v1.4.4)
- **Platform**: Windows primary, cross-platform where possible
- **Dependencies**: Minimal — pugixml for XML, cpp-httplib and miniz bundled with DuckDB

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Table function over procedure | Returns useful summary, follows DuckDB patterns | ✓ Good |
| Overwrite existing tables | Simpler mental model, matches "re-import" workflow | ✓ Good |
| Default schema only | Keeps queries simple, avoids schema prefix everywhere | ✓ Good |
| German locale hardcoded | GDPdU is German standard, all exports use this format | ✓ Good |
| pugixml for XML | Simple DOM API, handles GDPdU DTD gracefully | ✓ Good |
| Batch INSERT 1000 rows | Balances memory use vs. SQL statement size | ✓ Good |
| Separate function for Nextcloud | Keeps local and cloud import concerns separate | ✓ Good |
| Zip-name prefix for tables | Avoids collisions when importing multiple exports | ✓ Good |
| Skip-and-continue on errors | Most practical for batch import workflows | ✓ Good |
| Use duckdb_httplib directly | DuckDB's HTTPClient wrapper doesn't support PROPFIND | ✓ Good |
| Disable SSL cert verification | Common for internal Nextcloud with self-signed certs | ⚠️ Revisit |
| Result struct error handling | No exceptions thrown, better for batch processing | ✓ Good |
| Use duckdb_miniz for zip | Already bundled with DuckDB, no additional deps | ✓ Good |
| Sanitize zip filenames | Ensures valid SQL identifiers, avoids collisions | ✓ Good |
| Search for index.xml in zip | Handles both root-level and nested directory structures | ✓ Good |

---
*Last updated: 2026-02-13 after v1.1 milestone*
