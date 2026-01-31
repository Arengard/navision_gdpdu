# Technology Stack

**Analysis Date:** 2026-01-24

## Languages

**Primary:**
- C++ 11 - Core extension implementation
- CMake - Build system configuration

**Secondary:**
- SQL/DuckDB - Data import and transformation queries
- Bash - CI/CD scripts and build automation
- YAML - GitHub Actions workflow configuration

## Runtime

**Environment:**
- DuckDB 1.4.3+ - Database engine and extension framework
- Native binary compilation to platform-specific extension files

**Build Tools:**
- CMake 3.12+ - Cross-platform build configuration
- Ninja - High-performance build tool (optional, used in CI)
- vcpkg - C++ package manager (Windows builds)

## Frameworks

**Core:**
- DuckDB Extension API (v1.4+) - Provides table functions, data structures, and extension loading
  - Location: Headers from `duckdb.hpp` and `duckdb/main/extension.hpp`
  - Version: 1.4.3 (fetched via FetchContent in CMakeLists.txt)

**Testing:**
- None currently configured
- Makefile placeholder for `make test` target exists at `Makefile:28`

**Build/Dev:**
- FetchContent (CMake) - Dependency management for DuckDB and pugixml
- GitHub Actions - CI/CD pipeline for multi-platform builds

## Key Dependencies

**Critical:**
- pugixml v1.14 - XML parsing library for GDPdU index.xml schema files
  - Package: `pugixml`
  - Linked in `src/CMakeLists.txt:17-18` to both static and loadable extensions
  - Used by `src/gdpdu_parser.cpp` for XML DOM parsing

**Infrastructure:**
- DuckDB v1.4.3 - Relational database with extension support
  - Fetched from: `https://github.com/duckdb/duckdb.git`
  - GIT_TAG: `v1.4.3`
  - Extended via DUCKDB_CPP_EXTENSION_ENTRY macro pattern

## Configuration

**Environment:**
- BUILD_UNITTESTS - Disabled (set to OFF in CMakeLists.txt:19)
- BUILD_SHELL - Disabled (set to OFF in CMakeLists.txt:20)
- BUILD_BENCHMARKS - Disabled (set to OFF in CMakeLists.txt:21)
- EXTENSION_STATIC_BUILD - Enabled (set to ON in CMakeLists.txt:27)
- DUCKDB_EXTENSION_NAMES - Empty string (no built-in extensions loaded)
- SKIP_EXTENSIONS - parquet, icu, tpch, tpcds, fts, httpfs, json, jemalloc, autocomplete, inet, excel, sqlsmith, visualizer

**Build:**
- Root: `CMakeLists.txt` - Main configuration for DuckDB and pugixml fetching
- Extension: `src/CMakeLists.txt` - Builds both static and loadable extension variants
- Package config: `extension_config.cmake` - Tells DuckDB where to find the gdpdu extension
- vcpkg: `vcpkg.json` - Package dependencies for Windows builds
- Makefile: `Makefile` - Convenience targets for configure, release, debug, clean, test

## Platform Requirements

**Development:**
- Linux: `build-essential`, `cmake`, `ninja-build`, `curl`, `wget`, `zlib1g-dev`, `libcurl4-openssl-dev`
- macOS: `cmake`, `ninja`, `curl`, `wget` (via Homebrew)
- Windows: `cmake`, `ninja` (via Chocolatey), MSVC, vcpkg

**Production:**
- DuckDB installation or embedded DuckDB instance (1.4+)
- GDPdU export directory with:
  - `index.xml` - Schema definition file
  - `.txt` data files referenced in index.xml
- No external runtime dependencies beyond DuckDB

## Deployment

**Artifact Type:**
- `.duckdb_extension` - Platform-specific binary extension file
- Built for: Linux x64, macOS (Intel/Apple Silicon), Windows x64

**Installation:**
```sql
INSTALL './gdpdu.duckdb_extension';
LOAD gdpdu;
```

**Multi-platform Builds:**
- Linux: `ubuntu-latest` runner (GitHub Actions)
- macOS: `macos-latest` runner (GitHub Actions)
- Windows: `windows-latest` runner (GitHub Actions)

---

*Stack analysis: 2026-01-24*
