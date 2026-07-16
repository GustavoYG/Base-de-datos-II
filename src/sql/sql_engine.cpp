#include "sql/sql_engine.h"

#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cctype>
#include <algorithm>

#include "common/utils.h"
#include "db/catalog.h"

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

// Evalua la condicion para una celda. Si ambos lados son numericos, compara
// numericamente; si no, compara como texto (comportamiento "simplified SQL").
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

    std::cout << '+';
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < w[i] + 2; ++j) std::cout << '-';
        std::cout << '+';
    }
    std::cout << "\n";

    std::cout << '|';
    for (int i = 0; i < n; ++i) {
        std::cout << " " << cols[i];
        int pad = w[i] - (int)cols[i].size();
        for (int p = 0; p < pad + 1; ++p) std::cout << " ";
        std::cout << "|";
    }
    std::cout << "\n";

    std::cout << '+';
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < w[i] + 2; ++j) std::cout << '-';
        std::cout << '+';
    }
    std::cout << "\n";

    for (const auto& r : data) {
        std::cout << '|';
        for (int i = 0; i < n; ++i) {
            std::cout << " " << r[i];
            int pad = w[i] - (int)r[i].size();
            for (int p = 0; p < pad + 1; ++p) std::cout << " ";
            std::cout << "|";
        }
        std::cout << "\n";
    }

    std::cout << '+';
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < w[i] + 2; ++j) std::cout << '-';
        std::cout << '+';
    }
    std::cout << "\n";
}

} // namespace

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
    if (ToLower(tokens[0]) != "select") {
        std::cout << "Error de sintaxis: la consulta debe comenzar con SELECT.\n";
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
    LoadedTable* t = catalog.Get(table);
    if (!t) {
        std::cout << "Error: La tabla \"" << table << "\" no existe. Asegurese de haberla cargado primero en la Opcion 1.\n";
        return;
    }

    // Resolver columnas seleccionadas.
    std::vector<int> selIdx;
    if (selectAll) {
        for (int i = 0; i < (int)t->columns.size(); ++i) selIdx.push_back(i);
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
    }

    // Evaluar sobre las filas.
    std::vector<std::vector<std::string>> result;
    int totalMatching = 0;
    for (const auto& row : t->rows) {
        if (hasWhere && !EvalCondition(row[whereIdx], wOp, wVal)) continue;
        totalMatching++;
        std::vector<std::string> proj;
        proj.reserve(selIdx.size());
        for (int i : selIdx) proj.push_back(row[i]);
        result.push_back(std::move(proj));
    }

    std::vector<std::string> headers;
    for (int i : selIdx) headers.push_back(t->columns[i]);

    // Vista previa (el spec muestra un numero limitado de filas; ajustable).
    const int PREVIEW_LIMIT = 2;
    size_t shown = (size_t)PREVIEW_LIMIT < result.size() ? (size_t)PREVIEW_LIMIT : result.size();
    std::vector<std::vector<std::string>> preview(result.begin(), result.begin() + shown);
    PrintTable(headers, preview);

    if (hasWhere)
        std::cout << "[Mostrando " << shown << " de " << totalMatching << " filas que cumplen la condicion]\n";
    else
        std::cout << "[Mostrando " << shown << " de " << t->rows.size() << " filas]\n";
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
