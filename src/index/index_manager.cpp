#include "index/index_manager.h"

#include <fstream>
#include <filesystem>
#include <iostream>

#include "index/bplus_tree.h"
#include "storage/record_manager.h"
#include "io/serializer.h"
#include "common/utils.h"

namespace fs = std::filesystem;

std::string IndexManager::GetIndexPath(const std::string& tableName, const std::string& colName) {
    return Catalog::kTablesDir + "/" + tableName + "_" + colName + ".idx";
}

bool IndexManager::HasIndex(const std::string& tableName, const std::string& colName) {
    std::string path = GetIndexPath(tableName, colName);
    std::error_code ec;
    return fs::exists(path, ec);
}

bool IndexManager::BuildIndex(Catalog& catalog, const std::string& tableName, const std::string& colName) {
    const StoredTable* t = catalog.Get(tableName);
    if (!t) return false;

    int colIdx = t->GetColumnIndex(colName);
    if (colIdx < 0) return false;

    const ColumnDef& cdef = t->schema.columns[colIdx];
    std::string indexPath = GetIndexPath(tableName, colName);

    // Si ya existe un indice previo, eliminarlo para reconstruir
    std::error_code ec;
    fs::remove(indexPath, ec);
    fs::remove(indexPath + ".meta", ec);

    BPlusTree bTree(indexPath, cdef.type);

    RecordManager rm(t->binPath, t->schema, ReplacementPolicy::LRU);
    std::vector<std::pair<int, int>> locs;
    rm.ScanAll(locs);

    for (const auto& loc : locs) {
        std::vector<unsigned char> row;
        if (!rm.ReadRecord(loc.first, loc.second, row)) continue;
        if ((int)row.size() != t->schema.rowSize) continue;

        BTreeKey key;
        if (cdef.type == ColumnType::INT32) {
            int32_t val = GetFieldInt32(t->schema, row, cdef.name);
            BTreeKeyFromInt32(key, val);
        } else if (cdef.type == ColumnType::FLOAT) {
            float val = GetFieldFloat(t->schema, row, cdef.name);
            BTreeKeyFromFloat(key, val);
        } else {
            std::string val = GetFieldString(t->schema, row, cdef.name);
            BTreeKeyFromString(key, val, cdef.length);
        }

        bTree.Insert(key, loc.first, (int16_t)loc.second);
    }

    return true;
}
