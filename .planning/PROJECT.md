# GDPdU DuckDB Extension

## What This Is

A DuckDB C++ extension that imports GDPdU (Grundsätze zum Datenzugriff und zur Prüfbarkeit digitaler Unterlagen) tax audit data exports into DuckDB. Users call a table function with a directory path, and all tables defined in the `index.xml` are created and populated in the database.

## Core Value

Read any valid GDPdU export into DuckDB with a single function call — no manual schema definition or data wrangling required.

## Current Milestone: v1.1 Nextcloud Batch Import

**Goal:** Import multiple GDPdU exports from a Nextcloud folder via WebDAV — download zips, extract, and batch-import with table name prefixing.

**Target features:**
- New `gdpdu_import_nextcloud(url, user, pass)` table function
- WebDAV client to list and download zip files from Nextcloud
- Zip extraction to access GDPdU exports
- Batch import across all zips with zip-name-prefixed table names
- Resilient processing — skip failed zips, report in results

## Requirements

### Validated

- ✓ Table function `gdpdu_import(path)` — v1.0 Phase 5
- ✓ Parse `index.xml` to extract table/column definitions — v1.0 Phase 2
- ✓ Map GDPdU types to DuckDB types — v1.0 Phase 3
- ✓ Read semicolon-delimited `.txt` files with German locale — v1.0 Phase 4
- ✓ Create tables with correct column definitions — v1.0 Phase 3
- ✓ Overwrite existing tables (DROP + CREATE) — v1.0 Phase 3
- ✓ Return summary result set (table name, row count, status) — v1.0 Phase 5
- ✓ Handle UTF-8 encoded files — v1.0 Phase 4
- ✓ Support composite primary keys — v1.0 Phase 2

### Active

- [ ] New function `gdpdu_import_nextcloud(url, user, pass)` for batch cloud import
- [ ] Connect to Nextcloud via WebDAV with username/password authentication
- [ ] List all `.zip` files in a Nextcloud folder
- [ ] Download and extract zip files (one GDPdU export per zip)
- [ ] Prefix table names with zip filename to avoid collisions
- [ ] Skip failed zips and continue — report failures in result set
- [ ] Return summary with table name, row count, status, and source zip

### Out of Scope

- Fixed-length record format — GDPdU supports this but real-world exports use VariableLength
- Foreign key relationship creation — index.xml doesn't define relationships
- Incremental/delta imports — always full overwrite for simplicity
- Nextcloud OAuth/SSO — WebDAV with username+password or app tokens is sufficient
- Recursive folder traversal — only list zips in the specified folder
- Streaming zip extraction — download fully then extract (simpler, reliable)

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
| Separate function for Nextcloud | Keeps local and cloud import concerns separate | — Pending |
| Zip-name prefix for tables | Avoids collisions when importing multiple exports | — Pending |
| Skip-and-continue on errors | Most practical for batch import workflows | — Pending |

---
*Last updated: 2026-02-12 after milestone v1.1 start*
