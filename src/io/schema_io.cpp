#include "io/schema_io.h"

#include <fstream>
#include <sstream>

namespace schemaio {

static const char* TypeName(ColumnType t) {
    switch (t) {
        case ColumnType::INT32:  return "INT32";
        case ColumnType::FLOAT:  return "FLOAT";
        case ColumnType::BOOL:   return "BOOL";
        case ColumnType::INT64:  return "INT64";
        case ColumnType::DOUBLE: return "DOUBLE";
        case ColumnType::STRING: return "STRING";
        default:                 return "STRING";
    }
}

static ColumnType TypeFromName(const std::string& s) {
    if (s == "INT32")  return ColumnType::INT32;
    if (s == "FLOAT")  return ColumnType::FLOAT;
    if (s == "BOOL")   return ColumnType::BOOL;
    if (s == "INT64")  return ColumnType::INT64;
    if (s == "DOUBLE") return ColumnType::DOUBLE;
    return ColumnType::STRING;
}

std::string SchemaPath(const std::string& tablesDir, const std::string& tableName) {
    return tablesDir + "/" + tableName + ".schema";
}

bool SaveSchema(const std::string& path, const Schema& schema) {
    std::ofstream os(path.c_str());
    if (!os) return false;
    for (const auto& c : schema.columns) {
        os << c.name << " " << TypeName(c.type) << " " << c.length << "\n";
    }
    return (bool)os;
}

bool LoadSchema(const std::string& path, Schema& schema) {
    std::ifstream is(path.c_str());
    if (!is) return false;
    schema.columns.clear();
    std::string line;
    while (std::getline(is, line)) {
        std::istringstream ss(line);
        std::string name, type;
        int32_t len = 0;
        if (!(ss >> name >> type)) continue;
        ss >> len;
        schema.AddColumn(name, TypeFromName(type), len);
    }
    if (schema.columns.empty()) return false;
    schema.Finalize();
    return true;
}

} // namespace schemaio
