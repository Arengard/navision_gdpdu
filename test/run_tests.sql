-- GDPdU Extension Test Suite
-- Run with: duckdb -unsigned -c "LOAD 'dist/gdpdu.duckdb_extension';" < test/run_tests.sql

-- ============================================================
-- Test 1: Basic GDPdU import
-- ============================================================
SELECT '--- Test 1: Basic GDPdU import ---' as test;

SELECT * FROM import_gdpdu_navision('test/fixtures/basic_gdpdu');

-- Verify Kunden table exists and has correct row count
SELECT CASE WHEN cnt = 3 THEN 'PASS' ELSE 'FAIL: expected 3 rows, got ' || cnt::VARCHAR END as test_kunden_row_count
FROM (SELECT COUNT(*) as cnt FROM "Kunden");

-- Verify Buchungen table exists and has correct row count
SELECT CASE WHEN cnt = 3 THEN 'PASS' ELSE 'FAIL: expected 3 rows, got ' || cnt::VARCHAR END as test_buchungen_row_count
FROM (SELECT COUNT(*) as cnt FROM "Buchungen");

-- ============================================================
-- Test 2: Column names (snake_case conversion from Name element)
-- ============================================================
SELECT '--- Test 2: Column names ---' as test;

SELECT CASE WHEN column_name = 'nr' THEN 'PASS' ELSE 'FAIL: expected nr, got ' || column_name END as test_pk_column
FROM information_schema.columns
WHERE table_name = 'Kunden' AND ordinal_position = 1;

SELECT CASE WHEN column_name = 'name' THEN 'PASS' ELSE 'FAIL: expected name, got ' || column_name END as test_name_column
FROM information_schema.columns
WHERE table_name = 'Kunden' AND ordinal_position = 2;

SELECT CASE WHEN column_name = 'saldo' THEN 'PASS' ELSE 'FAIL: expected saldo, got ' || column_name END as test_saldo_column
FROM information_schema.columns
WHERE table_name = 'Kunden' AND ordinal_position = 3;

SELECT CASE WHEN column_name = 'erstell_datum' THEN 'PASS' ELSE 'FAIL: expected erstell_datum, got ' || column_name END as test_date_column
FROM information_schema.columns
WHERE table_name = 'Kunden' AND ordinal_position = 4;

-- ============================================================
-- Test 3: Data type mapping
-- ============================================================
SELECT '--- Test 3: Data type mapping ---' as test;

-- Nr should be VARCHAR (AlphaNumeric)
SELECT CASE WHEN data_type = 'VARCHAR' THEN 'PASS' ELSE 'FAIL: nr type is ' || data_type END as test_varchar_type
FROM information_schema.columns
WHERE table_name = 'Kunden' AND column_name = 'nr';

-- Saldo should be DECIMAL (Numeric with Accuracy=2)
SELECT CASE WHEN data_type = 'DECIMAL' THEN 'PASS' ELSE 'FAIL: saldo type is ' || data_type END as test_decimal_type
FROM information_schema.columns
WHERE table_name = 'Kunden' AND column_name = 'saldo';

-- ErstellDatum should be DATE
SELECT CASE WHEN data_type = 'DATE' THEN 'PASS' ELSE 'FAIL: erstell_datum type is ' || data_type END as test_date_type
FROM information_schema.columns
WHERE table_name = 'Kunden' AND column_name = 'erstell_datum';

-- Anzahl should be BIGINT (Numeric without Accuracy)
SELECT CASE WHEN data_type = 'BIGINT' THEN 'PASS' ELSE 'FAIL: anzahl type is ' || data_type END as test_bigint_type
FROM information_schema.columns
WHERE table_name = 'Kunden' AND column_name = 'anzahl';

-- ============================================================
-- Test 4: German decimal conversion
-- ============================================================
SELECT '--- Test 4: German decimal conversion ---' as test;

SELECT CASE WHEN saldo = 1234.56 THEN 'PASS' ELSE 'FAIL: expected 1234.56, got ' || saldo::VARCHAR END as test_decimal_conversion
FROM "Kunden" WHERE nr = 'K001';

SELECT CASE WHEN saldo = -567.89 THEN 'PASS' ELSE 'FAIL: expected -567.89, got ' || saldo::VARCHAR END as test_negative_decimal
FROM "Kunden" WHERE nr = 'K002';

SELECT CASE WHEN saldo = 0.00 THEN 'PASS' ELSE 'FAIL: expected 0.00, got ' || saldo::VARCHAR END as test_zero_decimal
FROM "Kunden" WHERE nr = 'K003';

-- ============================================================
-- Test 5: German date conversion (DD.MM.YYYY -> DATE)
-- ============================================================
SELECT '--- Test 5: German date conversion ---' as test;

SELECT CASE WHEN erstell_datum = DATE '2024-03-15' THEN 'PASS' ELSE 'FAIL: expected 2024-03-15, got ' || erstell_datum::VARCHAR END as test_date_conversion
FROM "Kunden" WHERE nr = 'K001';

-- ============================================================
-- Test 6: Integer column (Numeric without Accuracy)
-- ============================================================
SELECT '--- Test 6: Integer values ---' as test;

SELECT CASE WHEN anzahl = 5 THEN 'PASS' ELSE 'FAIL: expected 5, got ' || anzahl::VARCHAR END as test_integer_value
FROM "Kunden" WHERE nr = 'K001';

SELECT CASE WHEN anzahl = 0 THEN 'PASS' ELSE 'FAIL: expected 0, got ' || anzahl::VARCHAR END as test_zero_integer
FROM "Kunden" WHERE nr = 'K003';

-- ============================================================
-- Test 7: Second table (Buchungen)
-- ============================================================
SELECT '--- Test 7: Buchungen table ---' as test;

SELECT CASE WHEN betrag = 1500.00 THEN 'PASS' ELSE 'FAIL: expected 1500.00, got ' || betrag::VARCHAR END as test_buchung_betrag
FROM "Buchungen" WHERE lfd_nr = 1;

SELECT CASE WHEN betrag = -250.75 THEN 'PASS' ELSE 'FAIL: expected -250.75, got ' || betrag::VARCHAR END as test_negative_buchung
FROM "Buchungen" WHERE lfd_nr = 2;

-- ============================================================
-- Test 8: Description-based column names
-- ============================================================
SELECT '--- Test 8: Description-based import ---' as test;

SELECT * FROM import_gdpdu_navision('test/fixtures/basic_gdpdu', 'Description');

-- Should use Description element for names, converted to snake_case
SELECT CASE WHEN column_name = 'kundennummer' THEN 'PASS' ELSE 'FAIL: expected kundennummer, got ' || column_name END as test_description_pk
FROM information_schema.columns
WHERE table_name = 'Kunden' AND ordinal_position = 1;

SELECT CASE WHEN column_name = 'kundenname' THEN 'PASS' ELSE 'FAIL: expected kundenname, got ' || column_name END as test_description_name
FROM information_schema.columns
WHERE table_name = 'Kunden' AND ordinal_position = 2;

-- ============================================================
-- Test 9: Path traversal rejection
-- ============================================================
SELECT '--- Test 9: Path traversal rejection ---' as test;

SELECT CASE WHEN status LIKE '%Path traversal%' THEN 'PASS' ELSE 'FAIL: expected path traversal error, got ' || status END as test_path_traversal
FROM import_gdpdu_navision('../../etc/passwd');

-- ============================================================
-- Summary
-- ============================================================
SELECT '--- All tests completed ---' as test;
