# Requirements: GDPdU DuckDB Extension

**Defined:** 2026-01-22
**Core Value:** Read any valid GDPdU export into DuckDB with a single function call — no manual schema definition or data wrangling required.

## v1.0 Requirements (Complete)

### Core Import

- [x] **IMPORT-01**: User can call `gdpdu_import(path)` table function with a directory path
- [x] **IMPORT-02**: Function returns result set with table name, row count, and status for each imported table
- [x] **IMPORT-03**: Function discovers and reads `index.xml` from the provided directory
- [x] **IMPORT-04**: All tables defined in index.xml are created in the default schema
- [x] **IMPORT-05**: Existing tables with same names are dropped and recreated (overwrite behavior)

### Schema Parsing

- [x] **SCHEMA-01**: Extension parses index.xml following GDPdU DTD structure
- [x] **SCHEMA-02**: Column names are extracted from `<Name>` elements in VariableColumn/VariablePrimaryKey
- [x] **SCHEMA-03**: Column types are mapped: AlphaNumeric→VARCHAR, Numeric→DECIMAL, Date→DATE
- [x] **SCHEMA-04**: Numeric precision is applied from `<Accuracy>` element when present
- [x] **SCHEMA-05**: Primary keys are identified from `<VariablePrimaryKey>` elements (including composite)
- [x] **SCHEMA-06**: Data file path is extracted from `<URL>` element for each table

### Data Loading

- [x] **DATA-01**: Extension reads semicolon-delimited `.txt` files
- [x] **DATA-02**: German decimal format is parsed (comma as decimal separator)
- [x] **DATA-03**: German digit grouping is handled (dot as thousands separator)
- [x] **DATA-04**: German date format is parsed (DD.MM.YYYY)
- [x] **DATA-05**: UTF-8 encoded files are read correctly
- [x] **DATA-06**: Quoted string values are parsed correctly

### Extension Infrastructure

- [x] **EXT-01**: Extension compiles as a DuckDB C++ extension
- [x] **EXT-02**: Extension registers `gdpdu_import` as a table function
- [x] **EXT-03**: Extension can be loaded via `LOAD 'gdpdu'`
- [x] **EXT-04**: Windows file paths (backslashes) are handled correctly

## v1.1 Requirements

Requirements for Nextcloud Batch Import milestone.

### Networking

- [ ] **NET-01**: User can connect to Nextcloud via WebDAV using username and password
- [ ] **NET-02**: Extension lists all `.zip` files in a specified Nextcloud WebDAV folder
- [ ] **NET-03**: Extension downloads zip files from Nextcloud to a temp directory

### Zip Handling

- [ ] **ZIP-01**: Extension extracts GDPdU export (index.xml + .txt files) from a downloaded zip
- [ ] **ZIP-02**: Temporary files are cleaned up after import completes

### Batch Import

- [ ] **BATCH-01**: User can call `gdpdu_import_nextcloud(url, user, pass)` table function
- [ ] **BATCH-02**: Table names are prefixed with sanitized zip filename (e.g. `export2024_Buchungen`)
- [ ] **BATCH-03**: Failed zip files are skipped and remaining zips continue importing
- [ ] **BATCH-04**: Result set includes table name, row count, status, and source zip filename

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Advanced Features

- **ADV-01**: Support for fixed-length record format (alternative to VariableLength)
- **ADV-03**: Incremental import (only new/changed records)
- **ADV-04**: Schema-qualified table names (`gdpdu.Sachposten`)

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Foreign key creation | index.xml doesn't define relationships |
| Non-file media types | GDPdU spec allows CD/DVD references, we assume filesystem |
| Data validation | Extension imports as-is, validation is user's responsibility |
| GUI/CLI wrapper | Extension only, tooling built separately if needed |
| Nextcloud OAuth/SSO | WebDAV with username+password or app tokens is sufficient |
| Recursive folder traversal | Only list zips in the specified folder |
| Streaming zip extraction | Download fully then extract (simpler, reliable) |

## Traceability

Which phases cover which requirements. Updated by create-roadmap.

| Requirement | Phase | Status |
|-------------|-------|--------|
| IMPORT-01 | Phase 5 | Complete |
| IMPORT-02 | Phase 5 | Complete |
| IMPORT-03 | Phase 5 | Complete |
| IMPORT-04 | Phase 3 | Complete |
| IMPORT-05 | Phase 3 | Complete |
| SCHEMA-01 | Phase 2 | Complete |
| SCHEMA-02 | Phase 2 | Complete |
| SCHEMA-03 | Phase 2 | Complete |
| SCHEMA-04 | Phase 2 | Complete |
| SCHEMA-05 | Phase 2 | Complete |
| SCHEMA-06 | Phase 2 | Complete |
| DATA-01 | Phase 4 | Complete |
| DATA-02 | Phase 4 | Complete |
| DATA-03 | Phase 4 | Complete |
| DATA-04 | Phase 4 | Complete |
| DATA-05 | Phase 4 | Complete |
| DATA-06 | Phase 4 | Complete |
| EXT-01 | Phase 1 | Complete |
| EXT-02 | Phase 5 | Complete |
| EXT-03 | Phase 1 | Complete |
| EXT-04 | Phase 5 | Complete |
| NET-01 | Phase 6 | Pending |
| NET-02 | Phase 6 | Pending |
| NET-03 | Phase 6 | Pending |
| ZIP-01 | Phase 7 | Pending |
| ZIP-02 | Phase 7 | Pending |
| BATCH-01 | Phase 8 | Pending |
| BATCH-02 | Phase 8 | Pending |
| BATCH-03 | Phase 8 | Pending |
| BATCH-04 | Phase 8 | Pending |

**Coverage:**
- v1.0 requirements: 21 total — 21 complete
- v1.1 requirements: 9 total — 9 mapped to phases
- Unmapped: 0 (100% coverage)

---
*Requirements defined: 2026-01-22*
*Last updated: 2026-02-12 after v1.1 roadmap creation*
