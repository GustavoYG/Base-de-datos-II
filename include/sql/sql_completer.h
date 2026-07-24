#pragma once

#include <string>
#include <vector>
#include <map>

class Catalog;

enum class CompletionCategory {
    KEYWORD,
    TABLE,
    COLUMN,
    DATATYPE,
    OPERATOR,
    VALUE
};

struct CompletionItem {
    std::string text;           // Texto exacto a insertar al completar
    std::string display;        // Texto visible en el menu emergente
    std::string detail;         // Informacion descriptiva (ej. "[COLUMNA (usuarios)]")
    CompletionCategory category;
    
    // Posicion desde la cual se debe reemplazar en el input actual
    size_t replaceStart = 0;
    size_t replaceLength = 0;
};

class SqlCompleter {
public:
    SqlCompleter() = default;

    // Genera la lista de sugerencias para la consulta en 'query' dada la posicion del cursor
    static std::vector<CompletionItem> GetCompletions(
        const std::string& query,
        size_t cursorPos,
        const Catalog* catalog
    );

private:
    // Palabras clave estándar soportadas por el proyecto
    static const std::vector<std::string> kKeywords;
    static const std::vector<std::string> kDataTypes;
};
