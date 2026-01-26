#include "gdpdu_extension.hpp"
#include "gdpdu_importer.hpp"
#include "gdpdu_exporter.hpp"
#include "generic_xml_importer.hpp"
#include "folder_importer.hpp"
#include "xml_parser_config.hpp"
#include "xml_parser_registration.hpp"
#include "duckdb.hpp"
#include "duckdb/main/extension.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/function/function_set.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/connection.hpp"

namespace duckdb {

// Bind data: stores the directory path and column name field argument
struct GdpduImportBindData : public TableFunctionData {
    std::string directory_path;
    std::string column_name_field;
};

// Global state: stores import results and current position
struct GdpduImportGlobalState : public GlobalTableFunctionState {
    std::vector<ImportResult> results;
    idx_t current_row;
    bool done;

    GdpduImportGlobalState() : current_row(0), done(false) {}
};

// Bind function: validates arguments and defines return columns
static unique_ptr<FunctionData> GdpduImportBind(
    ClientContext &context,
    TableFunctionBindInput &input,
    vector<LogicalType> &return_types,
    vector<string> &names
) {
    auto bind_data = make_uniq<GdpduImportBindData>();

    // Get directory path argument (required)
    bind_data->directory_path = input.inputs[0].GetValue<string>();

    // Get column_name_field argument (optional, defaults to "Name")
    if (input.inputs.size() > 1 && !input.inputs[1].IsNull()) {
        bind_data->column_name_field = input.inputs[1].GetValue<string>();
    } else {
        bind_data->column_name_field = "Name";
    }

    // Define return columns
    return_types.push_back(LogicalType::VARCHAR);  // table_name
    names.push_back("table_name");

    return_types.push_back(LogicalType::BIGINT);   // row_count
    names.push_back("row_count");

    return_types.push_back(LogicalType::VARCHAR);  // status
    names.push_back("status");

    return std::move(bind_data);
}

// Init function: runs the import and stores results
static unique_ptr<GlobalTableFunctionState> GdpduImportInit(
    ClientContext &context,
    TableFunctionInitInput &input
) {
    auto state = make_uniq<GdpduImportGlobalState>();

    // Get bind data
    auto &bind_data = input.bind_data->Cast<GdpduImportBindData>();

    // Create connection and run import
    auto &db = DatabaseInstance::GetDatabase(context);
    Connection conn(db);

    state->results = import_gdpdu_navision(conn, bind_data.directory_path, bind_data.column_name_field);
    state->current_row = 0;
    state->done = state->results.empty();

    return std::move(state);
}

// Scan function: outputs result rows
static void GdpduImportScan(
    ClientContext &context,
    TableFunctionInput &data,
    DataChunk &output
) {
    auto &state = data.global_state->Cast<GdpduImportGlobalState>();

    if (state.done) {
        return;
    }

    idx_t count = 0;
    idx_t max_count = STANDARD_VECTOR_SIZE;

    while (state.current_row < state.results.size() && count < max_count) {
        auto &result = state.results[state.current_row];

        output.SetValue(0, count, Value(result.table_name));
        output.SetValue(1, count, Value(result.row_count));
        output.SetValue(2, count, Value(result.status));

        state.current_row++;
        count++;
    }

    output.SetCardinality(count);

    if (state.current_row >= state.results.size()) {
        state.done = true;
    }
}

// Bind data for generic XML import
struct XmlImportBindData : public TableFunctionData {
    std::string directory_path;
    std::string parser_type;
    XmlParserConfig config;
};

// Global state for generic XML import (reuse same structure)
struct XmlImportGlobalState : public GlobalTableFunctionState {
    std::vector<ImportResult> results;
    idx_t current_row;
    bool done;

    XmlImportGlobalState() : current_row(0), done(false) {}
};

// Bind function for generic XML import
static unique_ptr<FunctionData> XmlImportBind(
    ClientContext &context,
    TableFunctionBindInput &input,
    vector<LogicalType> &return_types,
    vector<string> &names
) {
    auto bind_data = make_uniq<XmlImportBindData>();

    // Get directory path argument (required)
    bind_data->directory_path = input.inputs[0].GetValue<string>();

    // Get parser_type argument (optional, defaults to "gdpdu")
    if (input.inputs.size() > 1 && !input.inputs[1].IsNull()) {
        bind_data->parser_type = input.inputs[1].GetValue<string>();
    } else {
        bind_data->parser_type = "gdpdu";
    }

    // Configure default settings for GDPdU
    bind_data->config.parser_type = bind_data->parser_type;
    bind_data->config.index_file = "index.xml";
    bind_data->config.root_element = "DataSet/Media";
    bind_data->config.table_element = "Table";
    bind_data->config.column_element = "VariableColumn";
    bind_data->config.primary_key_element = "VariablePrimaryKey";
    bind_data->config.delimiter = ";";
    bind_data->config.has_header = false;
    bind_data->config.decimal_symbol = ',';
    bind_data->config.digit_grouping = '.';
    
    // Default element mappings
    bind_data->config.table_mapping.name_field = "Name";
    bind_data->config.table_mapping.url_field = "URL";
    bind_data->config.table_mapping.description_field = "Description";
    bind_data->config.column_mapping.name_field = "Name";
    bind_data->config.column_mapping.type_field = "";
    bind_data->config.column_mapping.precision_field = "";

    // Define return columns (same as GDPdU import)
    return_types.push_back(LogicalType::VARCHAR);  // table_name
    names.push_back("table_name");

    return_types.push_back(LogicalType::BIGINT);   // row_count
    names.push_back("row_count");

    return_types.push_back(LogicalType::VARCHAR);  // status
    names.push_back("status");

    return std::move(bind_data);
}

// Init function for generic XML import
static unique_ptr<GlobalTableFunctionState> XmlImportInit(
    ClientContext &context,
    TableFunctionInitInput &input
) {
    auto state = make_uniq<XmlImportGlobalState>();

    // Get bind data
    auto &bind_data = input.bind_data->Cast<XmlImportBindData>();

    // Create connection and run import
    auto &db = DatabaseInstance::GetDatabase(context);
    Connection conn(db);

    state->results = import_xml_data(conn, bind_data.directory_path, bind_data.parser_type, bind_data.config);
    state->current_row = 0;
    state->done = state->results.empty();

    return std::move(state);
}

// Scan function for generic XML import (reuse same logic)
static void XmlImportScan(
    ClientContext &context,
    TableFunctionInput &data,
    DataChunk &output
) {
    auto &state = data.global_state->Cast<XmlImportGlobalState>();

    if (state.done) {
        return;
    }

    idx_t count = 0;
    idx_t max_count = STANDARD_VECTOR_SIZE;

    while (state.current_row < state.results.size() && count < max_count) {
        auto &result = state.results[state.current_row];

        output.SetValue(0, count, Value(result.table_name));
        output.SetValue(1, count, Value(result.row_count));
        output.SetValue(2, count, Value(result.status));

        state.current_row++;
        count++;
    }

    output.SetCardinality(count);

    if (state.current_row >= state.results.size()) {
        state.done = true;
    }
}

// Standalone load function following DuckDB extension pattern
static void LoadInternal(ExtensionLoader &loader) {
    // Register XML parsers
    register_xml_parsers();
    
    // Create table function set to support both:
    // - import_gdpdu_navision('path')                           -> uses "Name" for column names
    // - import_gdpdu_navision('path', 'Description')            -> uses "Description" for column names
    TableFunctionSet gdpdu_import_set("import_gdpdu_navision");

    // Single argument version (directory_path only)
    TableFunction gdpdu_import_1arg(
        "import_gdpdu_navision",
        {LogicalType::VARCHAR},
        GdpduImportScan,
        GdpduImportBind,
        GdpduImportInit
    );
    gdpdu_import_set.AddFunction(gdpdu_import_1arg);

    // Two argument version (directory_path, column_name_field)
    TableFunction gdpdu_import_2args(
        "import_gdpdu_navision",
        {LogicalType::VARCHAR, LogicalType::VARCHAR},
        GdpduImportScan,
        GdpduImportBind,
        GdpduImportInit
    );
    gdpdu_import_set.AddFunction(gdpdu_import_2args);

    // Register with the extension loader
    loader.RegisterFunction(gdpdu_import_set);

    // Create table function for DATEV GDPdU import
    // DATEV uses standard GDPdU format with "Name" element for column names
    TableFunctionSet gdpdu_datev_import_set("import_gdpdu_datev");

    // Bind function for DATEV import (only directory_path argument)
    auto GdpduDatevImportBind = [](ClientContext &context,
                                   TableFunctionBindInput &input,
                                   vector<LogicalType> &return_types,
                                   vector<string> &names) -> unique_ptr<FunctionData> {
        auto bind_data = make_uniq<GdpduImportBindData>();
        bind_data->directory_path = input.inputs[0].GetValue<string>();
        bind_data->column_name_field = "Name";  // DATEV uses Name element

        // Define return columns
        return_types.push_back(LogicalType::VARCHAR);  // table_name
        names.push_back("table_name");
        return_types.push_back(LogicalType::BIGINT);   // row_count
        names.push_back("row_count");
        return_types.push_back(LogicalType::VARCHAR);  // status
        names.push_back("status");

        return std::move(bind_data);
    };

    // Init function for DATEV import
    auto GdpduDatevImportInit = [](ClientContext &context,
                                   TableFunctionInitInput &input) -> unique_ptr<GlobalTableFunctionState> {
        auto state = make_uniq<GdpduImportGlobalState>();
        auto &bind_data = input.bind_data->Cast<GdpduImportBindData>();
        auto &db = DatabaseInstance::GetDatabase(context);
        Connection conn(db);

        state->results = import_gdpdu_datev(conn, bind_data.directory_path);
        state->current_row = 0;
        state->done = state->results.empty();

        return std::move(state);
    };

    // Single argument version (directory_path only)
    TableFunction gdpdu_datev_import_func(
        "import_gdpdu_datev",
        {LogicalType::VARCHAR},
        GdpduImportScan,  // Reuse the same scan function
        GdpduDatevImportBind,
        GdpduDatevImportInit
    );
    gdpdu_datev_import_set.AddFunction(gdpdu_datev_import_func);

    // Register with the extension loader
    loader.RegisterFunction(gdpdu_datev_import_set);

    // Register generic import_xml_data function
    TableFunctionSet xml_import_set("import_xml_data");
    
    // Single argument version (directory_path only, uses "gdpdu" parser)
    TableFunction xml_import_1arg(
        "import_xml_data",
        {LogicalType::VARCHAR},
        XmlImportScan,
        XmlImportBind,
        XmlImportInit
    );
    xml_import_set.AddFunction(xml_import_1arg);
    
    // Two argument version (directory_path, parser_type)
    TableFunction xml_import_2args(
        "import_xml_data",
        {LogicalType::VARCHAR, LogicalType::VARCHAR},
        XmlImportScan,
        XmlImportBind,
        XmlImportInit
    );
    xml_import_set.AddFunction(xml_import_2args);
    
    // Register with the extension loader
    loader.RegisterFunction(xml_import_set);
    
    // Register import_folder function
    TableFunctionSet folder_import_set("import_folder");
    
    // Bind data for folder import
    struct FolderImportBindData : public TableFunctionData {
        std::string folder_path;
        std::string file_type;
    };
    
    // Global state for folder import
    struct FolderImportGlobalState : public GlobalTableFunctionState {
        std::vector<FileImportResult> results;
        idx_t current_row;
        bool done;
        
        FolderImportGlobalState() : current_row(0), done(false) {}
    };
    
    // Bind function for folder import
    auto FolderImportBind = [](ClientContext &context,
                               TableFunctionBindInput &input,
                               vector<LogicalType> &return_types,
                               vector<string> &names) -> unique_ptr<FunctionData> {
        auto bind_data = make_uniq<FolderImportBindData>();
        
        // Get folder path (required)
        bind_data->folder_path = input.inputs[0].GetValue<string>();
        
        // Get file_type argument (optional, defaults to "csv")
        if (input.inputs.size() > 1 && !input.inputs[1].IsNull()) {
            bind_data->file_type = input.inputs[1].GetValue<string>();
        } else {
            bind_data->file_type = "csv";
        }
        
        // Define return columns
        return_types.push_back(LogicalType::VARCHAR);  // table_name
        names.push_back("table_name");
        
        return_types.push_back(LogicalType::VARCHAR);  // file_name
        names.push_back("file_name");
        
        return_types.push_back(LogicalType::BIGINT);   // row_count
        names.push_back("row_count");
        
        return_types.push_back(LogicalType::INTEGER);  // column_count
        names.push_back("column_count");
        
        return_types.push_back(LogicalType::VARCHAR);  // status
        names.push_back("status");
        
        return std::move(bind_data);
    };
    
    // Init function for folder import
    auto FolderImportInit = [](ClientContext &context,
                               TableFunctionInitInput &input) -> unique_ptr<GlobalTableFunctionState> {
        auto state = make_uniq<FolderImportGlobalState>();
        
        // Get bind data
        auto &bind_data = input.bind_data->Cast<FolderImportBindData>();
        
        // Create connection and run import
        auto &db = DatabaseInstance::GetDatabase(context);
        Connection conn(db);
        
        state->results = import_folder(conn, bind_data.folder_path, bind_data.file_type);
        state->current_row = 0;
        state->done = state->results.empty();
        
        return std::move(state);
    };
    
    // Scan function for folder import
    auto FolderImportScan = [](ClientContext &context,
                              TableFunctionInput &data,
                              DataChunk &output) -> void {
        auto &state = data.global_state->Cast<FolderImportGlobalState>();
        
        if (state.done) {
            return;
        }
        
        idx_t count = 0;
        idx_t max_count = STANDARD_VECTOR_SIZE;
        
        while (state.current_row < state.results.size() && count < max_count) {
            auto &result = state.results[state.current_row];
            
            output.SetValue(0, count, Value(result.table_name));
            output.SetValue(1, count, Value(result.file_name));
            output.SetValue(2, count, Value(result.row_count));
            output.SetValue(3, count, Value(static_cast<int32_t>(result.column_count)));
            output.SetValue(4, count, Value(result.status));
            
            state.current_row++;
            count++;
        }
        
        output.SetCardinality(count);
        
        if (state.current_row >= state.results.size()) {
            state.done = true;
        }
    };
    
    // Single argument version (folder_path only, defaults to "csv")
    TableFunction folder_import_1arg(
        "import_folder",
        {LogicalType::VARCHAR},
        FolderImportScan,
        FolderImportBind,
        FolderImportInit
    );
    folder_import_set.AddFunction(folder_import_1arg);
    
    // Two argument version (folder_path, file_type)
    TableFunction folder_import_2args(
        "import_folder",
        {LogicalType::VARCHAR, LogicalType::VARCHAR},
        FolderImportScan,
        FolderImportBind,
        FolderImportInit
    );
    folder_import_set.AddFunction(folder_import_2args);
    
    // Register with the extension loader
    loader.RegisterFunction(folder_import_set);
    
    // Register export_gdpdu table function
    // Bind data for export_gdpdu
    struct GdpduExportBindData : public TableFunctionData {
        std::string export_path;
        std::string table_name;
    };
    
    // Global state for export_gdpdu
    struct GdpduExportGlobalState : public GlobalTableFunctionState {
        ExportResult result;
        bool done;
        
        GdpduExportGlobalState() : done(false) {}
    };
    
    // Bind function for export_gdpdu
    auto GdpduExportBind = [](ClientContext &context,
                              TableFunctionBindInput &input,
                              vector<LogicalType> &return_types,
                              vector<string> &names) -> unique_ptr<FunctionData> {
        auto bind_data = make_uniq<GdpduExportBindData>();
        bind_data->export_path = input.inputs[0].GetValue<string>();
        bind_data->table_name = input.inputs[1].GetValue<string>();
        
        return_types.push_back(LogicalType::VARCHAR); // table_name
        names.push_back("table_name");
        return_types.push_back(LogicalType::VARCHAR); // file_path
        names.push_back("file_path");
        return_types.push_back(LogicalType::BIGINT);  // row_count
        names.push_back("row_count");
        return_types.push_back(LogicalType::VARCHAR); // status
        names.push_back("status");
        
        return std::move(bind_data);
    };
    
    // Init function for export_gdpdu
    auto GdpduExportInit = [](ClientContext &context,
                              TableFunctionInitInput &input) -> unique_ptr<GlobalTableFunctionState> {
        auto state = make_uniq<GdpduExportGlobalState>();
        
        auto &bind_data = input.bind_data->Cast<GdpduExportBindData>();
        auto &db = DatabaseInstance::GetDatabase(context);
        Connection conn(db);
        
        state->result = export_gdpdu(conn, bind_data.export_path, bind_data.table_name);
        state->done = false;
        
        return std::move(state);
    };
    
    // Scan function for export_gdpdu
    auto GdpduExportScan = [](ClientContext &context,
                             TableFunctionInput &data,
                             DataChunk &output) -> void {
        auto &state = data.global_state->Cast<GdpduExportGlobalState>();
        
        if (state.done) {
            return;
        }
        
        output.SetValue(0, 0, Value(state.result.table_name));
        output.SetValue(1, 0, Value(state.result.file_path));
        output.SetValue(2, 0, Value(state.result.row_count));
        output.SetValue(3, 0, Value(state.result.status));
        output.SetCardinality(1);
        
        state.done = true;
    };
    
    TableFunction export_gdpdu_func(
        "export_gdpdu",
        {LogicalType::VARCHAR, LogicalType::VARCHAR},
        GdpduExportScan,
        GdpduExportBind,
        GdpduExportInit
    );
    
    TableFunctionSet export_gdpdu_set("export_gdpdu");
    export_gdpdu_set.AddFunction(export_gdpdu_func);
    loader.RegisterFunction(export_gdpdu_set);
}

// Extension class implementation for DuckDB 1.4+
void GdpduExtension::Load(ExtensionLoader &loader) {
    LoadInternal(loader);
}

std::string GdpduExtension::Name() {
    return "gdpdu";
}

std::string GdpduExtension::Version() const {
#ifdef EXT_VERSION
    return EXT_VERSION;
#else
    return "v0.1.0";
#endif
}

} // namespace duckdb

// Entry point for the loadable extension using DuckDB 1.4+ CPP extension API
// Explicitly define with visibility attribute to ensure symbol is exported
extern "C" DUCKDB_EXTENSION_API void gdpdu_duckdb_cpp_init(duckdb::ExtensionLoader &loader) {
    duckdb::LoadInternal(loader);
}
