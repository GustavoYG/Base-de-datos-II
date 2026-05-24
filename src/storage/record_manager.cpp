#include "storage/record_manager.h"

#include <cstring>

#include "common/utils.h"

RecordManager::RecordManager(const std::string& path) : pm(path), currentPageId(-1) {}

bool RecordManager::InsertRecord(const PassengerRecord& r, int& outPageId, int& outSlot) {
    const int recordSize = (int)sizeof(PassengerRecord);
    const int slotEntrySize = (int)sizeof(SlotEntry);

    while (true) {
        if (currentPageId < 0) {
            currentPageId = pm.AllocatePage();
        }

        Page page;
        if (!pm.ReadPage(currentPageId, page)) return false;

        RecordPage* rp = (RecordPage*)&page;

        if (rp->slotCount < 0 || rp->slotCount > 32767) {
            rp->slotCount = 0;
        }
        if (rp->freeSpaceOffset < 0 || rp->freeSpaceOffset > (int)sizeof(rp->data)) {
            rp->freeSpaceOffset = 0;
        }

        int usedData = rp->freeSpaceOffset;
        int usedSlotDir = rp->slotCount * slotEntrySize;
        int freeSpace = (int)sizeof(rp->data) - usedData - usedSlotDir;

        int required = recordSize + slotEntrySize;
        if (freeSpace < required) {
            currentPageId = pm.AllocatePage();
            continue;
        }

        // write record at freeSpaceOffset
        int recordOffset = rp->freeSpaceOffset;
        std::memcpy(rp->data + recordOffset, &r, recordSize);
        rp->freeSpaceOffset += recordSize;

        // build slot entry and write it at the end (grow backward)
        SlotEntry se;
        se.offset = (int16_t)recordOffset;
        se.length = (int16_t)recordSize;

        int slotPos = (int)sizeof(rp->data) - (rp->slotCount + 1) * slotEntrySize;
        std::memcpy(rp->data + slotPos, &se, slotEntrySize);

        rp->slotCount++;

        rp->header.freeBytes = (int)(sizeof(rp->data) - rp->freeSpaceOffset - rp->slotCount * slotEntrySize);
        rp->header.checksum = SimpleChecksum(((unsigned char*)rp) + sizeof(PageHeader), PAGE_SIZE - sizeof(PageHeader));

        if (!pm.WritePage(currentPageId, *(Page*)rp)) return false;

        outPageId = currentPageId;
        outSlot = rp->slotCount - 1;
        return true;
    }
}

bool RecordManager::ReadRecord(int pageId, int slot, PassengerRecord& out) {
    Page page;
    if (!pm.ReadPage(pageId, page)) return false;

    RecordPage* rp = (RecordPage*)&page;
    const int slotEntrySize = (int)sizeof(SlotEntry);

    if (slot < 0 || slot >= rp->slotCount) return false;

    int slotPos = (int)sizeof(rp->data) - (slot + 1) * slotEntrySize;
    SlotEntry se;
    std::memcpy(&se, rp->data + slotPos, slotEntrySize);

    if (se.offset < 0 || se.offset + se.length > (int)sizeof(rp->data)) return false;

    std::memcpy(&out, rp->data + se.offset, sizeof(PassengerRecord));
    return true;
}
