#include "db/catalog.h"

#include <fstream>
#include <iostream>
#include <cctype>
#include <filesystem>

#include "io/csv_reader.h"
#include "io/csv_loader.h"
#include "io/schema_io.h"

namespace fs = std::filesystem;

const std::string Catalog::kTablesDir = "data/tables";

std::string StoredTable::TableNameFromPath(const std::string& path) {
    size_t s = path.find_last_of("/\\");
    std::string base = (s == std::string::npos) ? path : path.substr(s + 1);
    size_t d = base.find_last_of('.');
    if (d != std::string::npos) {
        std::string ext = base.substr(d);
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".csv") base = base.substr(0, d);
    }
    return base;
}

int StoredTable::GetColumnIndex(const std::string& col) const {
    return schema.GetColumnIndex(col);
}

static bool FileExists(const std::string& p) {
    std::ifstream f(p.c_str());
    return (bool)f;
}

Catalog::Catalog() {
    // Registrar todas las tablas persistidas leyendo sus .schema.
    std::error_code ec;
    if (!fs::exists(kTablesDir, ec)) return;
    for (const auto& entry : fs::directory_iterator(kTablesDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext != ".schema") continue;
        std::string base = entry.path().stem().string(); // nombre sin .schema
        RegisterFromSchema(base, entry.path().string());
    }
}

bool Catalog::RegisterFromSchema(const std::string& tableName, const std::string& schemaPath) {
    StoredTable t;
    if (!schemaio::LoadSchema(schemaPath, t.schema)) return false;
    t.name = tableName;
    t.binPath = kTablesDir + "/" + tableName + ".bin";
    tables_[tableName] = std::move(t);
    return true;
}

bool Catalog::LoadCsv(const std::string& path) {
    std::string base = StoredTable::TableNameFromPath(path);

    // Resolution amigable: prueba varias ubicaciones relativas.
    std::vector<std::string> cands = {
        path,
        "data/" + path,
        "data/raw/" + path,
        "data/raw/" + base + ".csv",
        "data/" + base + ".csv"
    };
    std::string resolved;
    for (const auto& c : cands) {
        if (FileExists(c)) { resolved = c; break; }
    }
    if (resolved.empty()) return false;

    // Carga dinamica al heap file en modo streaming (sin cargar el CSV en RAM).
    csvloader::CsvLoadResult res = csvloader::LoadCsvToStorage(resolved, base, kTablesDir);
    if (!res.ok) return false;

    // Registrar en el catalogo (lee el schema que el loader acaba de persistir).
    return RegisterFromSchema(base, schemaio::SchemaPath(kTablesDir, base));
}

bool Catalog::Exists(const std::string& name) const {
    return tables_.count(name) > 0;
}

StoredTable* Catalog::Get(const std::string& name) {
    auto it = tables_.find(name);
    return (it == tables_.end()) ? nullptr : &it->second;
}

const StoredTable* Catalog::Get(const std::string& name) const {
    auto it = tables_.find(name);
    return (it == tables_.end()) ? nullptr : &it->second;
}

std::vector<std::string> Catalog::GetAllTableNames() const {
    std::vector<std::string> names;
    names.reserve(tables_.size());
    for (const auto& kv : tables_) {
        names.push_back(kv.first);
    }
    return names;
}

void Catalog::PrintLoadedTables() const {
    for (const auto& kv : tables_) {
        std::cout << "  - " << kv.first << " (" << kv.second.schema.columns.size()
                  << " columnas, heap file: " << kv.second.binPath << ")\n";
    }
}

static ColumnType ParseColumnTypeStr(const std::string& typeStr) {
    std::string s = typeStr;
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    if (s == "int" || s == "int32" || s == "integer") return ColumnType::INT32;
    if (s == "int64" || s == "bigint") return ColumnType::INT64;
    if (s == "float" || s == "real") return ColumnType::FLOAT;
    if (s == "double") return ColumnType::DOUBLE;
    if (s == "bool" || s == "boolean") return ColumnType::BOOL;
    return ColumnType::STRING;
}

bool Catalog::CreateTable(const std::string& name, const std::vector<std::string>& columns) {
    if (Exists(name)) return false;
    std::error_code ec;
    fs::create_directories(kTablesDir, ec);
    Schema schema;
    for (const auto& rawCol : columns) {
        std::istringstream ss(rawCol);
        std::string colName, typeStr;
        ss >> colName;
        if (ss >> typeStr) {
            schema.AddColumn(colName, ParseColumnTypeStr(typeStr), DEFAULT_VARCHAR_LEN);
        } else {
            schema.AddColumn(colName, ColumnType::STRING, DEFAULT_VARCHAR_LEN);
        }
    }
    schema.Finalize();
    std::string schPath = schemaio::SchemaPath(kTablesDir, name);
    if (!schemaio::SaveSchema(schPath, schema)) return false;
    StoredTable t;
    t.name = name;
    t.schema = schema;
    t.binPath = kTablesDir + "/" + name + ".bin";
    tables_[name] = std::move(t);
    return true;
}

bool Catalog::DropTable(const std::string& name) {
    auto it = tables_.find(name);
    if (it == tables_.end()) return false;
    tables_.erase(it);
    std::error_code ec;
    fs::remove(kTablesDir + "/" + name + ".bin", ec);
    fs::remove(kTablesDir + "/" + name + ".bin.wal", ec);
    fs::remove(schemaio::SchemaPath(kTablesDir, name), ec);
    return true;
}
