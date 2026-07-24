#include "common/schema.h"

void Schema::AddColumn(const std::string& name, ColumnType type, int32_t length) {
    ColumnDef c;
    c.name = name;
    c.type = type;
    c.length = length;
    c.offset = 0;
    columns.push_back(c);
}

int32_t Schema::TypeSize(ColumnType type, int32_t length) {
    switch (type) {
        case ColumnType::INT32:  return 4;
        case ColumnType::FLOAT:  return 4;
        case ColumnType::BOOL:   return 4;
        case ColumnType::INT64:  return 8;
        case ColumnType::DOUBLE: return 8;
        case ColumnType::STRING: return (length > 0) ? length : DEFAULT_VARCHAR_LEN;
        default:                 return 4;
    }
}

void Schema::Finalize() {
    int32_t off = 0;
    for (auto& c : columns) {
        c.offset = off;
        int32_t size = TypeSize(c.type, c.length);
        // Para columnas de texto, asegura un largo minimo razonable.
        if (c.type == ColumnType::STRING && c.length <= 0) {
            c.length = DEFAULT_VARCHAR_LEN;
            size = c.length;
        }
        off += size;
    }
    rowSize = off;
}

int Schema::GetColumnIndex(const std::string& name) const {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].name == name) return (int)i;
    }
    return -1;
}

bool Schema::HasColumn(const std::string& name) const {
    return GetColumnIndex(name) >= 0;
}
