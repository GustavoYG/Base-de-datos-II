#include "sql/sql_completer.h"

#include <algorithm>
#include <cctype>
#include <set>

#include "db/catalog.h"
#include "common/utils.h"

const std::vector<std::string> SqlCompleter::kKeywords = {
    "SELECT", "FROM", "WHERE", "INSERT INTO", "VALUES", "UPDATE", "SET",
    "DELETE FROM", "CREATE TABLE", "DROP TABLE", "LIST", "QUIT", "SHOW TABLES",
    "AND", "OR", "LIMIT"
};

const std::vector<std::string> SqlCompleter::kDataTypes = {
    "INT32", "INT64", "FLOAT", "DOUBLE", "BOOL", "VARCHAR", "STRING"
};

namespace {

// Convierte a minusculas para comparacion case-insensitive
static std::string Lower(const std::string& str) {
    std::string s = str;
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Verifica si 'str' empieza con 'prefix' (case-insensitive)
static bool StartsWithNoCase(const std::string& str, const std::string& prefix) {
    if (prefix.empty()) return true;
    if (str.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower((unsigned char)str[i]) != std::tolower((unsigned char)prefix[i]))
            return false;
    }
    return true;
}

// Estructura para tokenizar manteniendo posiciones
struct TokenPos {
    std::string text;
    size_t start;
    size_t end;
};

static std::vector<TokenPos> TokenizeWithPositions(const std::string& str, size_t maxPos) {
    std::vector<TokenPos> tokens;
    std::string cur;
    size_t start = 0;
    bool inToken = false;

    size_t limit = std::min(str.size(), maxPos);
    for (size_t i = 0; i < limit; ++i) {
        char c = str[i];
        if (std::isalnum((unsigned char)c) || c == '_' || c == '*' || c == '.') {
            if (!inToken) {
                inToken = true;
                start = i;
                cur.clear();
            }
            cur += c;
        } else {
            if (inToken) {
                tokens.push_back({cur, start, i});
                inToken = false;
                cur.clear();
            }
        }
    }
    if (inToken) {
        tokens.push_back({cur, start, limit});
    }
    return tokens;
}

// Intenta extraer el nombre de la tabla mencionada en la consulta previa
static std::string ExtractReferencedTable(const std::vector<TokenPos>& tokens, const Catalog* catalog) {
    if (!catalog) return "";
    for (size_t i = 0; i < tokens.size(); ++i) {
        std::string tLower = Lower(tokens[i].text);
        if (tLower == "from" || tLower == "into" || tLower == "update" || tLower == "table") {
            if (i + 1 < tokens.size()) {
                std::string cand = tokens[i + 1].text;
                if (catalog->Exists(cand)) return cand;
            }
        }
    }
    return "";
}

} // namespace

std::vector<CompletionItem> SqlCompleter::GetCompletions(
    const std::string& query,
    size_t cursorPos,
    const Catalog* catalog)
{
    std::vector<CompletionItem> results;

    size_t effectiveCursor = std::min(query.size(), cursorPos);

    // Determinar la palabra actual que se está escribiendo
    size_t wordStart = effectiveCursor;
    while (wordStart > 0) {
        char c = query[wordStart - 1];
        if (std::isalnum((unsigned char)c) || c == '_' || c == '*') {
            wordStart--;
        } else {
            break;
        }
    }

    std::string currentPrefix = query.substr(wordStart, effectiveCursor - wordStart);
    size_t replaceStart = wordStart;
    size_t replaceLength = effectiveCursor - wordStart;

    // Tokenizar la consulta previa hasta la palabra actual
    std::vector<TokenPos> tokens = TokenizeWithPositions(query, wordStart);

    std::string lastToken;
    std::string prevToken;
    if (!tokens.empty()) {
        lastToken = Lower(tokens.back().text);
        if (tokens.size() >= 2) {
            prevToken = Lower(tokens[tokens.size() - 2].text);
        }
    }

    std::string refTable = ExtractReferencedTable(tokens, catalog);

    bool suggestTables = false;
    bool suggestColumns = false;
    bool suggestDataTypes = false;
    bool suggestKeywords = false;
    bool suggestValues = false;

    // Analisis contextual basado en el ultimo token relevante
    if (tokens.empty()) {
        suggestKeywords = true;
    } else if (lastToken == "from" || lastToken == "into" || lastToken == "update" ||
               (lastToken == "table" && (prevToken == "create" || prevToken == "drop"))) {
        suggestTables = true;
    } else if (lastToken == "select" || lastToken == "where" || lastToken == "set" || lastToken == "and" || lastToken == "or") {
        suggestColumns = true;
        suggestKeywords = true;
    } else if (lastToken == "create") {
        CompletionItem item;
        item.text = "TABLE";
        item.display = "TABLE";
        item.detail = "[KEYWORD]";
        item.category = CompletionCategory::KEYWORD;
        item.replaceStart = replaceStart;
        item.replaceLength = replaceLength;
        if (StartsWithNoCase("TABLE", currentPrefix)) results.push_back(item);
    } else if (lastToken == "drop") {
        CompletionItem item;
        item.text = "TABLE";
        item.display = "TABLE";
        item.detail = "[KEYWORD]";
        item.category = CompletionCategory::KEYWORD;
        item.replaceStart = replaceStart;
        item.replaceLength = replaceLength;
        if (StartsWithNoCase("TABLE", currentPrefix)) results.push_back(item);
    } else if (lastToken == "insert") {
        CompletionItem item;
        item.text = "INTO";
        item.display = "INTO";
        item.detail = "[KEYWORD]";
        item.category = CompletionCategory::KEYWORD;
        item.replaceStart = replaceStart;
        item.replaceLength = replaceLength;
        if (StartsWithNoCase("INTO", currentPrefix)) results.push_back(item);
    } else if (lastToken == "delete") {
        CompletionItem item;
        item.text = "FROM";
        item.display = "FROM";
        item.detail = "[KEYWORD]";
        item.category = CompletionCategory::KEYWORD;
        item.replaceStart = replaceStart;
        item.replaceLength = replaceLength;
        if (StartsWithNoCase("FROM", currentPrefix)) results.push_back(item);
    } else {
        // En cualquier otro punto, sugerir keywords o columnas si estamos tras un identificador
        suggestKeywords = true;
        if (!refTable.empty()) suggestColumns = true;
    }

    std::set<std::string> added;

    // 1. Sugerencias de TABLAS
    if (suggestTables && catalog) {
        std::vector<std::string> tableNames = catalog->GetAllTableNames();
        for (const auto& tName : tableNames) {
            if (StartsWithNoCase(tName, currentPrefix)) {
                CompletionItem item;
                item.text = tName;
                item.display = tName;
                item.detail = "[TABLA]";
                item.category = CompletionCategory::TABLE;
                item.replaceStart = replaceStart;
                item.replaceLength = replaceLength;
                results.push_back(item);
                added.insert(Lower(tName));
            }
        }
    }

    // 2. Sugerencias de COLUMNAS
    if (suggestColumns && catalog) {
        if (!refTable.empty() && catalog->Exists(refTable)) {
            const StoredTable* t = catalog->Get(refTable);
            if (t) {
                for (const auto& col : t->schema.columns) {
                    if (StartsWithNoCase(col.name, currentPrefix) && !added.count(Lower(col.name))) {
                        CompletionItem item;
                        item.text = col.name;
                        item.display = col.name;
                        item.detail = "[COLUMNA (" + refTable + ")]";
                        item.category = CompletionCategory::COLUMN;
                        item.replaceStart = replaceStart;
                        item.replaceLength = replaceLength;
                        results.push_back(item);
                        added.insert(Lower(col.name));
                    }
                }
            }
        } else {
            // Sugerir columnas de todas las tablas registradas
            for (const auto& kv : catalog->GetAllTables()) {
                for (const auto& col : kv.second.schema.columns) {
                    if (StartsWithNoCase(col.name, currentPrefix) && !added.count(Lower(col.name))) {
                        CompletionItem item;
                        item.text = col.name;
                        item.display = col.name;
                        item.detail = "[COLUMNA (" + kv.first + ")]";
                        item.category = CompletionCategory::COLUMN;
                        item.replaceStart = replaceStart;
                        item.replaceLength = replaceLength;
                        results.push_back(item);
                        added.insert(Lower(col.name));
                    }
                }
            }
        }
    }

    // 3. Sugerencias de PALABRAS CLAVE
    if (suggestKeywords) {
        for (const auto& kw : kKeywords) {
            if (StartsWithNoCase(kw, currentPrefix) && !added.count(Lower(kw))) {
                CompletionItem item;
                item.text = kw;
                item.display = kw;
                item.detail = "[KEYWORD]";
                item.category = CompletionCategory::KEYWORD;
                item.replaceStart = replaceStart;
                item.replaceLength = replaceLength;
                results.push_back(item);
                added.insert(Lower(kw));
            }
        }
    }

    return results;
}
