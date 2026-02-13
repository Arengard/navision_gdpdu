#include "zip_extractor.hpp"
#include "webdav_client.hpp"
#include "miniz.hpp"

#include <fstream>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

namespace duckdb {

// Helper function to create all parent directories for a file path
static bool create_parent_dirs(const std::string& file_path) {
    size_t pos = 0;
    while ((pos = file_path.find('/', pos)) != std::string::npos) {
        std::string dir = file_path.substr(0, pos);
        if (!dir.empty()) {
            MKDIR(dir.c_str());  // Ignore errors - directory may already exist
        }
        pos++;
    }
    return true;
}

// Helper function to normalize path separators to forward slashes
static std::string normalize_path(const std::string& path) {
    std::string normalized = path;
    for (size_t i = 0; i < normalized.size(); i++) {
        if (normalized[i] == '\\') {
            normalized[i] = '/';
        }
    }
    return normalized;
}

// Read entire file into memory
static bool read_file_to_buffer(const std::string& path, std::vector<char>& buffer) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    buffer.resize(static_cast<size_t>(size));
    return !!file.read(buffer.data(), size);
}

ZipExtractResult extract_zip(const std::string& zip_path) {
    ZipExtractResult result;
    result.success = false;

    duckdb_miniz::mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    bool zip_opened = false;

    try {
        // Create temp extraction directory
        std::string extract_dir = create_temp_download_dir();
        result.extract_dir = extract_dir;

        // Read zip file into memory (DuckDB's miniz is built with MINIZ_NO_STDIO,
        // so file-based APIs like mz_zip_reader_init_file are not available)
        std::vector<char> zip_data;
        if (!read_file_to_buffer(zip_path, zip_data)) {
            result.error_message = "Failed to read zip file: " + zip_path;
            cleanup_temp_dir(extract_dir);
            return result;
        }

        // Open the zip archive from memory
        if (!duckdb_miniz::mz_zip_reader_init_mem(&zip_archive, zip_data.data(), zip_data.size(), 0)) {
            result.error_message = "Failed to parse zip file: " + zip_path;
            cleanup_temp_dir(extract_dir);
            return result;
        }
        zip_opened = true;

        // Get number of files in archive
        int num_files = (int)duckdb_miniz::mz_zip_reader_get_num_files(&zip_archive);

        // Extract each file
        for (int i = 0; i < num_files; i++) {
            duckdb_miniz::mz_zip_archive_file_stat file_stat;
            if (!duckdb_miniz::mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
                result.error_message = "Failed to read file stat at index " + std::to_string(i);
                duckdb_miniz::mz_zip_reader_end(&zip_archive);
                cleanup_temp_dir(extract_dir);
                return result;
            }

            // Skip directory entries
            if (duckdb_miniz::mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
                continue;
            }

            // Normalize the filename (convert backslashes to forward slashes)
            std::string filename = normalize_path(file_stat.m_filename);

            // Build full output path
            std::string output_path = extract_dir;
            if (output_path.back() != '/' && output_path.back() != '\\') {
                output_path += "/";
            }
            output_path += filename;

            // Create parent directories for this file
            create_parent_dirs(output_path);

            // Extract file to heap, then write to disk
            size_t uncomp_size = 0;
            void *extracted = duckdb_miniz::mz_zip_reader_extract_to_heap(&zip_archive, i, &uncomp_size, 0);
            if (!extracted) {
                result.error_message = "Failed to extract file: " + filename;
                duckdb_miniz::mz_zip_reader_end(&zip_archive);
                cleanup_temp_dir(extract_dir);
                return result;
            }

            // Write extracted data to disk
            std::ofstream outfile(output_path, std::ios::binary);
            if (!outfile) {
                duckdb_miniz::mz_free(extracted);
                result.error_message = "Failed to create output file: " + output_path;
                duckdb_miniz::mz_zip_reader_end(&zip_archive);
                cleanup_temp_dir(extract_dir);
                return result;
            }
            outfile.write(static_cast<const char*>(extracted), uncomp_size);
            outfile.close();
            duckdb_miniz::mz_free(extracted);

            if (!outfile) {
                result.error_message = "Failed to write file: " + output_path;
                duckdb_miniz::mz_zip_reader_end(&zip_archive);
                cleanup_temp_dir(extract_dir);
                return result;
            }

            // Add to list of extracted files
            result.extracted_files.push_back(filename);
        }

        // Close the archive
        duckdb_miniz::mz_zip_reader_end(&zip_archive);
        zip_opened = false;

        result.success = true;
        return result;

    } catch (const std::exception& e) {
        result.error_message = "Exception during zip extraction: " + std::string(e.what());
        if (zip_opened) {
            duckdb_miniz::mz_zip_reader_end(&zip_archive);
        }
        if (!result.extract_dir.empty()) {
            cleanup_temp_dir(result.extract_dir);
        }
        return result;
    } catch (...) {
        result.error_message = "Unknown exception during zip extraction";
        if (zip_opened) {
            duckdb_miniz::mz_zip_reader_end(&zip_archive);
        }
        if (!result.extract_dir.empty()) {
            cleanup_temp_dir(result.extract_dir);
        }
        return result;
    }
}

void cleanup_extract_dir(const std::string& dir_path) {
    cleanup_temp_dir(dir_path);
}

} // namespace duckdb
