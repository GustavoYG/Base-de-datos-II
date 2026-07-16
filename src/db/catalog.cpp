#include "db/catalog.h"

#include <fstream>
#include <iostream>
#include <cctype>
#include <filesystem>

#include "io/csv_reader.h"

namespace fs = std::filesystem;

const std::string Catalog::kTablesDir = "data/tables";

// --- Helpers de serializacion binaria (little-endian, longitud-prefijada) ---
static void WriteI32(std::ostream& os, int32_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(int32_t));
}
static bool ReadI32(std::istream& is, int32_t& v) {
    return (bool)is.read(reinterpret_cast<char*>(&v), sizeof(int32_t));
}
static void WriteStr(std::ostream& os, const std::string& s) {
    int32_t n = (int32_t)s.size();
    WriteI32(os, n);
    if (n > 0) os.write(s.data(), n);
}
static bool ReadStr(std::istream& is, std::string& s) {
    int32_t n;
    if (!ReadI32(is, n)) return false;
    if (n < 0) return false;
    s.resize((size_t)n);
    if (n > 0 && !is.read(&s[0], n)) return false;
    return true;
}

std::string LoadedTable::TableNameFromPath(const std::string& path) {
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

int LoadedTable::GetColumnIndex(const std::string& col) const {
    std::string low = col;
    for (char& c : low) c = (char)std::tolower((unsigned char)c);
    for (size_t i = 0; i < columns.size(); ++i) {
        std::string c2 = columns[i];
        for (char& c : c2) c = (char)std::tolower((unsigned char)c);
        if (c2 == low) return (int)i;
    }
    return -1;
}

static bool FileExists(const std::string& p) {
    std::ifstream f(p.c_str());
    return (bool)f;
}

Catalog::Catalog() {
    LoadAllTables();
}

bool Catalog::LoadCsv(const std::string& path) {
    std::string base = LoadedTable::TableNameFromPath(path);

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

    std::vector<std::vector<std::string>> rows = io::CsvReader_ReadAll(resolved);
    if (rows.empty()) return false; // archivo inexistente o sin cabecera

    LoadedTable t;
    t.name = base;
    t.columns = rows[0];
    for (size_t i = 1; i < rows.size(); ++i) t.rows.push_back(rows[i]);
    tables_[base] = std::move(t);

    // Persistir en disco para que sobreviva al cierre del programa.
    SaveTable(tables_[base]);
    return true;
}

bool Catalog::Exists(const std::string& name) const {
    return tables_.count(name) > 0;
}

LoadedTable* Catalog::Get(const std::string& name) {
    auto it = tables_.find(name);
    return (it == tables_.end()) ? nullptr : &it->second;
}

const LoadedTable* Catalog::Get(const std::string& name) const {
    auto it = tables_.find(name);
    return (it == tables_.end()) ? nullptr : &it->second;
}

void Catalog::PrintLoadedTables() const {
    for (const auto& kv : tables_) {
        std::cout << "  - " << kv.first << " (" << kv.second.rows.size() << " filas)\n";
    }
}

// --- Persistencia ---

bool Catalog::SaveTable(const LoadedTable& t) {
    std::error_code ec;
    fs::create_directories(kTablesDir, ec);

    std::string file = kTablesDir + "/" + t.name + ".tbl";
    std::ofstream os(file, std::ios::binary | std::ios::trunc);
    if (!os) return false;

    os.write("TBL1", 4);                       // magic
    WriteI32(os, (int32_t)t.columns.size());   // nro de columnas
    for (const auto& c : t.columns) WriteStr(os, c);
    WriteI32(os, (int32_t)t.rows.size());      // nro de filas
    for (const auto& r : t.rows) {
        WriteI32(os, (int32_t)r.size());       // nro de celdas por fila
        for (const auto& cell : r) WriteStr(os, cell);
    }
    return (bool)os;
}

bool Catalog::LoadTableFile(const std::string& file) {
    std::ifstream is(file, std::ios::binary);
    if (!is) return false;

    char magic[5] = {0};
    is.read(magic, 4);
    if (std::string(magic) != "TBL1") return false;

    LoadedTable t;
    int32_t nCols = 0;
    if (!ReadI32(is, nCols) || nCols < 0) return false;
    t.columns.resize((size_t)nCols);
    for (int32_t i = 0; i < nCols; ++i) {
        if (!ReadStr(is, t.columns[i])) return false;
    }

    int32_t nRows = 0;
    if (!ReadI32(is, nRows) || nRows < 0) return false;
    t.rows.resize((size_t)nRows);
    for (int32_t r = 0; r < nRows; ++r) {
        int32_t nCells = 0;
        if (!ReadI32(is, nCells) || nCells < 0) return false;
        t.rows[r].resize((size_t)nCells);
        for (int32_t c = 0; c < nCells; ++c) {
            if (!ReadStr(is, t.rows[r][c])) return false;
        }
    }
    if (!is) return false;

    // Nombre de la tabla = base del archivo (sin .tbl).
    std::string base = file;
    size_t s = base.find_last_of("/\\");
    if (s != std::string::npos) base = base.substr(s + 1);
    size_t d = base.find_last_of('.');
    if (d != std::string::npos) base = base.substr(0, d);
    t.name = base;

    tables_[base] = std::move(t);
    return true;
}

void Catalog::SaveAllTables() {
    for (const auto& kv : tables_) SaveTable(kv.second);
}

void Catalog::LoadAllTables() {
    std::error_code ec;
    if (!fs::exists(kTablesDir, ec)) return;
    for (const auto& entry : fs::directory_iterator(kTablesDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".tbl") LoadTableFile(entry.path().string());
    }
}
