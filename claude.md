# Build Instructions

Do NOT build locally. Instead, commit and push to GitHub — the CI pipeline will handle the build.

## Windows Build (Reference Only)

```bash
cd C:\extensions\navision_gdpdu
mkdir build_x64
cd build_x64
cmake .. -DBUILD_LOADABLE_EXTENSION=ON -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Output
The built extension will be at: `build_x64/Release/gdpdu.duckdb_extension`
