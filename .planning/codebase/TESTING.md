# Testing Patterns

**Analysis Date:** 2026-01-24

## Test Framework

**Runner:**
- No test framework currently configured
- CMake-based build system (`CMakeLists.txt`)
- Makefile provides placeholder test target: `make test` echoes "No tests configured yet"
- Extension model does not use traditional unit testing

**Assertion Library:**
- Not applicable; no test framework installed

**Run Commands:**
```bash
make release              # Build release extension
make debug               # Build debug extension
make test                # Placeholder (no-op)
make clean               # Remove build artifacts
```

**Build System:**
- CMake 3.12+: `cmake_minimum_required(VERSION 3.12)`
- C++11 standard: `set(CMAKE_CXX_STANDARD 11)`
- Ninja and Make supported: `cmake --build build`

## Test File Organization

**Location:**
- No test files present in repository
- Extension designed for runtime testing via DuckDB SQL queries
- CI/CD workflows test extension loading (see GitHub Actions)

**Naming:**
- Not applicable; no test files present

**Structure:**
- Not applicable; no test infrastructure

## Test Structure

**Manual Integration Testing:**

The project lacks automated unit tests but uses manual integration testing via GitHub Actions workflows.

**Testing via SQL (Example usage from workflow):**
```sql
-- Extension loading test (commented out in CI)
INSTALL 'dist/gdpdu.duckdb_extension';
LOAD gdpdu;
SELECT 'Extension loaded successfully' as status;

-- Expected usage test
SELECT * FROM gdpdu_import('/path/to/gdpdu/data');
```

**Patterns:**
- Load extension: `INSTALL` + `LOAD` commands
- Test basic functionality: call `gdpdu_import()` table function
- Verify results: return metadata about imported tables
- Entry point verification: test that extension registration works

**Build-Level Testing:**
```bash
# CI/CD validates:
make release                           # Successful compilation
file dist/gdpdu.duckdb_extension       # Extension artifact exists
duckdb -unsigned -c "LOAD gdpdu; ..."  # Runtime loading
```

## Mocking

**Framework:** Not applicable; extension operates at DuckDB level.

**Patterns:** Not applicable; no unit test infrastructure.

**Approach:**
- Extension tested as black-box: input XML+CSV files, verify output tables created in DuckDB
- DuckDB Connection object used directly (no mocking needed)
- File I/O tested via real files in integration tests

## Fixtures and Factories

**Test Data:**
- No fixture files committed to repository
- Expected test data: GDPdU format (index.xml + .txt CSV files)
- Samples of expected structure in source code comments:
  - `index.xml` with `DataSet/Media/Table` structure (gdpdu_parser.cpp:258-276)
  - CSV data files with semicolon delimiters (gdpdu_importer.cpp:128-139)

**Location:**
- Test data would be external to codebase (not included)
- Users provide their own GDPdU data directories

## Coverage

**Requirements:** None enforced; no coverage tracking configured.

**View Coverage:**
- Not available; no coverage tool configured
- Manual code review required

## Test Types

**Unit Tests:**
- Not implemented
- Should test individual functions: `to_snake_case()`, `parse_line()`, `convert_german_decimal()`, `convert_german_date()`
- Would require CMake test integration (e.g., Catch2, GoogleTest)

**Integration Tests:**
- Partially tested via CI/CD GitHub Actions workflows
- Test extension builds on multiple platforms: Linux, macOS, Windows
- Test extension artifact creation: `dist/gdpdu.duckdb_extension`
- Extension loading test disabled: marked as "Test step disabled - entry point export issue being investigated"

**E2E Tests:**
- Not implemented
- Would require: GDPdU sample data, DuckDB binary, and test queries
- Currently done manually: load extension, run `SELECT * FROM gdpdu_import(path)`

## CI/CD Integration

**Build Pipeline:**

Three parallel platform builds defined:

**File:** `.github/workflows/linux-build-on-push.yml`
```yaml
# Linux x64 build
- Install: sudo apt-get install build-essential cmake ninja-build curl wget zlib1g-dev libcurl4-openssl-dev
- Build: make release (runs cmake configure + build)
- Package: copy *.duckdb_extension to dist/ with build-info.txt
- Upload: GitHub artifacts (retention: 90 days for SHA-based, 30 days for branch-latest)
- Release: Create GitHub release on tags
- Disabled test: LOAD gdpdu; test (commented out, pending entry point fix)
```

**File:** `.github/workflows/macos-build-on-push.yml` and `windows-build-on-push.yml`
- Same structure as Linux build
- Platform-specific dependencies

**Release Strategy:**
- Tag-based releases: `softprops/action-gh-release` triggered on `refs/tags/*`
- Artifact retention: 90 days for specific commits, 30 days for branch builds
- Build metadata: `build-info.txt` includes Git SHA, branch, timestamp, and installation instructions

## Test Gaps

**Known Issues:**
- Extension loading test disabled in CI/CD workflows (lines 90-104 in linux-build-on-push.yml)
- Reason: "entry point export issue being investigated"
- Testing relies on manual verification post-build

**Untested Areas:**
1. **No unit tests** for core parsing functions:
   - `to_snake_case()` unicode/umlaut handling (gdpdu_parser.cpp:15-121)
   - `parse_line()` CSV parsing with quotes (gdpdu_data_parser.cpp:5-45)
   - `convert_german_decimal()` locale-specific number conversion (gdpdu_data_parser.cpp:47-72)
   - `convert_german_date()` DD.MM.YYYY → ISO format conversion (gdpdu_data_parser.cpp:74-92)

2. **No integration tests** for full import pipeline:
   - XML schema parsing with real GDPdU files
   - Table creation in DuckDB
   - CSV data loading with type conversions
   - Error handling: malformed XML, missing files, type conversion failures

3. **No regression tests** for:
   - German locale handling (umlauts, comma decimals, dot grouping)
   - Windows path handling (backslash conversion)
   - Large file handling (memory/streaming)
   - Special characters in field values (quotes, delimiters)

4. **No error path testing**:
   - Missing index.xml
   - Invalid DataSet/Media structure
   - Column count mismatches
   - Type conversion failures (invalid dates, unparseable numbers)
   - File I/O errors

## Test Infrastructure Recommendations

**For Future Implementation:**

1. **Unit Test Framework:**
   - Catch2 or GoogleTest (C++ native, CMake integration)
   - Test directories: `tests/unit/`
   - Naming: `*_test.cpp` (e.g., `gdpdu_parser_test.cpp`)

2. **Test Data:**
   - `tests/fixtures/gdpdu-sample/` directory with:
     - `index.xml` (valid schema)
     - Sample `*.txt` CSV files (numeric, dates, text)
   - Invalid samples for error testing

3. **CMake Integration:**
   - Add `enable_testing()` in CMakeLists.txt
   - Link test executable against DuckDB, pugixml, and Catch2
   - `make test` should run all tests

4. **CI/CD:**
   - Add test step before packaging: `ctest --output-on-failure`
   - Test both release and debug builds
   - Report coverage if implemented

**Quick Start for Adding Tests:**
```cpp
// tests/unit/gdpdu_parser_test.cpp
#include <catch2/catch.hpp>
#include "gdpdu_parser.hpp"

using namespace duckdb;

TEST_CASE("to_snake_case converts PascalCase") {
    REQUIRE(to_snake_case("EUCountryCode") == "eu_country_code");
    REQUIRE(to_snake_case("ä") == "a");  // German umlauts
}

TEST_CASE("parse_line handles CSV with quotes") {
    auto fields = GdpduDataParser::parse_line(
        "field1;\"quoted,value\";field3",
        ';', '"'
    );
    REQUIRE(fields.size() == 3);
    REQUIRE(fields[1] == "quoted,value");
}
```

---

*Testing analysis: 2026-01-24*
