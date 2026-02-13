# Milestones

## v1.0 Local Import (Shipped: 2026-01-22)

**Phases:** 1-5 | **Plans:** 6 | **Tasks:** ~12
**Timeline:** 2026-01-22 (single day)

**Delivered:** DuckDB extension that imports GDPdU tax audit exports with a single table function call.

**Key accomplishments:**
- DuckDB C++ extension scaffolding with CMake build system
- XML parser for index.xml schema extraction (tables, columns, types, primary keys)
- Table creator with GDPdU-to-DuckDB type mapping
- Data parser for semicolon-delimited files with German locale (decimal comma, DD.MM.YYYY dates)
- `import_gdpdu_navision(path)` table function returning summary result set

---

## v1.1 Nextcloud Batch Import (Shipped: 2026-02-13)

**Phases:** 6-8 | **Plans:** 3 | **Tasks:** 6
**Timeline:** 2 days (2026-02-12 → 2026-02-13)
**Git range:** feat(06-01) → feat(08-01) | **Lines added:** 2,889

**Delivered:** Batch import of multiple GDPdU exports from Nextcloud via WebDAV with table name prefixing.

**Key accomplishments:**
- WebDAV client with HTTP Basic Auth, PROPFIND directory listing, and file download
- Zip extraction module using duckdb_miniz with nested path support
- Batch import orchestrator with skip-and-continue error handling
- New `import_gdpdu_nextcloud(url, user, pass)` table function
- Table name prefixing with sanitized zip filenames to avoid collisions

---

