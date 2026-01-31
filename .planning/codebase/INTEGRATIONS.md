# External Integrations

**Analysis Date:** 2026-01-24

## APIs & External Services

**None configured** - This is a standalone database extension with no outbound API calls.

The extension operates entirely within DuckDB and reads local GDPdU export directories. No external service calls are made.

## Data Storage

**Databases:**
- DuckDB (primary database)
  - Connection: In-process via `DatabaseInstance::GetDatabase(context)` in `src/gdpdu_extension.cpp:72`
  - Client/API: DuckDB C++ API (`duckdb.hpp`, `duckdb/main/connection.hpp`)
  - Interaction: `Connection` object executes SQL for table creation and data insertion

**File Storage:**
- Local filesystem only
  - Reads GDPdU export directories containing `index.xml` and `.txt` data files
  - Path handling in `src/gdpdu_importer.cpp:10-26` normalizes Windows/Unix paths
  - Directory path passed as argument to `gdpdu_import()` table function

**Data Format:**
- XML Schema: `index.xml` file parsed with pugixml
  - Location: Root of GDPdU export directory
  - Defines table structures and column definitions
  - Parsed in `src/gdpdu_parser.cpp` via `parse_index_xml()` function

- Semicolon-delimited text files (`.txt`)
  - Delimiter: `;` (semicolon)
  - Header: None (GDPdU files have no header row)
  - Quote character: `"` (double quote)
  - Read via DuckDB's native `read_csv()` function in `src/gdpdu_importer.cpp:127`

**Caching:**
- None

## Authentication & Identity

**Auth Provider:**
- Not applicable - No external authentication

**Internal Authorization:**
- DuckDB transaction/connection context scoping
  - ClientContext passed through bind, init, and scan functions

## Monitoring & Observability

**Error Tracking:**
- None configured
- Errors reported as string status in ImportResult struct
- `src/gdpdu_importer.cpp:92-97` catches parse exceptions
- `src/gdpdu_importer.cpp:141-157` catches load exceptions and sets status field

**Logs:**
- No structured logging
- DuckDB extension runs within DuckDB process logs
- Build diagnostics via GitHub Actions job logs
- Build artifacts include `build-info.txt` with commit SHA, branch, and timestamp

## CI/CD & Deployment

**Hosting:**
- GitHub (source code repository)
- GitHub Releases (artifact distribution)

**CI Pipeline:**
- GitHub Actions (primary CI)
  - Linux builds: `.github/workflows/linux-build-on-push.yml` - Runs on every branch push
  - macOS builds: `.github/workflows/macos-build-on-push.yml` - Runs on every branch push
  - Windows builds: `.github/workflows/windows-build-on-push.yml` - Runs on every branch push
  - Release builds: `.github/workflows/release.yml` - Triggered on git tags matching `v*` pattern

**Build Output:**
- Extension artifact: `dist/gdpdu.duckdb_extension`
- Metadata: `dist/build-info.txt` with git SHA, branch, and build date
- Uploaded to GitHub Actions artifacts with 90-day retention (per-commit) or 30-day (per-branch)
- Released to GitHub Releases on version tags

**Build Environment Variables:**
- `BUILD_UNITTESTS=0` - Disable DuckDB unit tests
- `BUILD_SHELL=0` - Disable DuckDB shell build
- `VCPKG_TOOLCHAIN_PATH` - Windows only, points to vcpkg CMake toolchain

## Environment Configuration

**Required env vars:**
- None for runtime operation
- GDPdU directory path passed as SQL argument: `SELECT * FROM gdpdu_import('/path/to/data')`

**Optional Parameters:**
- Column name field: `SELECT * FROM gdpdu_import('/path/to/data', 'Description')`
  - Default: 'Name' - Uses XML `<Name>` element for column names
  - Alternative: 'Description' - Uses XML `<Description>` element for column names (German labels)

**Secrets location:**
- None required
- GITHUB_TOKEN used by `softprops/action-gh-release@v2` for GitHub release creation (automatic via GitHub Actions)

## Webhooks & Callbacks

**Incoming:**
- None

**Outgoing:**
- None

## Data Format Support

**GDPdU Data Types Supported:**
- AlphaNumeric → DuckDB VARCHAR
- Numeric (integer) → DuckDB BIGINT
- Numeric (decimal) → DuckDB DECIMAL(18,n)
- Date (German format DD.MM.YYYY) → DuckDB DATE

**Number Format Handling:**
- Input: German format with comma as decimal separator (e.g., `1.234,56`)
- Processing: Conversion in `src/gdpdu_importer.cpp:64-69`
  - Removes digit grouping dots: `1.234` → `1234`
  - Replaces decimal comma: `,` → `.`
- Output: Standard decimal format in DuckDB

**Date Format Handling:**
- Input: German date format `DD.MM.YYYY` (e.g., `24.01.2026`)
- Processing: Conversion in `src/gdpdu_importer.cpp:73` using DuckDB `strptime()` function
- Empty/whitespace values: Converted to NULL via CASE statement

---

*Integration audit: 2026-01-24*
