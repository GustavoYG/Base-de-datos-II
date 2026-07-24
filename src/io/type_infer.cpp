#include "io/type_infer.h"

#include <fstream>
#include <cstdlib>
#include <cctype>

#include "io/csv_reader.h"
#include "common/utils.h"

namespace typeinfer {

bool IsInt(const std::string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    if (i >= s.size()) return false; // solo signo
    bool any = false;
    for (; i < s.size(); ++i) {
        if (!std::isdigit((unsigned char)s[i])) return false;
        any = true;
    }
    return any;
}

bool IsFloat(const std::string& s) {
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0';
}

bool IsBool(const std::string& s) {
    std::string low = ToLower(s);
    return low == "true" || low == "false" || low == "1" || low == "0";
}

// Acepta YYYY-MM-DD, YYYY/MM/DD, YYYY.MM.DD (ano de 4 digitos).
bool IsDate(const std::string& s) {
    if (s.size() < 10) return false;
    auto digit = [](char c) { return std::isdigit((unsigned char)c) != 0; };
    if (!digit(s[0]) || !digit(s[1]) || !digit(s[2]) || !digit(s[3])) return false;
    char sep = s[4];
    if (sep != '-' && sep != '/' && sep != '.') return false;
    if (!digit(s[5]) || !digit(s[6]) || s[7] != sep) return false;
    if (!digit(s[8]) || !digit(s[9])) return false;
    // No debe haber hora (sin espacio despues del dia).
    return s.size() == 10;
}

bool IsDateTime(const std::string& s) {
    if (s.size() < 19) return false;
    // Debe empezar como fecha y luego " HH:MM:SS".
    if (!IsDate(s.substr(0, 10))) return false;
    if (s[10] != ' ' && s[10] != 'T') return false;
    const std::string& t = s.substr(11);
    if (t.size() < 8) return false;
    auto digit = [](char c) { return std::isdigit((unsigned char)c) != 0; };
    if (!digit(t[0]) || !digit(t[1]) || t[2] != ':' ||
        !digit(t[3]) || !digit(t[4]) || t[5] != ':' ||
        !digit(t[6]) || !digit(t[7])) return false;
    return true;
}

ColumnType InferValue(const std::string& s) {
    if (s.empty()) return ColumnType::STRING; // lo mas seguro para valores nulos
    if (IsBool(s)) return ColumnType::BOOL;
    if (IsInt(s)) {
        // INT32 si cabe, sino INT64.
        errno = 0;
        long long v = std::strtoll(s.c_str(), nullptr, 10);
        if (v >= -2147483647LL - 1 && v <= 2147483647LL) return ColumnType::INT32;
        return ColumnType::INT64;
    }
    if (IsFloat(s)) return ColumnType::FLOAT; // FLOAT por defecto; DOUBLE si es muy largo
    if (IsDateTime(s)) return ColumnType::STRING; // se guarda como texto (DATETIME)
    if (IsDate(s)) return ColumnType::STRING;     // se guarda como texto (DATE)
    if (s.size() == 1) return ColumnType::STRING; // CHAR
    return ColumnType::STRING;                    // VARCHAR/TEXT
}

// Orden de generalidad para combinar inferencias de una misma columna.
static int Rank(ColumnType t) {
    switch (t) {
        case ColumnType::BOOL:   return 0;
        case ColumnType::INT32:  return 1;
        case ColumnType::INT64:  return 2;
        case ColumnType::FLOAT:  return 3;
        case ColumnType::DOUBLE: return 4;
        case ColumnType::STRING: return 5; // CHAR/VARCHAR/DATE/DATETIME/TEXT
        default:                 return 5;
    }
}

ColumnType Combine(ColumnType a, ColumnType b) {
    // Si uno es texto (STRING) y el otro numerico/bool, gana el texto (no se
    // puede perder informacion). Si ambos son numericos, se queda el mas amplio.
    if (a == ColumnType::STRING || b == ColumnType::STRING) return ColumnType::STRING;
    return (Rank(a) >= Rank(b)) ? a : b;
}

std::vector<ColumnType> InferColumnTypes(
    const std::vector<std::string>& header,
    const std::vector<std::vector<std::string>>& sampleRows) {
    std::vector<ColumnType> types(header.size(), ColumnType::STRING);
    if (sampleRows.empty()) return types;

    // Semilla: el PRIMER valor inferido de cada columna (no STRING, para que
    // Combine no quede siempre dominado por el valor inicial).
    const auto& first = sampleRows.front();
    for (size_t i = 0; i < header.size() && i < first.size(); ++i) {
        types[i] = InferValue(first[i]);
    }
    // Combinar con el resto de filas de muestra.
    for (size_t r = 1; r < sampleRows.size(); ++r) {
        const auto& row = sampleRows[r];
        for (size_t i = 0; i < header.size() && i < row.size(); ++i) {
            types[i] = Combine(types[i], InferValue(row[i]));
        }
    }
    return types;
}

std::vector<int> MeasureMaxWidths(
    const std::string& csvPath,
    size_t numColumns,
    bool skipHeader,
    const std::vector<ColumnType>& types) {
    std::vector<int> widths(numColumns, 0);
    std::ifstream in(csvPath.c_str());
    if (!in) return widths;

    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (first && skipHeader) { first = false; continue; }
        first = false;
        std::vector<std::string> cells = io::CsvReader_ReadAll_one(line);
        for (size_t i = 0; i < numColumns && i < cells.size(); ++i) {
            // Solo medimos columnas de texto (CHAR/VARCHAR/DATE/DATETIME/TEXT).
            if (types[i] != ColumnType::STRING) continue;
            int len = (int)cells[i].size();
            if (len > widths[i]) widths[i] = len;
        }
    }
    return widths;
}

} // namespace typeinfer
