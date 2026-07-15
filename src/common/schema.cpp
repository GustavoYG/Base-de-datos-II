#include "common/schema.h"

void Schema::AddColumn(const std::string& name, ColumnType type, int32_t length) {
    ColumnDef c;
    c.name = name;
    c.type = type;
    c.length = length;
    c.offset = 0;
    columns.push_back(c);
}

void Schema::Finalize() {
    int32_t off = 0;
    for (auto& c : columns) {
        c.offset = off;
        // INT32 / FLOAT / BOOL ocupan 4 bytes; STRING usa su largo fijo.
        int32_t size = (c.type == ColumnType::STRING) ? c.length : 4;
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
