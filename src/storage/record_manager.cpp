#include "storage/record_manager.h"

#include <cstring>

#include "common/utils.h"

RecordManager::RecordManager(const std::string& path) : pm(path), currentPageId(-1) {}

bool RecordManager::InsertRecord(const PassengerRecord& r, int& outPageId, int& outSlot) {
    int recordSize = (int)sizeof(PassengerRecord);

    while (true) {
        if (currentPageId < 0) {
            currentPageId = pm.AllocatePage();
        }

        Page page;
        if (!pm.ReadPage(currentPageId, page)) return false;

        RecordPage* rp = (RecordPage*)&page;
        int maxRecords = (int)(sizeof(rp->data) / recordSize);
        if (rp->recordCount < 0 || rp->recordCount > maxRecords) {
            rp->recordCount = 0;
        }

        if (rp->recordCount >= maxRecords) {
            currentPageId = pm.AllocatePage();
            continue;
        }

        int offset = rp->recordCount * recordSize;
        std::memcpy(rp->data + offset, &r, recordSize);
        rp->recordCount++;

        rp->header.freeBytes = (int)(sizeof(rp->data) - rp->recordCount * recordSize);
        rp->header.checksum = SimpleChecksum(rp->data, sizeof(rp->data));

        if (!pm.WritePage(currentPageId, *(Page*)rp)) return false;

        outPageId = currentPageId;
        outSlot = rp->recordCount - 1;
        return true;
    }
}

bool RecordManager::ReadRecord(int pageId, int slot, PassengerRecord& out) {
    Page page;
    if (!pm.ReadPage(pageId, page)) return false;

    RecordPage* rp = (RecordPage*)&page;
    int recordSize = (int)sizeof(PassengerRecord);
    if (slot < 0 || slot >= rp->recordCount) return false;

    int offset = slot * recordSize;
    std::memcpy(&out, rp->data + offset, recordSize);
    return true;
}
