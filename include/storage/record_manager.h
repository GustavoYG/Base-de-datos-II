#pragma once

#include <string>

#include "common/types.h"
#include "storage/page_manager.h"

class RecordManager {
private:
    PageManager pm;
    int currentPageId;

public:
    explicit RecordManager(const std::string& path);
    bool InsertRecord(const PassengerRecord& r, int& outPageId, int& outSlot);
    bool ReadRecord(int pageId, int slot, PassengerRecord& out);
};
