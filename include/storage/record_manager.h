#pragma once

#include <string>

#include "common/types.h"
#include "storage/page_manager.h"
#include "storage/buffer_pool.h"
#include "storage/wal_log.h"

class RecordManager {
private:
    PageManager pm;
    BufferPool* bp;
    std::string walPath;
    int currentPageId;

public:
    explicit RecordManager(const std::string& path);
    bool InsertRecord(const PassengerRecord& r, int& outPageId, int& outSlot);
    bool ReadRecord(int pageId, int slot, PassengerRecord& out);
    bool DeleteRecord(int pageId, int slot);
};
