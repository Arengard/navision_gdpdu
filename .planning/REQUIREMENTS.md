# Requirements: GDPdU DuckDB Extension

**Defined:** 2026-01-22
**Core Value:** Read any valid GDPdU export into DuckDB with a single function call — no manual schema definition or data wrangling required.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

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

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Advanced Features

- **ADV-01**: Support for fixed-length record format (alternative to VariableLength)
- **ADV-02**: Support for reading from ZIP archives
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

**Coverage:**
- v1 requirements: 21 total
- Mapped to phases: 21
- Unmapped: 0 ✓

---
*Requirements defined: 2026-01-22*
*Last updated: 2026-01-22 after roadmap creation*
