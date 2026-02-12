# Roadmap: GDPdU DuckDB Extension

## Overview

Build a DuckDB C++ extension that reads German tax audit (GDPdU) exports with a single function call. v1.0 delivered local import via `gdpdu_import(path)`. v1.1 adds Nextcloud batch import via WebDAV — download multiple zips, extract GDPdU exports, and import with table name prefixing.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

### v1.0 - Local Import
- [x] **Phase 1: Foundation** - DuckDB extension scaffolding that compiles and loads
- [x] **Phase 2: XML Parser** - Parse index.xml and extract table/column definitions
- [x] **Phase 3: Table Creation** - Create DuckDB tables from parsed schema definitions
- [x] **Phase 4: Data Parser** - Read .txt files with German locale formatting
- [x] **Phase 5: Integration** - Wire everything into gdpdu_import table function

### v1.1 - Nextcloud Batch Import
- [ ] **Phase 6: WebDAV Client** - Connect to Nextcloud and download zip files
- [ ] **Phase 7: Zip Extraction** - Extract GDPdU exports from downloaded zips
- [ ] **Phase 8: Batch Import Integration** - Wire batch import with table name prefixing

## Phase Details

### Phase 1: Foundation
**Goal**: DuckDB extension scaffolding that compiles and loads
**Depends on**: Nothing (first phase)
**Requirements**: EXT-01, EXT-03
**Success Criteria** (what must be TRUE):
  1. Extension compiles without errors using CMake
  2. Extension can be loaded in DuckDB via `LOAD 'gdpdu'`
  3. Extension logs successful initialization
**Research**: Likely (DuckDB extension API, CMake template)
**Research topics**: DuckDB extension template, extension API for 1.0+, CMake configuration
**Plans**: 1/1 complete

### Phase 2: XML Parser
**Goal**: Parse index.xml and extract table/column definitions
**Depends on**: Phase 1
**Requirements**: SCHEMA-01, SCHEMA-02, SCHEMA-03, SCHEMA-04, SCHEMA-05, SCHEMA-06
**Success Criteria** (what must be TRUE):
  1. Parser reads index.xml and extracts all table definitions
  2. Column names and types are correctly extracted for each table
  3. Primary key columns are identified (including composite keys)
  4. Data file paths are extracted from URL elements
**Research**: Unlikely (standard XML parsing, pugixml well-documented)
**Plans**: 2/2 complete
  - 02-01: Schema data structures (TableDef, ColumnDef, GdpduType)
  - 02-02: XML parser implementation (parse_index_xml with pugixml)

### Phase 3: Table Creation
**Goal**: Create DuckDB tables from parsed schema definitions
**Depends on**: Phase 2
**Requirements**: IMPORT-04, IMPORT-05
**Success Criteria** (what must be TRUE):
  1. Tables are created in DuckDB with correct column names
  2. Column types match GDPdU→DuckDB mapping (AlphaNumeric→VARCHAR, etc.)
  3. Existing tables are dropped before recreation
**Research**: Unlikely (uses DuckDB API from Phase 1)
**Plans**: 1/1 complete
  - 03-01: Table creator module (create_tables, DROP/CREATE, type mapping)

### Phase 4: Data Parser
**Goal**: Read .txt files with German locale formatting
**Depends on**: Phase 1
**Requirements**: DATA-01, DATA-02, DATA-03, DATA-04, DATA-05, DATA-06
**Success Criteria** (what must be TRUE):
  1. Semicolon-delimited files are parsed correctly
  2. German decimals (comma) are converted to proper numeric values
  3. German dates (DD.MM.YYYY) are converted to DATE type
  4. UTF-8 files with quoted strings are handled correctly
**Research**: Unlikely (standard C++ file I/O and parsing)
**Plans**: 1/1 complete
  - 04-01: Data parser module (line parsing, German decimal/date conversion, file parsing)

### Phase 5: Integration
**Goal**: Wire everything into gdpdu_import table function
**Depends on**: Phase 2, Phase 3, Phase 4
**Requirements**: IMPORT-01, IMPORT-02, IMPORT-03, EXT-02, EXT-04
**Success Criteria** (what must be TRUE):
  1. User can call `SELECT * FROM gdpdu_import('path')`
  2. Function returns table name, row count, status for each imported table
  3. All tables from index.xml are created and populated in one call
  4. Windows paths with backslashes work correctly
**Research**: Unlikely (wiring existing components)
**Plans**: 1/1 complete
  - 05-01: Import orchestrator and table function registration

### Phase 6: WebDAV Client
**Goal**: Connect to Nextcloud and download zip files via WebDAV
**Depends on**: Phase 1 (extension infrastructure)
**Requirements**: NET-01, NET-02, NET-03
**Success Criteria** (what must be TRUE):
  1. User can authenticate to Nextcloud WebDAV with username and password
  2. Extension lists all `.zip` files from a specified WebDAV folder URL
  3. Extension downloads zip files to a temporary directory
  4. Network errors are caught and reported clearly
**Research**: TBD
**Plans**: TBD

### Phase 7: Zip Extraction
**Goal**: Extract GDPdU exports from downloaded zips with cleanup
**Depends on**: Phase 6 (needs downloaded zips)
**Requirements**: ZIP-01, ZIP-02
**Success Criteria** (what must be TRUE):
  1. Extension extracts index.xml and .txt files from a zip archive
  2. Extracted files are accessible for import processing
  3. Temporary files are cleaned up after import completes (success or failure)
  4. Extraction errors are caught and reported
**Research**: TBD
**Plans**: TBD

### Phase 8: Batch Import Integration
**Goal**: Wire batch import with table name prefixing and resilient processing
**Depends on**: Phase 5 (gdpdu_import logic), Phase 6 (WebDAV), Phase 7 (Zip extraction)
**Requirements**: BATCH-01, BATCH-02, BATCH-03, BATCH-04
**Success Criteria** (what must be TRUE):
  1. User can call `SELECT * FROM gdpdu_import_nextcloud('url', 'user', 'pass')`
  2. Table names are prefixed with sanitized zip filename (e.g. `export2024_Buchungen`)
  3. Failed zip files are skipped and remaining zips continue importing
  4. Result set includes table name, row count, status, and source zip filename for each table
  5. Function completes successfully even if some zips fail
**Research**: TBD
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation | 1/1 | Complete | 2026-01-22 |
| 2. XML Parser | 2/2 | Complete | 2026-01-22 |
| 3. Table Creation | 1/1 | Complete | 2026-01-22 |
| 4. Data Parser | 1/1 | Complete | 2026-01-22 |
| 5. Integration | 1/1 | Complete | 2026-01-22 |
| 6. WebDAV Client | 0/0 | Not Started | — |
| 7. Zip Extraction | 0/0 | Not Started | — |
| 8. Batch Import Integration | 0/0 | Not Started | — |

**Overall Progress:** █████░░░░░ 62% (5/8 phases complete)
