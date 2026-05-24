#include "storage/record_manager.h"

#include <cstring>

#include "common/utils.h"
#include "storage/wal_log.h"

RecordManager::RecordManager(const std::string& path) : pm(path), bp(nullptr), currentPageId(-1) {
    walPath = path + ".wal";
    bp = new BufferPool(pm, 16);

    // simple recovery: read last valid wal entry and apply to page
    std::string payload = ReadLastValidWalPayload(walPath);
    if (!payload.empty()) {
        if (payload.size() >= sizeof(int) + PAGE_SIZE) {
            int pageId = 0;
            std::memcpy(&pageId, payload.data(), sizeof(int));
            Page p;
            std::memcpy(&p, payload.data() + sizeof(int), PAGE_SIZE);
            pm.WritePage(pageId, p);
        }
    }
}

bool RecordManager::InsertRecord(const PassengerRecord& r, int& outPageId, int& outSlot) {
    const int recordSize = (int)sizeof(PassengerRecord);
    const int slotEntrySize = (int)sizeof(SlotEntry);

    while (true) {
        if (currentPageId < 0) {
            currentPageId = pm.AllocatePage();
        }

        Page* pagePtr = bp->PinPage(currentPageId);
        if (!pagePtr) return false;

        RecordPage* rp = (RecordPage*)pagePtr;

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
            bp->UnpinPage(currentPageId, false);
            currentPageId = pm.AllocatePage();
            continue;
        }

        // try to reuse a free slot from freelist
        int reuseSlot = -1;
        if (rp->freeSlotHead >= 0) {
            reuseSlot = rp->freeSlotHead;
            int slotPos = (int)sizeof(rp->data) - (reuseSlot + 1) * slotEntrySize;
            SlotEntry freeSe;
            std::memcpy(&freeSe, rp->data + slotPos, slotEntrySize);
            // next free stored in offset field of the free slot
            rp->freeSlotHead = freeSe.offset;
        }

        // write record at freeSpaceOffset
        int recordOffset = rp->freeSpaceOffset;
        std::memcpy(rp->data + recordOffset, &r, recordSize);
        rp->freeSpaceOffset += recordSize;

        // build slot entry and write it at the end (grow backward)
        SlotEntry se;
        se.offset = (int16_t)recordOffset;
        se.length = (int16_t)recordSize;

        if (reuseSlot >= 0) {
            int slotPos = (int)sizeof(rp->data) - (reuseSlot + 1) * slotEntrySize;
            std::memcpy(rp->data + slotPos, &se, slotEntrySize);
            outSlot = reuseSlot;
        } else {
            int slotPos = (int)sizeof(rp->data) - (rp->slotCount + 1) * slotEntrySize;
            std::memcpy(rp->data + slotPos, &se, slotEntrySize);
            outSlot = rp->slotCount;
            rp->slotCount++;
        }

        rp->header.freeBytes = (int)(sizeof(rp->data) - rp->freeSpaceOffset - rp->slotCount * slotEntrySize);
        rp->header.checksum = SimpleChecksum(((unsigned char*)rp) + sizeof(PageHeader), PAGE_SIZE - sizeof(PageHeader));

        // WAL: write page image to WAL before marking page dirty
        std::string payload;
        payload.resize(sizeof(int) + PAGE_SIZE);
        std::memcpy(&payload[0], &currentPageId, sizeof(int));
        std::memcpy(&payload[0] + sizeof(int), (unsigned char*)rp, PAGE_SIZE);
        AppendWalEntry(walPath, payload);

        // unpin and mark dirty
        if (!bp->UnpinPage(currentPageId, true)) return false;

        outPageId = currentPageId;
        return true;
    }
}

bool RecordManager::ReadRecord(int pageId, int slot, PassengerRecord& out) {
    Page* pagePtr = bp->PinPage(pageId);
    if (!pagePtr) return false;

    RecordPage* rp = (RecordPage*)pagePtr;
    const int slotEntrySize = (int)sizeof(SlotEntry);

    if (slot < 0 || slot >= rp->slotCount) { bp->UnpinPage(pageId, false); return false; }

    int slotPos = (int)sizeof(rp->data) - (slot + 1) * slotEntrySize;
    SlotEntry se;
    std::memcpy(&se, rp->data + slotPos, slotEntrySize);
    if (se.length == 0) { bp->UnpinPage(pageId, false); return false; }

    if (se.offset < 0 || se.offset + se.length > (int)sizeof(rp->data)) { bp->UnpinPage(pageId, false); return false; }

    std::memcpy(&out, rp->data + se.offset, sizeof(PassengerRecord));
    bp->UnpinPage(pageId, false);
    return true;
}

bool RecordManager::DeleteRecord(int pageId, int slot) {
    Page* pagePtr = bp->PinPage(pageId);
    if (!pagePtr) return false;

    RecordPage* rp = (RecordPage*)pagePtr;
    const int slotEntrySize = (int)sizeof(SlotEntry);

    if (slot < 0 || slot >= rp->slotCount) { bp->UnpinPage(pageId, false); return false; }

    int slotPos = (int)sizeof(rp->data) - (slot + 1) * slotEntrySize;
    SlotEntry se;
    std::memcpy(&se, rp->data + slotPos, slotEntrySize);

    if (se.length == 0) { bp->UnpinPage(pageId, false); return false; } // already deleted

    // link this slot into freelist: store current head in offset, set length=0
    SlotEntry freeEntry;
    freeEntry.offset = rp->freeSlotHead;
    freeEntry.length = 0;
    std::memcpy(rp->data + slotPos, &freeEntry, slotEntrySize);
    rp->freeSlotHead = (int16_t)slot;

    // update freeBytes (we don't reclaim record area now)
    rp->header.freeBytes = (int)(sizeof(rp->data) - rp->freeSpaceOffset - rp->slotCount * slotEntrySize) + se.length;
    rp->header.checksum = SimpleChecksum(((unsigned char*)rp) + sizeof(PageHeader), PAGE_SIZE - sizeof(PageHeader));

    // WAL and mark dirty
    std::string payload;
    payload.resize(sizeof(int) + PAGE_SIZE);
    std::memcpy(&payload[0], &pageId, sizeof(int));
    std::memcpy(&payload[0] + sizeof(int), (unsigned char*)rp, PAGE_SIZE);
    AppendWalEntry(walPath, payload);

    bool ok = bp->UnpinPage(pageId, true);
    return ok;
}
