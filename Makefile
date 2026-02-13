# GDPdU DuckDB Extension Makefile

.PHONY: all release debug clean test configure

# Default target
all: release

# Pass OPENSSL_ROOT_DIR from environment to CMake if set
CMAKE_EXTRA_FLAGS =
ifdef OPENSSL_ROOT_DIR
CMAKE_EXTRA_FLAGS += -DOPENSSL_ROOT_DIR="$(OPENSSL_ROOT_DIR)"
endif

# Configure CMake for release build
configure:
	@mkdir -p build
	cmake -B build -DCMAKE_BUILD_TYPE=Release $(CMAKE_EXTRA_FLAGS)

# Release build
release: configure
	cmake --build build --config Release --verbose

# Debug build
debug:
	@mkdir -p build
	cmake -B build -DCMAKE_BUILD_TYPE=Debug
	cmake --build build --config Debug

# Clean build artifacts
clean:
	@if exist build rmdir /s /q build 2>nul || rm -rf build

# Run tests (placeholder for future test infrastructure)
test: release
	@echo "No tests configured yet"

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build release version (default)"
	@echo "  release  - Build release version"
	@echo "  debug    - Build debug version"
	@echo "  clean    - Remove build directory"
	@echo "  test     - Run tests"
	@echo "  help     - Show this help message"
