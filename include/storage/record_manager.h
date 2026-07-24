#pragma once

#include <string>
#include <vector>

#include "common/types.h"
#include "common/schema.h"
#include "storage/page_manager.h"
#include "storage/buffer_pool.h"
#include "storage/wal_log.h"

// Gestion de registros generica sobre un heap file. Ya NO conoce PassengerRecord:
// una fila es un buffer de bytes de ancho fijo (schema.rowSize) y todo el CRUD
// opera sobre esa representacion. El slot directory, la freelist y el WAL se
// mantienen iguales porque asumen un tamano de registro fijo.
class RecordManager {
private:
    PageManager pm;
    BufferPool* bp;
    Schema schema;
    std::string walPath;
    int currentPageId;

public:
    explicit RecordManager(const std::string& path, const Schema& schema, ReplacementPolicy policy = ReplacementPolicy::LRU);
    ~RecordManager();

    bool InsertRecord(const std::vector<unsigned char>& row, int& outPageId, int& outSlot);
    bool ReadRecord(int pageId, int slot, std::vector<unsigned char>& outRow);
    bool UpdateRecord(int pageId, int slot, const std::vector<unsigned char>& row);
    bool DeleteRecord(int pageId, int slot);

    int GetNumPages() const;

    // Enumerar todos los registros validos (pageId, slot) para un full table
    // scan. Solo devuelve slots ocupados (length > 0) dentro del rango real del
    // slot directory de cada pagina.
    void ScanAll(std::vector<std::pair<int,int>>& out) const;
};
