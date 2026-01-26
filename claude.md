# Build Instructions

## Windows Build (Local)

```bash
cd C:\extensions\navision_gdpdu
mkdir build_x64
cd build_x64

# Step 2: Run CMake with loadable extension enabled
cmake .. -DBUILD_LOADABLE_EXTENSION=ON -G "Visual Studio 17 2022" -A x64

# Step 3: Build the project
cmake --build . --config Release
```

## Output
The built extension will be at: `build_x64/Release/gdpdu.duckdb_extension`
