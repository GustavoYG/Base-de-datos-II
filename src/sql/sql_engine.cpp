#include "sql/sql_engine.h"

#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <functional>
#include <sstream>
#include <iomanip>

#include "db/catalog.h"
#include "common/utils.h"
#include "io/serializer.h"
#include "io/interactive_cli.h"
#include "storage/record_manager.h"
#include "sql/operator.h"
#include "sql/operators.h"
#include "index/index_manager.h"
#include "sql/benchmark.h"

namespace {

bool IsNumber(const std::string& s) {
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0';
}

// Divide por espacios conservando substrings entre comillas como un token.
std::vector<std::string> Tokenize(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    bool inQ = false;
    for (char c : s) {
        if (c == '"') { inQ = !inQ; cur += c; continue; }
        if (!inQ && std::isspace((unsigned char)c)) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// Separa una lista "a,b , c" por comas.
std::vector<std::string> SplitCommas(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            size_t a = cur.find_first_not_of(" \t\r\n");
            if (a != std::string::npos) {
                size_t b = cur.find_last_not_of(" \t\r\n");
                out.push_back(cur.substr(a, b - a + 1));
            }
            cur.clear();
        } else {
            cur += c;
        }
    }
    size_t a = cur.find_first_not_of(" \t\r\n");
    if (a != std::string::npos) {
        size_t b = cur.find_last_not_of(" \t\r\n");
        out.push_back(cur.substr(a, b - a + 1));
    }
    return out;
}

std::string StripQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

bool ParseOp(const std::string& raw, CmpOperator& op) {
    std::string r = ToLower(raw);
    if (r == "=" || r == "==") { op = CmpOperator::EQ; return true; }
    if (r == "!=" || r == "<>") { op = CmpOperator::NE; return true; }
    if (r == ">") { op = CmpOperator::GT; return true; }
    if (r == "<") { op = CmpOperator::LT; return true; }
    if (r == ">=") { op = CmpOperator::GE; return true; }
    if (r == "<=") { op = CmpOperator::LE; return true; }
    return false;
}

void PrintTable(const std::vector<std::string>& cols,
                const std::vector<std::vector<std::string>>& data) {
    if (cols.empty()) return;
    const int n = (int)cols.size();
    std::vector<int> w(n, 0);
    for (int i = 0; i < n; ++i) w[i] = (int)cols[i].size();
    for (const auto& row : data)
        for (int i = 0; i < n && i < (int)row.size(); ++i)
            if ((int)row[i].size() > w[i]) w[i] = (int)row[i].size();

    std::cout << "+";
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < w[i] + 2; ++j) std::cout << "-";
        std::cout << "+";
    }
    std::cout << "\n";

    std::cout << "|";
    for (int i = 0; i < n; ++i) {
        std::cout << " " << cols[i];
        int pad = w[i] - (int)cols[i].size();
        for (int p = 0; p < pad + 1; ++p) std::cout << " ";
        std::cout << "|";
    }
    std::cout << "\n";

    std::cout << "+";
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < w[i] + 2; ++j) std::cout << "-";
        std::cout << "+";
    }
    std::cout << "\n";

    for (const auto& r : data) {
        std::cout << "|";
        for (int i = 0; i < n; ++i) {
            std::string val = (i < (int)r.size()) ? r[i] : "";
            std::cout << " " << val;
            int pad = w[i] - (int)val.size();
            for (int p = 0; p < pad + 1; ++p) std::cout << " ";
            std::cout << "|";
        }
        std::cout << "\n";
    }

    std::cout << "+";
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < w[i] + 2; ++j) std::cout << "-";
        std::cout << "+";
    }
    std::cout << "\n";
}

static std::string ExtractParenContent(const std::string& s, size_t startPos, size_t& endPos) {
    size_t open = s.find('(', startPos);
    if (open == std::string::npos) return "";
    int depth = 0;
    for (size_t i = open; i < s.size(); ++i) {
        if (s[i] == '(') depth++;
        else if (s[i] == ')') { depth--; if (depth == 0) { endPos = i + 1; return s.substr(open + 1, i - open - 1); } }
    }
    return "";
}

static bool SerializeValue(const ColumnDef& col, const std::string& val, std::vector<unsigned char>& out) {
    out.resize(Schema::TypeSize(col.type, col.length));
    std::fill(out.begin(), out.end(), 0);
    switch (col.type) {
        case ColumnType::INT32: {
            int32_t v = std::strtol(val.c_str(), nullptr, 10);
            std::memcpy(out.data(), &v, sizeof(v));
            break;
        }
        case ColumnType::INT64: {
            int64_t v = std::strtoll(val.c_str(), nullptr, 10);
            std::memcpy(out.data(), &v, sizeof(v));
            break;
        }
        case ColumnType::FLOAT: {
            float v = std::strtof(val.c_str(), nullptr);
            std::memcpy(out.data(), &v, sizeof(v));
            break;
        }
        case ColumnType::DOUBLE: {
            double v = std::strtod(val.c_str(), nullptr);
            std::memcpy(out.data(), &v, sizeof(v));
            break;
        }
        case ColumnType::BOOL: {
            uint8_t v = 0;
            std::string low = ToLower(val);
            if (low == "1" || low == "true") v = 1;
            std::memcpy(out.data(), &v, sizeof(v));
            break;
        }
        default: {
            size_t n = std::min(val.size(), (size_t)col.length - 1);
            std::memcpy(out.data(), val.c_str(), n);
            out[n] = 0;
            break;
        }
    }
    return true;
}

// ---- INSERT INTO tabla (col1, col2) VALUES (val1, val2) ----
static std::string ExecuteInsert(Catalog& catalog, const std::string& rawQuery) {
    std::string q = rawQuery;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    std::vector<std::string> toks = Tokenize(q);
    if (toks.size() < 4 || ToLower(toks[0]) != "insert" || ToLower(toks[1]) != "into")
        return "Error de sintaxis: se esperaba INSERT INTO tabla ...";

    std::string table = toks[2];
    const StoredTable* t = catalog.Get(table);
    if (!t) return "Error: La tabla \"" + table + "\" no existe.";

    size_t pos = q.find(table);
    pos += table.size();

    size_t afterCols = 0;
    std::string colsRaw = ExtractParenContent(q, pos, afterCols);
    if (colsRaw.empty()) return "Error de sintaxis: falta lista de columnas (col1, col2, ...).";
    std::vector<std::string> cols = SplitCommas(colsRaw);

    size_t afterVals = 0;
    std::string valsRaw = ExtractParenContent(q, afterCols, afterVals);
    if (valsRaw.empty()) return "Error de sintaxis: falta lista de valores (val1, val2, ...).";
    std::vector<std::string> vals = SplitCommas(valsRaw);

    if (cols.size() != vals.size())
        return "Error: la cantidad de columnas y valores no coincide.";

    std::vector<unsigned char> row(t->schema.rowSize);
    for (size_t i = 0; i < cols.size(); ++i) {
        int idx = t->GetColumnIndex(cols[i]);
        if (idx < 0) return "Error: la columna \"" + cols[i] + "\" no existe en \"" + table + "\".";
        const ColumnDef& cdef = t->schema.columns[idx];
        std::vector<unsigned char> fBytes;
        SerializeValue(cdef, StripQuotes(vals[i]), fBytes);
        std::memcpy(row.data() + cdef.offset, fBytes.data(), fBytes.size());
    }

    RecordManager rm(t->binPath, t->schema, ReplacementPolicy::LRU);
    int pId, slot;
    if (!rm.InsertRecord(row, pId, slot)) {
        return "Error: no se pudo insertar el registro.";
    }

    // Reconstruir indices si existen
    for (const auto& cdef : t->schema.columns) {
        if (IndexManager::HasIndex(table, cdef.name)) {
            IndexManager::BuildIndex(catalog, table, cdef.name);
        }
    }

    return "OK. 1 fila insertada en \"" + table + "\".";
}

// ---- DELETE FROM tabla WHERE col op val ----
static std::string ExecuteDelete(Catalog& catalog, const std::string& rawQuery) {
    std::string q = rawQuery;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    std::vector<std::string> toks = Tokenize(q);
    if (toks.size() < 3 || ToLower(toks[0]) != "delete" || ToLower(toks[1]) != "from")
        return "Error de sintaxis: se esperaba DELETE FROM tabla ...";

    std::string table = toks[2];
    const StoredTable* t = catalog.Get(table);
    if (!t) return "Error: La tabla \"" + table + "\" no existe.";

    ScanOperator scanOp(*t);
    std::unique_ptr<Operator> plan = std::make_unique<ScanOperator>(*t);

    if (toks.size() >= 7 && ToLower(toks[3]) == "where") {
        CmpOperator op;
        if (ParseOp(toks[5], op)) {
            plan = std::make_unique<SelectOperator>(std::move(plan), toks[4], op, StripQuotes(toks[6]));
        }
    }

    RecordManager rm(t->binPath, t->schema, ReplacementPolicy::LRU);
    plan->Open();
    Tuple tuple;
    int deleted = 0;

    std::vector<std::pair<int, int>> locs;
    rm.ScanAll(locs);

    int whereIdx = (toks.size() >= 7) ? t->GetColumnIndex(toks[4]) : -1;
    CmpOperator wOp;
    if (toks.size() >= 7) ParseOp(toks[5], wOp);

    for (const auto& loc : locs) {
        std::vector<unsigned char> row;
        if (!rm.ReadRecord(loc.first, loc.second, row)) continue;

        bool matches = true;
        if (whereIdx >= 0) {
            const ColumnDef& cdef = t->schema.columns[whereIdx];
            std::string cellVal;
            switch (cdef.type) {
                case ColumnType::INT32:  cellVal = std::to_string(GetFieldInt32(t->schema, row, cdef.name)); break;
                case ColumnType::INT64:  cellVal = std::to_string(GetFieldInt64(t->schema, row, cdef.name)); break;
                case ColumnType::FLOAT:  cellVal = std::to_string(GetFieldFloat(t->schema, row, cdef.name)); break;
                case ColumnType::DOUBLE: cellVal = std::to_string(GetFieldDouble(t->schema, row, cdef.name)); break;
                case ColumnType::BOOL:   cellVal = GetFieldBool(t->schema, row, cdef.name) ? "true" : "false"; break;
                default:                 cellVal = GetFieldString(t->schema, row, cdef.name); break;
            }
            if (toks.size() >= 7) {
                int cmp = cellVal.compare(StripQuotes(toks[6]));
                if (IsNumber(cellVal) && IsNumber(StripQuotes(toks[6]))) {
                    double a = std::strtod(cellVal.c_str(), nullptr);
                    double b = std::strtod(StripQuotes(toks[6]).c_str(), nullptr);
                    cmp = (a < b) ? -1 : (a > b ? 1 : 0);
                }
                switch (wOp) {
                    case CmpOperator::EQ: matches = (cmp == 0); break;
                    case CmpOperator::NE: matches = (cmp != 0); break;
                    case CmpOperator::GT: matches = (cmp > 0); break;
                    case CmpOperator::LT: matches = (cmp < 0); break;
                    case CmpOperator::GE: matches = (cmp >= 0); break;
                    case CmpOperator::LE: matches = (cmp <= 0); break;
                }
            }
        }

        if (matches) {
            rm.DeleteRecord(loc.first, loc.second);
            deleted++;
        }
    }
    plan->Close();

    // Reconstruir indices si existen
    for (const auto& cdef : t->schema.columns) {
        if (IndexManager::HasIndex(table, cdef.name)) {
            IndexManager::BuildIndex(catalog, table, cdef.name);
        }
    }

    return "OK. " + std::to_string(deleted) + " fila(s) eliminada(s) de \"" + table + "\".";
}

// ---- UPDATE tabla SET col = val WHERE col op val ----
static std::string ExecuteUpdate(Catalog& catalog, const std::string& rawQuery) {
    std::string q = rawQuery;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    std::vector<std::string> toks = Tokenize(q);
    if (toks.size() < 6 || ToLower(toks[0]) != "update" || ToLower(toks[2]) != "set")
        return "Error de sintaxis: se esperaba UPDATE tabla SET col = val ...";

    std::string table = toks[1];
    const StoredTable* t = catalog.Get(table);
    if (!t) return "Error: La tabla \"" + table + "\" no existe.";

    std::string setCol = toks[3];
    std::string setVal = StripQuotes(toks[5]);

    int setIdx = t->GetColumnIndex(setCol);
    if (setIdx < 0) return "Error: la columna \"" + setCol + "\" no existe en \"" + table + "\".";

    RecordManager rm(t->binPath, t->schema, ReplacementPolicy::LRU);
    std::vector<std::pair<int, int>> locs;
    rm.ScanAll(locs);

    int updated = 0;
    for (const auto& loc : locs) {
        std::vector<unsigned char> row;
        if (!rm.ReadRecord(loc.first, loc.second, row)) continue;

        const ColumnDef& cdef = t->schema.columns[setIdx];
        std::vector<unsigned char> fBytes;
        SerializeValue(cdef, setVal, fBytes);
        std::memcpy(row.data() + cdef.offset, fBytes.data(), fBytes.size());

        rm.UpdateRecord(loc.first, loc.second, row);
        updated++;
    }

    // Reconstruir indices si existen
    for (const auto& cdef : t->schema.columns) {
        if (IndexManager::HasIndex(table, cdef.name)) {
            IndexManager::BuildIndex(catalog, table, cdef.name);
        }
    }

    return "OK. " + std::to_string(updated) + " fila(s) actualizada(s) en \"" + table + "\".";
}

// ---- CREATE TABLE nombre (col1, col2, ...) ----
static std::string ExecuteCreateTable(Catalog& catalog, const std::string& rawQuery) {
    std::string q = rawQuery;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    std::vector<std::string> toks = Tokenize(q);
    if (toks.size() < 3 || ToLower(toks[0]) != "create" || ToLower(toks[1]) != "table")
        return "Error de sintaxis: se esperaba CREATE TABLE nombre (col1, col2, ...).";

    std::string table = toks[2];
    size_t parenIdx = table.find('(');
    if (parenIdx != std::string::npos) {
        table = table.substr(0, parenIdx);
    }
    if (table.empty()) return "Error de sintaxis: falta nombre de la tabla.";

    if (catalog.Exists(table)) return "Error: La tabla \"" + table + "\" ya existe.";

    size_t afterParen = 0;
    std::string colsRaw = ExtractParenContent(q, 0, afterParen);
    if (colsRaw.empty()) return "Error de sintaxis: falta lista de columnas (col1, col2, ...).";

    std::vector<std::string> cols = SplitCommas(colsRaw);
    if (cols.empty()) return "Error de sintaxis: lista de columnas vacia.";

    if (!catalog.CreateTable(table, cols))
        return "Error: No se pudo crear la tabla \"" + table + "\".";

    return "OK. Tabla \"" + table + "\" creada con " + std::to_string(cols.size()) + " columnas.";
}

// ---- DROP TABLE nombre ----
static std::string ExecuteDropTable(Catalog& catalog, const std::string& rawQuery) {
    std::string q = rawQuery;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    std::vector<std::string> toks = Tokenize(q);
    if (toks.size() < 3 || ToLower(toks[0]) != "drop" || ToLower(toks[1]) != "table")
        return "Error de sintaxis: se esperaba DROP TABLE nombre.";

    std::string table = toks[2];
    if (!catalog.Exists(table)) return "Error: La tabla \"" + table + "\" no existe.";

    if (!catalog.DropTable(table)) return "Error: No se pudo eliminar la tabla \"" + table + "\".";

    return "OK. Tabla \"" + table + "\" eliminada.";
}

} // namespace

std::string ExecuteModifyQuery(Catalog& catalog, const std::string& query) {
    std::string low = ToLower(query);
    size_t pos = low.find_first_not_of(" \t\r\n");
    if (pos == std::string::npos) return "Consulta vacia.";
    std::string cmd = low.substr(pos);

    if (cmd.rfind("insert", 0) == 0) return ExecuteInsert(catalog, query);
    if (cmd.rfind("delete", 0) == 0) return ExecuteDelete(catalog, query);
    if (cmd.rfind("update", 0) == 0) return ExecuteUpdate(catalog, query);
    if (cmd.rfind("create table", 0) == 0) return ExecuteCreateTable(catalog, query);
    if (cmd.rfind("drop table", 0) == 0) return ExecuteDropTable(catalog, query);

    return "Comando no soportado.";
}

// ---- EJECUCION DE CONSULTAS SELECT Y MODIFICACIONES CON MODELO VOLCANO ----
void ExecuteAndPrintQuery(Catalog& catalog, const std::string& query) {
    std::string q = query;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    size_t pos = q.find_first_not_of(" \t\r\n");
    if (pos == std::string::npos) return;
    std::string low = ToLower(q.substr(pos));

    // Command BENCHMARK
    if (low.rfind("benchmark", 0) == 0) {
        benchmark::RunIndexBenchmark(catalog, 10000);
        return;
    }

    // Command CREATE INDEX ON table (col)
    if (low.rfind("create index", 0) == 0) {
        std::vector<std::string> toks = Tokenize(q);
        // CREATE INDEX [name] ON table (col)
        std::string table, col;
        for (size_t i = 0; i < toks.size(); ++i) {
            if (ToLower(toks[i]) == "on" && i + 1 < toks.size()) {
                table = toks[i + 1];
            }
        }
        size_t afterP = 0;
        col = ExtractParenContent(q, 0, afterP);
        if (col.empty() && toks.size() >= 4) {
            col = toks.back();
            col.erase(std::remove(col.begin(), col.end(), '('), col.end());
            col.erase(std::remove(col.begin(), col.end(), ')'), col.end());
        }

        if (!table.empty() && !col.empty() && catalog.Exists(table)) {
            Timer tTimer;
            tTimer.Start();
            if (IndexManager::BuildIndex(catalog, table, col)) {
                std::cout << "OK. Indice B+ Tree creado sobre tabla \"" << table << "\", columna \"" << col
                          << "\" en " << std::fixed << std::setprecision(2) << tTimer.ElapsedMs() << " ms.\n";
            } else {
                std::cout << "Error creando el indice B+ Tree.\n";
            }
        } else {
            std::cout << "Sintaxis: CREATE INDEX ON tabla (columna)\n";
        }
        return;
    }

    // Modificaciones (INSERT/UPDATE/DELETE/CREATE TABLE/DROP TABLE)
    if (low.rfind("insert", 0) == 0 || low.rfind("delete", 0) == 0 || low.rfind("update", 0) == 0 ||
        low.rfind("create table", 0) == 0 || low.rfind("drop table", 0) == 0) {
        Timer tTimer;
        tTimer.Start();
        std::string res = ExecuteModifyQuery(catalog, q);
        std::cout << res << " [" << std::fixed << std::setprecision(2) << tTimer.ElapsedMs() << " ms]\n";
        return;
    }

    if (low.rfind("select", 0) != 0) {
        std::cout << "Consulta no reconocida. Use SELECT, INSERT, UPDATE, DELETE, CREATE INDEX o BENCHMARK.\n";
        return;
    }

    // ---- PARSER VOLCANO COMPLETO PARA SELECT ----
    std::vector<std::string> toks = Tokenize(q);

    // Extraer clausulas principal: SELECT ... FROM ... [JOIN ...] [WHERE ...] [GROUP BY ...] [ORDER BY ...] [LIMIT ...] [OFFSET ...]
    std::string mainTable;
    std::string joinTable, joinCol1, joinCol2;
    std::string whereCol, whereVal;
    CmpOperator whereOp = CmpOperator::EQ;
    bool hasWhere = false;
    bool whereUsedIndex = false;

    std::vector<std::string> selectCols;
    std::vector<std::string> groupByCols;
    std::vector<AggregateExpr> aggExprs;
    std::vector<SortOperator::SortKey> sortKeys;
    int limitVal = -1;
    int offsetVal = 0;

    size_t iFrom = toks.size(), iJoin = toks.size(), iWhere = toks.size(), iGroup = toks.size(), iOrder = toks.size(), iLimit = toks.size(), iOffset = toks.size();

    for (size_t i = 0; i < toks.size(); ++i) {
        std::string tLow = ToLower(toks[i]);
        if (tLow == "from" && iFrom == toks.size()) iFrom = i;
        else if ((tLow == "join" || tLow == "inner") && iJoin == toks.size()) iJoin = i;
        else if (tLow == "where" && iWhere == toks.size()) iWhere = i;
        else if (tLow == "group" && i + 1 < toks.size() && ToLower(toks[i+1]) == "by" && iGroup == toks.size()) iGroup = i;
        else if (tLow == "order" && i + 1 < toks.size() && ToLower(toks[i+1]) == "by" && iOrder == toks.size()) iOrder = i;
        else if (tLow == "limit" && iLimit == toks.size()) iLimit = i;
        else if (tLow == "offset" && iOffset == toks.size()) iOffset = i;
    }

    if (iFrom >= toks.size() || iFrom + 1 >= toks.size()) {
        std::cout << "Error de sintaxis: falta la clausula FROM.\n";
        return;
    }

    mainTable = toks[iFrom + 1];
    const StoredTable* tMain = catalog.Get(mainTable);
    if (!tMain) {
        std::cout << "Error: La tabla \"" << mainTable << "\" no existe.\n";
        return;
    }

    // 1. Columnas SELECT y funciones de agregacion
    size_t selectEnd = iFrom;
    std::string selectRaw;
    for (size_t i = 1; i < selectEnd; ++i) selectRaw += toks[i] + " ";

    std::vector<std::string> rawSelectItems = SplitCommas(selectRaw);
    for (const auto& item : rawSelectItems) {
        std::string itemLow = ToLower(item);
        if (itemLow.find("count(") == 0 || itemLow.find("sum(") == 0 || itemLow.find("avg(") == 0 ||
            itemLow.find("min(") == 0 || itemLow.find("max(") == 0) {
            AggregateExpr agg;
            size_t pOpen = item.find('(');
            size_t pClose = item.find(')');
            std::string fn = ToLower(item.substr(0, pOpen));
            std::string arg = item.substr(pOpen + 1, pClose - pOpen - 1);

            if (fn == "count") agg.type = AggType::COUNT;
            else if (fn == "sum") agg.type = AggType::SUM;
            else if (fn == "avg") agg.type = AggType::AVG;
            else if (fn == "min") agg.type = AggType::MIN;
            else if (fn == "max") agg.type = AggType::MAX;

            agg.colName = arg;
            agg.alias = item;
            aggExprs.push_back(agg);
        } else {
            selectCols.push_back(item);
        }
    }

    // 2. Parsers adicionales (JOIN, WHERE, GROUP BY, ORDER BY, LIMIT, OFFSET)
    if (iJoin < toks.size()) {
        size_t idx = (ToLower(toks[iJoin]) == "inner") ? iJoin + 2 : iJoin + 1;
        if (idx < toks.size()) joinTable = toks[idx];
        for (size_t k = iJoin; k < std::min(iWhere, iGroup); ++k) {
            if (ToLower(toks[k]) == "on" && k + 3 < toks.size()) {
                joinCol1 = toks[k+1];
                joinCol2 = toks[k+3];
            }
        }
    }

    if (iWhere < toks.size() && iWhere + 3 < toks.size()) {
        hasWhere = true;
        whereCol = toks[iWhere + 1];
        ParseOp(toks[iWhere + 2], whereOp);
        whereVal = StripQuotes(toks[iWhere + 3]);
    }

    if (iGroup < toks.size()) {
        size_t start = iGroup + 2;
        size_t end = std::min({iOrder, iLimit, iOffset, toks.size()});
        for (size_t k = start; k < end; ++k) {
            if (toks[k] != ",") groupByCols.push_back(toks[k]);
        }
    }

    if (iOrder < toks.size()) {
        size_t start = iOrder + 2;
        size_t end = std::min({iLimit, iOffset, toks.size()});
        for (size_t k = start; k < end; ++k) {
            std::string cName = toks[k];
            bool isAsc = true;
            if (k + 1 < end && (ToLower(toks[k+1]) == "asc" || ToLower(toks[k+1]) == "desc")) {
                if (ToLower(toks[k+1]) == "desc") isAsc = false;
                k++;
            }
            sortKeys.push_back({cName, isAsc});
        }
    }

    if (iLimit < toks.size() && iLimit + 1 < toks.size()) {
        limitVal = std::atoi(toks[iLimit + 1].c_str());
    }

    if (iOffset < toks.size() && iOffset + 1 < toks.size()) {
        offsetVal = std::atoi(toks[iOffset + 1].c_str());
    }

    // ---- CONSTRUCCION DEL ARBOL DE OPERADORES VOLCANO ----
    Timer timer;
    timer.Start();

    std::unique_ptr<Operator> plan;

    // A. Seleccion de operador base (B+ Tree Index Scan o Full Scan)
    if (hasWhere && whereOp == CmpOperator::EQ && IndexManager::HasIndex(mainTable, whereCol)) {
        plan = std::make_unique<IndexScanOperator>(*tMain, whereCol, whereVal);
        whereUsedIndex = true;
    } else {
        plan = std::make_unique<ScanOperator>(*tMain);
    }

    // B. JOIN (si corresponde)
    if (!joinTable.empty() && catalog.Exists(joinTable)) {
        const StoredTable* tJoin = catalog.Get(joinTable);
        auto rightScan = std::make_unique<ScanOperator>(*tJoin);
        plan = std::make_unique<NestedLoopJoinOperator>(std::move(plan), std::move(rightScan), joinCol1, joinCol2);
    }

    // C. WHERE (si no fue resuelto completamente por el Indice B+ Tree)
    if (hasWhere && !whereUsedIndex) {
        plan = std::make_unique<SelectOperator>(std::move(plan), whereCol, whereOp, whereVal);
    }

    // D. GROUP BY y Agregaciones
    if (!groupByCols.empty() || !aggExprs.empty()) {
        plan = std::make_unique<HashAggregateOperator>(std::move(plan), groupByCols, aggExprs);
    }

    // E. Proyeccion (si no es SELECT *)
    if (selectCols.size() == 1 && selectCols[0] == "*") {
        // mantener todas las columnas
    } else if (!selectCols.empty() && aggExprs.empty()) {
        plan = std::make_unique<ProjectOperator>(std::move(plan), selectCols);
    }

    // F. ORDER BY
    if (!sortKeys.empty()) {
        plan = std::make_unique<SortOperator>(std::move(plan), sortKeys);
    }

    // G. LIMIT y OFFSET
    if (limitVal >= 0 || offsetVal > 0) {
        plan = std::make_unique<LimitOffsetOperator>(std::move(plan), limitVal, offsetVal);
    }

    // ---- EJECUCION VIA VOLCANO ITERATOR MODEL ----
    plan->Open();
    Tuple tuple;
    std::vector<std::vector<std::string>> results;
    while (plan->Next(tuple)) {
        results.push_back(tuple.values);
    }
    plan->Close();

    double elapsedMs = timer.ElapsedMs();

    PrintTable(plan->GetOutputColumns(), results);

    std::cout << "[" << results.size() << " fila(s) devuelta(s) en "
              << std::fixed << std::setprecision(2) << elapsedMs << " ms";
    if (whereUsedIndex) std::cout << " (USANDO INDICE B+ TREE)";
    std::cout << "]\n";
}

void RunSqlCli(Catalog& catalog) {
    std::cout << "Motor SQL CLI con Auto-Complete (escriba QUIT para regresar al menu principal).\n";
    std::cout << "  - Soporta consultas multilinea (finalice con ';' o presione Enter al terminar).\n";
    std::cout << "  - Tecla TAB / Flechas para sugerencias interactivas tipo IDE.\n";

    std::string buffer;

    auto HasUnclosedParensOrQuotes = [](const std::string& s) {
        int parens = 0;
        bool inQuotes = false;
        for (char c : s) {
            if (c == '"') inQuotes = !inQuotes;
            if (!inQuotes) {
                if (c == '(') parens++;
                else if (c == ')') parens--;
            }
        }
        return parens > 0 || inQuotes;
    };

    while (true) {
        std::string prompt = buffer.empty() ? "SQL> " : "  -> ";
        std::string line = cli::ReadLineWithAutoComplete(prompt, &catalog);

        size_t a = line.find_first_not_of(" \t\r\n");
        if (buffer.empty() && a != std::string::npos) {
            std::string lowLine = ToLower(line.substr(a));
            if (lowLine == "quit") break;
            if (lowLine == "clear") {
                std::cout << "\x1b[2J\x1b[H";
                continue;
            }
        }

        std::string trimmedLine = (a != std::string::npos) ? line.substr(a) : "";

        if (buffer.empty() && ToLower(trimmedLine) == "quit") break;

        if (!trimmedLine.empty()) {
            if (!buffer.empty()) buffer += " ";
            buffer += trimmedLine;
        }

        if (buffer.empty()) continue;

        std::string lowBuf = ToLower(buffer);
        if (lowBuf == "quit") break;

        bool endsWithSemicolon = (buffer.back() == ';');
        bool isSingleLineCmd = (lowBuf.rfind("benchmark", 0) == 0 || lowBuf == "list" || lowBuf == "show tables");
        bool unclosed = HasUnclosedParensOrQuotes(buffer);

        // Si termina en ';', es comando de una linea, o no hay parentesis abiertos y la linea fue vacia o termino la consulta:
        if (endsWithSemicolon || isSingleLineCmd || (!unclosed && (trimmedLine.empty() || endsWithSemicolon || line.find(';') != std::string::npos))) {
            ExecuteAndPrintQuery(catalog, buffer);
            buffer.clear();
        } else {
            // Continuar en la siguiente linea
        }
    }
}
