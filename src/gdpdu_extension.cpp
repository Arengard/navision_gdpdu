#include "gdpdu_extension.hpp"
#include "gdpdu_importer.hpp"
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

// Standalone load function following DuckDB extension pattern
static void LoadInternal(ExtensionLoader &loader) {
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
