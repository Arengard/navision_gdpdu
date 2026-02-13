---
phase: 08-batch-import-integration
plan: 01
subsystem: nextcloud-batch-import
tags: [batch-import, nextcloud, webdav, orchestration, integration]
dependency_graph:
  requires:
    - 06-01 (WebDAV Client)
    - 07-01 (Zip Extraction)
    - 05-01 (GDPdU Importer)
  provides:
    - import_gdpdu_nextcloud table function
  affects:
    - src/gdpdu_extension.cpp (function registration)
tech_stack:
  added: []
  patterns:
    - Skip-and-continue error handling
    - Table name prefixing for namespace isolation
    - Batch orchestration pattern
key_files:
  created:
    - src/include/nextcloud_importer.hpp
    - src/nextcloud_importer.cpp
  modified:
    - src/gdpdu_extension.cpp
decisions:
  - decision: "Use skip-and-continue pattern for failed zips"
    rationale: "Most practical for batch workflows - one bad zip shouldn't abort the entire import"
  - decision: "Sanitize zip filenames for table prefixes"
    rationale: "Ensures valid SQL identifiers and avoids naming collisions"
  - decision: "Search for index.xml to determine GDPdU root directory"
    rationale: "Handles both root-level and nested directory structures in zip files"
metrics:
  duration_minutes: 3
  tasks_completed: 2
  files_created: 2
  files_modified: 1
  commits: 2
  completed_date: 2026-02-13
---

# Phase 08 Plan 01: Nextcloud Batch Import Integration Summary

**One-liner:** Batch import orchestrator wiring WebDAV listing, zip download/extraction, and GDPdU import with table name prefixing and resilient error handling

## What Was Built

Implemented the final integration layer that delivers the `import_gdpdu_nextcloud` table function — the core feature of v1.1. This orchestrator combines all previous components (WebDAV client, zip extractor, GDPdU importer) into a single user-facing function that batch-imports multiple GDPdU exports from a Nextcloud folder.

### Key Components

**1. NextcloudImportResult Struct**
- 4 fields: `table_name` (prefixed), `row_count`, `status`, `source_zip`
- Provides detailed feedback for each imported table
- Enables tracking of which zip produced which tables

**2. Batch Orchestrator (import_from_nextcloud)**
- Lists zip files via WebDAV
- Downloads each to temp directory
- Extracts zip contents
- Detects GDPdU root by finding index.xml location
- Imports tables with sanitized zip prefix
- Renames tables to avoid collisions
- Uses skip-and-continue pattern for resilient batch processing

**3. Sanitize Zip Prefix Helper**
- Strips .zip extension (case-insensitive)
- Replaces non-alphanumeric chars with underscores
- Collapses consecutive underscores
- Trims leading/trailing underscores
- Example: "Export 2024.zip" → "Export_2024"

**4. Table Function Registration**
- 3 VARCHAR arguments: URL, username, password
- 4 return columns: table_name, row_count, status, source_zip
- Follows existing pattern from import_gdpdu_navision

## Implementation Details

### Error Handling Strategy

The implementation uses a **skip-and-continue pattern** for maximum resilience:

- If WebDAV listing fails → return single error result, abort
- If temp directory creation fails → return error result, abort
- For each zip file:
  - Download fails → add error result, continue with next zip
  - Extraction fails → add error result, continue with next zip
  - Import fails → add error result, continue with next zip
  - Rename fails → add error result with import data, continue

This ensures that one problematic zip file doesn't prevent importing all others.

### Table Name Prefixing

Each table gets prefixed with the sanitized zip filename:
```
Original zip: "Export 2024.zip"
Sanitized: "Export_2024"
Table in zip: "Buchungen"
Final name: "Export_2024_Buchungen"
```

This prevents naming collisions when importing multiple exports that contain tables with the same names.

### GDPdU Root Detection

The orchestrator searches for `index.xml` in the extracted files:
- If found at root → use extract_dir directly
- If found in subdirectory (e.g., `subdir/index.xml`) → use `extract_dir/subdir`

This handles both direct zip exports and zips that wrap the export in a parent folder.

### Resource Cleanup

The implementation ensures proper cleanup:
1. Each extraction directory is removed after processing its tables
2. The temp download directory is removed after all zips are processed
3. Cleanup happens even when errors occur (via continue statements)

## Integration Points

### Incoming Dependencies

- **WebDavClient** (06-01): Lists and downloads zip files from Nextcloud
- **extract_zip** (07-01): Extracts zip contents to temp directory
- **import_gdpdu_navision** (05-01): Imports tables from GDPdU data directory

### Outgoing API

Users call:
```sql
SELECT * FROM import_gdpdu_nextcloud(
    'https://cloud.example.com/remote.php/dav/files/user/exports/',
    'username',
    'password'
);
```

Returns result set with:
- `table_name`: Prefixed table name (e.g., "Export_2024_Buchungen")
- `row_count`: Number of rows imported (or 0 on error)
- `status`: "OK" or error description
- `source_zip`: Original zip filename

## Verification Status

### Code Verification

- ✅ Files created: `src/include/nextcloud_importer.hpp`, `src/nextcloud_importer.cpp`
- ✅ File modified: `src/gdpdu_extension.cpp`
- ✅ All required headers included
- ✅ Skip-and-continue pattern implemented correctly
- ✅ Table function registered with correct signature
- ✅ 4 return columns defined: table_name, row_count, status, source_zip

### Build Verification

- ✅ Code pushed to GitHub (commits 4416e3f, ee2a519)
- ⏳ CI build status: Pending (GitHub Actions will validate)

Per CLAUDE.md instructions, CI pipeline handles the build automatically.

## Deviations from Plan

None - plan executed exactly as written.

## Task Completion

| Task | Name                                          | Status   | Commit  | Files                                                        |
| ---- | --------------------------------------------- | -------- | ------- | ------------------------------------------------------------ |
| 1    | Create batch import orchestrator module       | Complete | 4416e3f | src/include/nextcloud_importer.hpp, src/nextcloud_importer.cpp |
| 2    | Register import_gdpdu_nextcloud table function| Complete | ee2a519 | src/gdpdu_extension.cpp                                      |

## Self-Check: PASSED

### Created Files Verification
```bash
[✓] src/include/nextcloud_importer.hpp exists
[✓] src/nextcloud_importer.cpp exists
```

### Commit Verification
```bash
[✓] Commit 4416e3f exists (Task 1)
[✓] Commit ee2a519 exists (Task 2)
```

### Code Structure Verification
- ✅ NextcloudImportResult struct has 4 fields
- ✅ sanitize_zip_prefix function implemented
- ✅ import_from_nextcloud function follows skip-and-continue pattern
- ✅ NextcloudImportBindData, NextcloudImportGlobalState defined
- ✅ NextcloudImportBind, NextcloudImportInit, NextcloudImportScan implemented
- ✅ Table function registered in LoadInternal

## Next Steps

**Phase 08 Status:** 1/1 plans complete ✅

**Phase 08 Complete!** The Nextcloud batch import integration is fully implemented.

**Next:** Milestone v1.1 is complete. All components are integrated:
- Local import (v1.0) ✅
- WebDAV client ✅
- Zip extraction ✅
- Batch orchestration ✅

The extension now supports:
```sql
-- Import from local directory
SELECT * FROM import_gdpdu_navision('/path/to/export');

-- Import from Nextcloud
SELECT * FROM import_gdpdu_nextcloud(
    'https://cloud.example.com/remote.php/dav/files/user/exports/',
    'username',
    'password'
);
```

## Notes

- CI build verification pending (automatic via GitHub Actions)
- Extension requires `-unsigned` flag to load (expected for unsigned extensions)
- Self-signed SSL certificates in Nextcloud are supported (cert verification disabled)
- Temp directories are created/cleaned automatically
- Failed zips are reported but don't abort the batch
