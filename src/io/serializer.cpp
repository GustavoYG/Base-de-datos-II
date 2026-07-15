#include "io/serializer.h"

#include <cstring>
#include <cstdlib>

#include "common/utils.h"

std::vector<unsigned char> RowToBytes(const Schema& schema, const std::vector<std::string>& row) {
    std::vector<unsigned char> out(schema.rowSize, 0);
    for (size_t i = 0; i < schema.columns.size(); ++i) {
        const ColumnDef& c = schema.columns[i];
        const std::string val = (i < row.size()) ? row[i] : std::string();
        unsigned char* p = out.data() + c.offset;
        if (c.type == ColumnType::INT32 || c.type == ColumnType::BOOL) {
            int32_t v = std::atoi(val.c_str());
            std::memcpy(p, &v, sizeof(int32_t));
        } else if (c.type == ColumnType::FLOAT) {
            float v = (float)std::atof(val.c_str());
            std::memcpy(p, &v, sizeof(float));
        } else if (c.type == ColumnType::STRING) {
            SafeCopy((char*)p, c.length, val);
        }
    }
    return out;
}

int32_t GetFieldInt32(const Schema& schema, const std::vector<unsigned char>& row, const std::string& col) {
    int idx = schema.GetColumnIndex(col);
    if (idx < 0) return 0;
    int32_t v = 0;
    std::memcpy(&v, row.data() + schema.columns[idx].offset, sizeof(int32_t));
    return v;
}

float GetFieldFloat(const Schema& schema, const std::vector<unsigned char>& row, const std::string& col) {
    int idx = schema.GetColumnIndex(col);
    if (idx < 0) return 0.0f;
    float v = 0.0f;
    std::memcpy(&v, row.data() + schema.columns[idx].offset, sizeof(float));
    return v;
}

std::string GetFieldString(const Schema& schema, const std::vector<unsigned char>& row, const std::string& col) {
    int idx = schema.GetColumnIndex(col);
    if (idx < 0) return std::string();
    const ColumnDef& c = schema.columns[idx];
    const unsigned char* p = row.data() + c.offset;
    int end = c.length;
    while (end > 0 && p[end - 1] == '\0') --end; // recorta el relleno
    return std::string((const char*)p, end);
}
