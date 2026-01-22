#define DUCKDB_EXTENSION_MAIN

#include "gdpdu_extension.hpp"
#include "gdpdu_importer.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension/extension_loader.hpp"

#include <iostream>

namespace duckdb {

// Bind data: stores the directory path argument
struct GdpduImportBindData : public TableFunctionData {
    std::string directory_path;
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
    
    // Get directory path argument
    bind_data->directory_path = input.inputs[0].GetValue<string>();
    
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
    
    state->results = import_gdpdu(conn, bind_data.directory_path);
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

} // namespace duckdb

// DuckDB 1.4+ extension entry point using the new macro
extern "C" {
DUCKDB_CPP_EXTENSION_ENTRY(gdpdu, loader) {
    // Set extension description
    loader.SetDescription("Import GDPdU (German tax audit) exports into DuckDB");
    
    // Create and register table function
    duckdb::TableFunction gdpdu_import(
        "gdpdu_import",
        {duckdb::LogicalType::VARCHAR},
        duckdb::GdpduImportScan,
        duckdb::GdpduImportBind,
        duckdb::GdpduImportInit
    );
    
    loader.RegisterFunction(gdpdu_import);
    
    std::cout << "GDPdU extension loaded successfully!" << std::endl;
}
}
