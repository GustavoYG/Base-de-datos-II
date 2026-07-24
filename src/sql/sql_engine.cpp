#include "sql/sql_engine.h"

#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <functional>

#include "db/catalog.h"
#include "common/utils.h"
#include "io/serializer.h"
#include "storage/record_manager.h"

namespace {

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

// Separa una lista "a,b , c" por comas, sin elementos vacios.
std::vector<std::string> SplitCommas(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else if (!std::isspace((unsigned char)c)) {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::string StripQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

bool IsNumber(const std::string& s) {
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0';
}

enum class CmpOp { EQ, NE, GT, LT, GE, LE };

bool ParseOp(const std::string& raw, CmpOp& op) {
    std::string r = ToLower(raw);
    if (r == "=" || r == "==") { op = CmpOp::EQ; return true; }
    if (r == "!=" || r == "<>") { op = CmpOp::NE; return true; }
    if (r == ">") { op = CmpOp::GT; return true; }
    if (r == "<") { op = CmpOp::LT; return true; }
    if (r == ">=") { op = CmpOp::GE; return true; }
    if (r == "<=") { op = CmpOp::LE; return true; }
    return false;
}

// Evalua la condicion para un valor de celda ya tipado. Si ambos lados son
// numericos, compara numericamente; si no, compara como texto.
bool EvalCondition(const std::string& cell, CmpOp op, const std::string& value) {
    if (IsNumber(cell) && IsNumber(value)) {
        double a = std::strtod(cell.c_str(), nullptr);
        double b = std::strtod(value.c_str(), nullptr);
        switch (op) {
            case CmpOp::EQ: return a == b;
            case CmpOp::NE: return a != b;
            case CmpOp::GT: return a > b;
            case CmpOp::LT: return a < b;
            case CmpOp::GE: return a >= b;
            case CmpOp::LE: return a <= b;
        }
    }
    int c = cell.compare(value);
    switch (op) {
        case CmpOp::EQ: return c == 0;
        case CmpOp::NE: return c != 0;
        case CmpOp::GT: return c > 0;
        case CmpOp::LT: return c < 0;
        case CmpOp::GE: return c >= 0;
        case CmpOp::LE: return c <= 0;
    }
    return false;
}

void PrintTable(const std::vector<std::string>& cols,
                const std::vector<std::vector<std::string>>& data) {
    const int n = (int)cols.size();
    std::vector<int> w(n, 0);
    for (int i = 0; i < n; ++i) w[i] = (int)cols[i].size();
    for (const auto& row : data)
        for (int i = 0; i < n; ++i)
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
            std::cout << " " << r[i];
            int pad = w[i] - (int)r[i].size();
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

// Recorre TODAS las filas del heap file de la tabla (full scan en streaming).
// Llama 'fn(pageId, slot, rowBytes)' por cada registro valido. Usa ScanAll del
// RecordManager, que solo enumera slots ocupados (no prueba slots vacios).
void ScanTable(const StoredTable& t, const std::function<void(int,int,const std::vector<unsigned char>&)>& fn) {
    RecordManager rm(t.binPath, t.schema, ReplacementPolicy::LRU);
    std::vector<std::pair<int,int>> locs;
    rm.ScanAll(locs);
    for (const auto& loc : locs) {
        std::vector<unsigned char> row;
        if (!rm.ReadRecord(loc.first, loc.second, row)) continue;
        if ((int)row.size() != t.schema.rowSize) continue;
        fn(loc.first, loc.second, row);
    }
}

} // namespace

// Helper: extrae contenido entre el primer '(' y su ')' correspondiente.
// Devuelve el contenido (sin paréntesis) y la posición justo después del ')'.
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

// Helper: convierte un string a bytes según el tipo de columna (para INSERT/UPDATE).
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
        default: { // STRING/CHAR/VARCHAR
            size_t n = std::min(val.size(), (size_t)col.length - 1);
            std::memcpy(out.data(), val.c_str(), n);
            out[n] = 0;
            break;
        }
    }
    return true;
}

// ---- INSERT INTO tabla (col1, col2, ...) VALUES (val1, val2, ...) ----
static std::string ExecuteInsert(Catalog& catalog, const std::string& rawQuery) {
    std::string q = rawQuery;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    size_t pos = q.find_first_not_of(" \t\r\n");
    if (pos == std::string::npos || ToLower(q.substr(pos, 6)) != "insert")
        return "Error de sintaxis: se esperaba INSERT.";
    pos += 6;
    while (pos < q.size() && q[pos] == ' ') pos++;
    if (ToLower(q.substr(pos, 4)) != "into")
        return "Error de sintaxis: se esperaba INTO despues de INSERT.";
    pos += 4;

    // Nombre de tabla
    while (pos < q.size() && q[pos] == ' ') pos++;
    size_t tableStart = pos;
    while (pos < q.size() && q[pos] != ' ' && q[pos] != '(') pos++;
    std::string table = q.substr(tableStart, pos - tableStart);

    const StoredTable* t = catalog.Get(table);
    if (!t) return "Error: La tabla \"" + table + "\" no existe.";

    // Extraer columnas: (col1, col2, ...)
    size_t afterParen = 0;
    std::string colsRaw = ExtractParenContent(q, pos, afterParen);
    if (colsRaw.empty()) return "Error de sintaxis: falta lista de columnas (col1, col2, ...).";
    std::vector<std::string> cols = SplitCommas(colsRaw);
    if (cols.empty()) return "Error de sintaxis: lista de columnas vacia.";

    // Buscar VALUES
    pos = afterParen;
    while (pos < q.size() && q[pos] == ' ') pos++;
    if (ToLower(q.substr(pos, 6)) != "values")
        return "Error de sintaxis: se esperaba VALUES.";
    pos += 6;

    // Extraer valores: (val1, val2, ...)
    std::string valsRaw = ExtractParenContent(q, pos, afterParen);
    if (valsRaw.empty()) return "Error de sintaxis: falta lista de valores (val1, val2, ...).";

    // Parsear valores respetando comillas
    std::vector<std::string> vals;
    {
        std::string cur;
        bool inQ = false;
        for (char c : valsRaw) {
            if (c == '"') { inQ = !inQ; cur += c; continue; }
            if (!inQ && c == ',') {
                if (!cur.empty()) { vals.push_back(StripQuotes(cur)); cur.clear(); }
            } else if (c != ' ' || inQ) {
                cur += c;
            }
        }
        if (!cur.empty()) vals.push_back(StripQuotes(cur));
    }

    if (cols.size() != vals.size()) {
        return "Error: cantidad de columnas (" + std::to_string(cols.size()) +
               ") no coincide con cantidad de valores (" + std::to_string(vals.size()) + ").";
    }

    // Preparar fila binaria según esquema
    std::vector<unsigned char> row(t->schema.rowSize);
    for (size_t i = 0; i < cols.size(); ++i) {
        int idx = t->GetColumnIndex(cols[i]);
        if (idx < 0)
            return "Error: La columna \"" + cols[i] + "\" no existe en la tabla \"" + table + "\".";
        const ColumnDef& cdef = t->schema.columns[idx];
        std::vector<unsigned char> fieldBytes;
        if (!SerializeValue(cdef, vals[i], fieldBytes)) return "Error serializando valor.";
        std::memcpy(row.data() + cdef.offset, fieldBytes.data(), fieldBytes.size());
    }

    // Insertar via RecordManager
    RecordManager rm(t->binPath, t->schema, ReplacementPolicy::LRU);
    int pageId, slot;
    if (!rm.InsertRecord(row, pageId, slot)) {
        return "Error: no se pudo insertar la fila (disco lleno?).";
    }
    return "OK. 1 fila insertada en \"" + table + "\".";
}

// ---- DELETE FROM tabla WHERE columna op valor ----
static std::string ExecuteDelete(Catalog& catalog, const std::string& rawQuery) {
    std::string q = rawQuery;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    size_t pos = q.find_first_not_of(" \t\r\n");
    if (pos == std::string::npos || ToLower(q.substr(pos, 6)) != "delete")
        return "Error de sintaxis: se esperaba DELETE.";
    pos += 6;
    while (pos < q.size() && q[pos] == ' ') pos++;
    if (ToLower(q.substr(pos, 4)) != "from")
        return "Error de sintaxis: se esperaba FROM despues de DELETE.";
    pos += 4;

    // Tabla
    while (pos < q.size() && q[pos] == ' ') pos++;
    size_t tStart = pos;
    while (pos < q.size() && q[pos] != ' ') pos++;
    std::string table = q.substr(tStart, pos - tStart);

    const StoredTable* t = catalog.Get(table);
    if (!t) return "Error: La tabla \"" + table + "\" no existe.";

    // WHERE
    while (pos < q.size() && q[pos] == ' ') pos++;
    if (ToLower(q.substr(pos, 5)) != "where")
        return "Error de sintaxis: se esperaba WHERE.";
    pos += 5;

    // columna operador valor
    while (pos < q.size() && q[pos] == ' ') pos++;
    size_t cStart = pos;
    while (pos < q.size() && q[pos] != ' ') pos++;
    std::string wCol = q.substr(cStart, pos - cStart);

    while (pos < q.size() && q[pos] == ' ') pos++;
    size_t oStart = pos;
    while (pos < q.size() && q[pos] != ' ') pos++;
    std::string opStr = q.substr(oStart, pos - oStart);
    CmpOp wOp;
    if (!ParseOp(opStr, wOp))
        return "Error de sintaxis: operador no soportado (use =, >, <, >=, <=, !=).";

    while (pos < q.size() && q[pos] == ' ') pos++;
    std::string wVal = q.substr(pos);
    if (wVal.size() >= 2 && wVal.front() == '"' && wVal.back() == '"')
        wVal = wVal.substr(1, wVal.size() - 2);

    int whereIdx = t->GetColumnIndex(wCol);
    if (whereIdx < 0)
        return "Error: La columna \"" + wCol + "\" no existe en la tabla \"" + table + "\".";

    // Normalizar BOOL si corresponde
    if (t->schema.columns[whereIdx].type == ColumnType::BOOL) {
        std::string low = ToLower(wVal);
        if (low == "1" || low == "true") wVal = "true";
        else if (low == "0" || low == "false") wVal = "false";
    }

    // Full scan + delete matching
    RecordManager rm(t->binPath, t->schema, ReplacementPolicy::LRU);
    std::vector<std::pair<int,int>> locs;
    rm.ScanAll(locs);
    int deleted = 0;
    for (const auto& loc : locs) {
        std::vector<unsigned char> row;
        if (!rm.ReadRecord(loc.first, loc.second, row)) continue;
        if ((int)row.size() != t->schema.rowSize) continue;

        // Obtener valor de celda para comparar
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
        if (EvalCondition(cellVal, wOp, wVal)) {
            rm.DeleteRecord(loc.first, loc.second);
            deleted++;
        }
    }
    return "OK. " + std::to_string(deleted) + " fila(s) eliminada(s) de \"" + table + "\".";
}

// ---- CREATE TABLE nombre (col1, col2, ...) ----
std::string ExecuteCreateTable(Catalog& catalog, const std::string& rawQuery) {
    std::string q = rawQuery;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    size_t pos = q.find_first_not_of(" \t\r\n");
    if (pos == std::string::npos || ToLower(q.substr(pos, 6)) != "create")
        return "Error de sintaxis: se esperaba CREATE.";
    pos += 6;

    while (pos < q.size() && q[pos] == ' ') pos++;
    if (ToLower(q.substr(pos, 5)) != "table")
        return "Error de sintaxis: se esperaba TABLE despues de CREATE.";
    pos += 5;

    // Nombre de tabla
    while (pos < q.size() && q[pos] == ' ') pos++;
    size_t tStart = pos;
    while (pos < q.size() && q[pos] != ' ' && q[pos] != '(') pos++;
    std::string table = q.substr(tStart, pos - tStart);
    if (table.empty()) return "Error de sintaxis: falta el nombre de la tabla.";

    if (catalog.Exists(table))
        return "Error: La tabla \"" + table + "\" ya existe.";

    // Extraer columnas entre parentesis
    size_t afterParen = 0;
    std::string colsRaw = ExtractParenContent(q, pos, afterParen);
    if (colsRaw.empty()) return "Error de sintaxis: falta lista de columnas (col1, col2, ...).";
    std::vector<std::string> cols = SplitCommas(colsRaw);
    if (cols.empty()) return "Error de sintaxis: lista de columnas vacia.";

    if (!catalog.CreateTable(table, cols))
        return "Error: No se pudo crear la tabla \"" + table + "\".";

    return "OK. Tabla \"" + table + "\" creada con " + std::to_string(cols.size()) + " columnas.";
}

// ---- DROP TABLE nombre ----
std::string ExecuteDropTable(Catalog& catalog, const std::string& rawQuery) {
    std::string q = rawQuery;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    size_t pos = q.find_first_not_of(" \t\r\n");
    if (pos == std::string::npos || ToLower(q.substr(pos, 4)) != "drop")
        return "Error de sintaxis: se esperaba DROP.";
    pos += 4;

    while (pos < q.size() && q[pos] == ' ') pos++;
    if (ToLower(q.substr(pos, 5)) != "table")
        return "Error de sintaxis: se esperaba TABLE despues de DROP.";
    pos += 5;

    // Nombre de tabla
    while (pos < q.size() && q[pos] == ' ') pos++;
    size_t tStart = pos;
    while (pos < q.size() && q[pos] != ' ') pos++;
    std::string table = q.substr(tStart, pos - tStart);
    if (table.empty()) return "Error de sintaxis: falta el nombre de la tabla.";

    if (!catalog.Exists(table))
        return "Error: La tabla \"" + table + "\" no existe.";

    if (!catalog.DropTable(table))
        return "Error: No se pudo eliminar la tabla \"" + table + "\".";

    return "OK. Tabla \"" + table + "\" eliminada.";
}

// ---- UPDATE tabla SET col = val WHERE col op val ----
static std::string ExecuteUpdate(Catalog& catalog, const std::string& rawQuery) {
    std::string q = rawQuery;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());

    size_t pos = q.find_first_not_of(" \t\r\n");
    if (pos == std::string::npos || ToLower(q.substr(pos, 6)) != "update")
        return "Error de sintaxis: se esperaba UPDATE.";
    pos += 6;

    // Tabla
    while (pos < q.size() && q[pos] == ' ') pos++;
    size_t tStart = pos;
    while (pos < q.size() && q[pos] != ' ') pos++;
    std::string table = q.substr(tStart, pos - tStart);

    const StoredTable* t = catalog.Get(table);
    if (!t) return "Error: La tabla \"" + table + "\" no existe.";

    // SET
    while (pos < q.size() && q[pos] == ' ') pos++;
    if (ToLower(q.substr(pos, 3)) != "set")
        return "Error de sintaxis: se esperaba SET.";
    pos += 3;

    // columna = valor
    while (pos < q.size() && q[pos] == ' ') pos++;
    size_t cStart = pos;
    while (pos < q.size() && q[pos] != ' ') pos++;
    std::string setCol = q.substr(cStart, pos - cStart);

    while (pos < q.size() && q[pos] == ' ') pos++;
    if (pos >= q.size() || q[pos] != '=')
        return "Error de sintaxis: se esperaba = despues de la columna en SET.";
    pos++; // saltar '='

    while (pos < q.size() && q[pos] == ' ') pos++;
    // Buscar WHERE para delimitar el valor
    size_t wherePos = pos;
    bool foundWhere = false;
    {
        std::string low = q;
        for (auto& c : low) c = (char)std::tolower((unsigned char)c);
        size_t wp = low.find(" where ", pos);
        if (wp != std::string::npos) {
            wherePos = wp;
            foundWhere = true;
        }
    }
    std::string setVal = q.substr(pos, wherePos - pos);
    if (setVal.size() >= 2 && setVal.front() == '"' && setVal.back() == '"')
        setVal = setVal.substr(1, setVal.size() - 2);

    int setIdx = t->GetColumnIndex(setCol);
    if (setIdx < 0)
        return "Error: La columna \"" + setCol + "\" no existe en la tabla \"" + table + "\".";

    if (!foundWhere)
        return "Error de sintaxis: clausula WHERE incompleta.";

    // WHERE columna operador valor
    pos = wherePos + 7; // saltar " WHERE "
    while (pos < q.size() && q[pos] == ' ') pos++;
    size_t wcStart = pos;
    while (pos < q.size() && q[pos] != ' ') pos++;
    std::string wCol = q.substr(wcStart, pos - wcStart);

    while (pos < q.size() && q[pos] == ' ') pos++;
    size_t woStart = pos;
    while (pos < q.size() && q[pos] != ' ') pos++;
    std::string wOpStr = q.substr(woStart, pos - woStart);
    CmpOp wOp;
    if (!ParseOp(wOpStr, wOp))
        return "Error de sintaxis: operador no soportado en WHERE.";

    while (pos < q.size() && q[pos] == ' ') pos++;
    std::string wVal = q.substr(pos);
    if (wVal.size() >= 2 && wVal.front() == '"' && wVal.back() == '"')
        wVal = wVal.substr(1, wVal.size() - 2);

    int whereIdx = t->GetColumnIndex(wCol);
    if (whereIdx < 0)
        return "Error: La columna \"" + wCol + "\" no existe en la tabla \"" + table + "\".";

    // Normalizar BOOL si corresponde
    if (t->schema.columns[whereIdx].type == ColumnType::BOOL) {
        std::string low = ToLower(wVal);
        if (low == "1" || low == "true") wVal = "true";
        else if (low == "0" || low == "false") wVal = "false";
    }

    // Full scan + update matching
    RecordManager rm(t->binPath, t->schema, ReplacementPolicy::LRU);
    std::vector<std::pair<int,int>> locs;
    rm.ScanAll(locs);
    int updated = 0;
    for (const auto& loc : locs) {
        std::vector<unsigned char> row;
        if (!rm.ReadRecord(loc.first, loc.second, row)) continue;
        if ((int)row.size() != t->schema.rowSize) continue;

        // Evaluar WHERE
        const ColumnDef& wcdef = t->schema.columns[whereIdx];
        std::string cellVal;
        switch (wcdef.type) {
            case ColumnType::INT32:  cellVal = std::to_string(GetFieldInt32(t->schema, row, wcdef.name)); break;
            case ColumnType::INT64:  cellVal = std::to_string(GetFieldInt64(t->schema, row, wcdef.name)); break;
            case ColumnType::FLOAT:  cellVal = std::to_string(GetFieldFloat(t->schema, row, wcdef.name)); break;
            case ColumnType::DOUBLE: cellVal = std::to_string(GetFieldDouble(t->schema, row, wcdef.name)); break;
            case ColumnType::BOOL:   cellVal = GetFieldBool(t->schema, row, wcdef.name) ? "true" : "false"; break;
            default:                 cellVal = GetFieldString(t->schema, row, wcdef.name); break;
        }
        if (!EvalCondition(cellVal, wOp, wVal)) continue;

        // Actualizar campo
        const ColumnDef& scdef = t->schema.columns[setIdx];
        std::vector<unsigned char> fieldBytes;
        if (!SerializeValue(scdef, setVal, fieldBytes)) return "Error serializando valor.";
        std::memcpy(row.data() + scdef.offset, fieldBytes.data(), fieldBytes.size());

        // Escribir de vuelta
        if (rm.UpdateRecord(loc.first, loc.second, row)) updated++;
    }
    return "OK. " + std::to_string(updated) + " fila(s) actualizada(s) en \"" + table + "\".";
}

// Dispatcher para INSERT/DELETE/UPDATE
std::string ExecuteModifyQuery(Catalog& catalog, const std::string& query) {
    std::string q = query;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());
    std::vector<std::string> tokens = Tokenize(q);
    if (tokens.empty()) return "Error de sintaxis: consulta vacia.";

    std::string cmd = ToLower(tokens[0]);
    if (cmd == "insert") return ExecuteInsert(catalog, q);
    if (cmd == "delete") return ExecuteDelete(catalog, q);
    if (cmd == "update") return ExecuteUpdate(catalog, q);
    return "Error de sintaxis: comando no reconocido.";
}

void ExecuteAndPrintQuery(Catalog& catalog, const std::string& query) {
    // El ';' es el terminador de sentencia; se remueve para no contaminar los
    // tokens (p.ej. que la tabla quede como "titanic;" o el valor como "1;").
    std::string q = query;
    q.erase(std::remove(q.begin(), q.end(), ';'), q.end());
    std::vector<std::string> tokens = Tokenize(q);
    if (tokens.empty()) {
        std::cout << "Error de sintaxis: consulta vacia.\n";
        return;
    }
    std::string cmd = ToLower(tokens[0]);

    // Despachar CREATE TABLE / DROP TABLE
    if (cmd == "create" || cmd == "drop") {
        std::string result = (cmd == "create") ? ExecuteCreateTable(catalog, q) : ExecuteDropTable(catalog, q);
        std::cout << result << "\n";
        return;
    }

    // Despachar INSERT / DELETE / UPDATE
    if (cmd == "insert" || cmd == "delete" || cmd == "update") {
        std::string result = ExecuteModifyQuery(catalog, query);
        std::cout << result << "\n";
        return;
    }

    if (cmd != "select") {
        std::cout << "Error de sintaxis: comando no reconocido \"" << tokens[0]
                  << "\". Comandos soportados: SELECT, INSERT, DELETE, UPDATE, CREATE TABLE, DROP TABLE.\n";
        return;
    }

    // Localizar FROM.
    size_t fromIdx = tokens.size();
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (ToLower(tokens[i]) == "from") { fromIdx = i; break; }
    }
    if (fromIdx == tokens.size()) {
        std::cout << "Error de sintaxis: falta la clausula FROM.\n";
        return;
    }

    // Columnas seleccionadas (entre SELECT y FROM), separando por comas.
    std::vector<std::string> colToks;
    for (size_t i = 1; i < fromIdx; ++i) {
        for (const auto& part : SplitCommas(tokens[i])) colToks.push_back(part);
    }
    if (colToks.empty()) {
        std::cout << "Error de sintaxis: Falta especificar las columnas o '*' despues de SELECT.\n";
        return;
    }
    bool selectAll = (colToks.size() == 1 && colToks[0] == "*");

    // Tabla.
    if (fromIdx + 1 >= tokens.size()) {
        std::cout << "Error de sintaxis: falta el nombre de la tabla despues de FROM.\n";
        return;
    }
    std::string table = tokens[fromIdx + 1];

    // WHERE (opcional).
    bool hasWhere = false;
    std::string wCol, wVal;
    CmpOp wOp = CmpOp::EQ;
    size_t wIdx = fromIdx + 2;
    if (wIdx < tokens.size()) {
        if (ToLower(tokens[wIdx]) != "where") {
            std::cout << "Error de sintaxis: se esperaba WHERE despues de la tabla.\n";
            return;
        }
        std::vector<std::string> cond(tokens.begin() + wIdx + 1, tokens.end());
        if (cond.size() < 3) {
            std::cout << "Error de sintaxis: condicion WHERE incompleta (esperado: columna operador valor).\n";
            return;
        }
        wCol = cond[0];
        if (!ParseOp(cond[1], wOp)) {
            std::cout << "Error de sintaxis: operador no soportado en WHERE (use =, >, <, >=, <=, !=).\n";
            return;
        }
        std::string val;
        for (size_t i = 2; i < cond.size(); ++i) { if (i > 2) val += " "; val += cond[i]; }
        wVal = StripQuotes(val);
        hasWhere = true;
    }

    // ¿Existe la tabla?
    const StoredTable* t = catalog.Get(table);
    if (!t) {
        std::cout << "Error: La tabla \"" << table << "\" no existe. Asegurese de haberla cargado primero en la Opcion 1.\n";
        return;
    }

    // Resolver columnas seleccionadas.
    std::vector<int> selIdx;
    if (selectAll) {
        for (int i = 0; i < (int)t->schema.columns.size(); ++i) selIdx.push_back(i);
    } else {
        for (const auto& c : colToks) {
            int idx = t->GetColumnIndex(c);
            if (idx < 0) {
                std::cout << "Error: La columna \"" << c << "\" no existe en la tabla \"" << table << "\".\n";
                return;
            }
            selIdx.push_back(idx);
        }
    }

    // Resolver columna del WHERE.
    int whereIdx = -1;
    if (hasWhere) {
        whereIdx = t->GetColumnIndex(wCol);
        if (whereIdx < 0) {
            std::cout << "Error: La columna \"" << wCol << "\" no existe en la tabla \"" << table << "\".\n";
            return;
        }
        // Si la columna es BOOL, normaliza el valor de comparacion a
        // "true"/"false" (el usuario puede escribir 1/0 o true/false).
        if (t->schema.columns[whereIdx].type == ColumnType::BOOL) {
            std::string low = ToLower(wVal);
            if (low == "1" || low == "true") wVal = "true";
            else if (low == "0" || low == "false") wVal = "false";
        }
    }

    // Proyectar una fila de bytes a strings segun el esquema.
    auto Project = [&](const std::vector<unsigned char>& row) {
        std::vector<std::string> proj;
        proj.reserve(selIdx.size());
        for (int i : selIdx) {
            const ColumnDef& c = t->schema.columns[i];
            switch (c.type) {
                case ColumnType::INT32:  proj.push_back(std::to_string(GetFieldInt32(t->schema, row, c.name))); break;
                case ColumnType::INT64:  proj.push_back(std::to_string(GetFieldInt64(t->schema, row, c.name))); break;
                case ColumnType::FLOAT:  proj.push_back(std::to_string(GetFieldFloat(t->schema, row, c.name))); break;
                case ColumnType::DOUBLE: proj.push_back(std::to_string(GetFieldDouble(t->schema, row, c.name))); break;
                case ColumnType::BOOL:   proj.push_back(GetFieldBool(t->schema, row, c.name) ? "true" : "false"); break;
                default:                 proj.push_back(GetFieldString(t->schema, row, c.name)); break;
            }
        }
        return proj;
    };

    // Valor de una celda como texto, respetando el tipo real de la columna
    // (para que el WHERE compare numericamente o como texto correctamente).
    auto CellToString = [&](const std::vector<unsigned char>& row, int colIdx) -> std::string {
        const ColumnDef& c = t->schema.columns[colIdx];
        switch (c.type) {
            case ColumnType::INT32:  return std::to_string(GetFieldInt32(t->schema, row, c.name));
            case ColumnType::INT64:  return std::to_string(GetFieldInt64(t->schema, row, c.name));
            case ColumnType::FLOAT:  return std::to_string(GetFieldFloat(t->schema, row, c.name));
            case ColumnType::DOUBLE: return std::to_string(GetFieldDouble(t->schema, row, c.name));
            case ColumnType::BOOL:   return GetFieldBool(t->schema, row, c.name) ? "true" : "false";
            default:                 return GetFieldString(t->schema, row, c.name);
        }
    };

    // Evaluar sobre las filas del heap file (full scan en streaming).
    std::vector<std::vector<std::string>> result;
    long long totalMatching = 0;
    ScanTable(*t, [&](int, int, const std::vector<unsigned char>& row) {
        if (hasWhere) {
            std::string cell = CellToString(row, whereIdx);
            if (!EvalCondition(cell, wOp, wVal)) return;
        }
        ++totalMatching;
        result.push_back(Project(row));
    });

    std::vector<std::string> headers;
    for (int i : selIdx) headers.push_back(t->schema.columns[i].name);

    // Vista previa (el spec muestra un numero limitado de filas; ajustable).
    const int PREVIEW_LIMIT = 2;
    size_t shown = (size_t)PREVIEW_LIMIT < result.size() ? (size_t)PREVIEW_LIMIT : result.size();
    std::vector<std::vector<std::string>> preview(result.begin(), result.begin() + shown);
    PrintTable(headers, preview);

    if (hasWhere)
        std::cout << "[Mostrando " << shown << " de " << totalMatching << " filas que cumplen la condicion]\n";
    else
        std::cout << "[Mostrando " << shown << " de " << totalMatching << " filas]\n";
}

void RunSqlCli(Catalog& catalog) {
    std::cout << "Motor SQL CLI (escriba QUIT para regresar al menu principal).\n";
    std::string line;
    while (true) {
        std::cout << "SQL> ";
        if (!std::getline(std::cin, line)) break; // EOF
        size_t a = line.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) continue;     // linea vacia
        size_t b = line.find_last_not_of(" \t\r\n");
        line = line.substr(a, b - a + 1);
        if (ToLower(line) == "quit") break;
        ExecuteAndPrintQuery(catalog, line);
    }
}
