#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace duckdb {

// Configuration for XML element paths and mappings
struct XmlElementMapping {
    std::string xml_path;           // XPath-like path to element (e.g., "DataSet/Media/Table")
    std::string name_field;          // Element name for table/column name (e.g., "Name")
    std::string description_field;    // Element name for description
    std::string url_field;           // Element name for data file URL
    std::string type_field;          // Element name for data type
    std::string precision_field;     // Element name for precision/accuracy
    std::string primary_key_field;   // Element name for primary key indicator
};

// Configuration for XML parser
struct XmlParserConfig {
    std::string parser_type;                    // "gdpdu", "generic", etc.
    std::string index_file;                     // "index.xml" or custom
    std::string root_element;                   // Root element path (e.g., "DataSet/Media")
    std::string table_element;                  // Table container element (e.g., "Table")
    std::string column_element;                 // Column element (e.g., "VariableColumn")
    std::string primary_key_element;           // Primary key element (e.g., "VariablePrimaryKey")
    
    // Element mappings
    XmlElementMapping table_mapping;
    XmlElementMapping column_mapping;
    
    // Data file settings
    std::string delimiter;                      // CSV delimiter (default: ";")
    bool has_header;                           // CSV has header row (default: false)
    char decimal_symbol;                       // Decimal separator (default: ',')
    char digit_grouping;                      // Thousands separator (default: '.')
    
    // Type mapping configuration
    std::map<std::string, std::string> type_mappings;  // XML type -> DuckDB type
    
    XmlParserConfig() 
        : parser_type("generic")
        , index_file("index.xml")
        , root_element("")
        , table_element("Table")
        , column_element("VariableColumn")
        , primary_key_element("VariablePrimaryKey")
        , delimiter(";")
        , has_header(false)
        , decimal_symbol(',')
        , digit_grouping('.') {
        
        // Default type mappings
        type_mappings["AlphaNumeric"] = "VARCHAR";
        type_mappings["Numeric"] = "DECIMAL";
        type_mappings["Date"] = "DATE";
    }
};

// Generic schema structure (similar to GdpduSchema but more flexible)
struct XmlTableSchema {
    std::string name;
    std::string url;
    std::string description;
    std::vector<std::string> primary_key_columns;
    
    struct Column {
        std::string name;
        std::string duckdb_type;  // DuckDB type string (e.g., "VARCHAR", "DECIMAL(18,2)")
        bool is_primary_key;
        int precision;
        
        Column() : is_primary_key(false), precision(0) {}
    };
    
    std::vector<Column> columns;
    
    // Locale settings
    char decimal_symbol;
    char digit_grouping;
    bool is_utf8;
    int skip_lines;             // Lines to skip at start of file (from Range/From, default 0)
    
    XmlTableSchema() 
        : decimal_symbol(',')
        , digit_grouping('.')
        , is_utf8(false)
        , skip_lines(0) {}
};

struct XmlSchema {
    std::string media_name;
    std::vector<XmlTableSchema> tables;
};

// Abstract base class for XML parsers
class XmlParser {
public:
    virtual ~XmlParser() = default;
    
    // Parse XML and return schema
    virtual XmlSchema parse(const std::string& directory_path, const XmlParserConfig& config) = 0;
    
    // Get parser name/type
    virtual std::string get_parser_type() const = 0;
};

} // namespace duckdb
