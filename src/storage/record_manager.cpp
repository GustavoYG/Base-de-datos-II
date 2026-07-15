#include "storage/record_manager.h"

#include <cstring>

#include "common/utils.h"
#include "storage/wal_log.h"

RecordManager::RecordManager(const std::string& path, const Schema& schema, ReplacementPolicy policy)
    : pm(path), schema(schema), bp(nullptr), currentPageId(-1) {
    walPath = path + ".wal";
    bp = new BufferPool(pm, 16, policy);

    // Recuperacion simple: re-aplica la ultima imagen de pagina valida del WAL.
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

RecordManager::~RecordManager() {
    // Vuelca las paginas sucias antes de soltar el buffer (antes se filtraba
    // y se perdia la ultima pagina modificada no evictada).
    if (bp) {
        bp->FlushAll();
        delete bp;
    }
}

bool RecordManager::InsertRecord(const std::vector<unsigned char>& row, int& outPageId, int& outSlot) {
    const int recordSize = schema.rowSize;
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

        // Reusar un slot libre de la freelist, si lo hay.
        int reuseSlot = -1;
        if (rp->freeSlotHead >= 0) {
            reuseSlot = rp->freeSlotHead;
            int slotPos = (int)sizeof(rp->data) - (reuseSlot + 1) * slotEntrySize;
            SlotEntry freeSe;
            std::memcpy(&freeSe, rp->data + slotPos, slotEntrySize);
            // El siguiente libre queda en el campo offset del slot borrado.
            rp->freeSlotHead = freeSe.offset;
        }

        // Escribir la fila (bytes crudos) en freeSpaceOffset.
        int recordOffset = rp->freeSpaceOffset;
        std::memcpy(rp->data + recordOffset, row.data(), recordSize);
        rp->freeSpaceOffset += recordSize;

        // Construir la entrada de slot y crecer el directorio hacia atras.
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

        // WAL: imagen de pagina antes de marcar dirty.
        std::string payload;
        payload.resize(sizeof(int) + PAGE_SIZE);
        std::memcpy(&payload[0], &currentPageId, sizeof(int));
        std::memcpy(&payload[0] + sizeof(int), (unsigned char*)rp, PAGE_SIZE);
        AppendWalEntry(walPath, payload);

        if (!bp->UnpinPage(currentPageId, true)) return false;

        outPageId = currentPageId;
        return true;
    }
}

bool RecordManager::ReadRecord(int pageId, int slot, std::vector<unsigned char>& outRow) {
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

    outRow.resize(se.length);
    std::memcpy(outRow.data(), rp->data + se.offset, se.length);
    bp->UnpinPage(pageId, false);
    return true;
}

bool RecordManager::UpdateRecord(int pageId, int slot, const std::vector<unsigned char>& row) {
    const int recordSize = schema.rowSize;
    Page* pagePtr = bp->PinPage(pageId);
    if (!pagePtr) return false;

    RecordPage* rp = (RecordPage*)pagePtr;
    const int slotEntrySize = (int)sizeof(SlotEntry);

    if (slot < 0 || slot >= rp->slotCount) { bp->UnpinPage(pageId, false); return false; }

    int slotPos = (int)sizeof(rp->data) - (slot + 1) * slotEntrySize;
    SlotEntry se;
    std::memcpy(&se, rp->data + slotPos, slotEntrySize);
    if (se.length == 0) { bp->UnpinPage(pageId, false); return false; }

    // Actualizacion en sitio: el ancho de la fila es fijo.
    std::memcpy(rp->data + se.offset, row.data(), recordSize);
    rp->header.checksum = SimpleChecksum(((unsigned char*)rp) + sizeof(PageHeader), PAGE_SIZE - sizeof(PageHeader));

    std::string payload;
    payload.resize(sizeof(int) + PAGE_SIZE);
    std::memcpy(&payload[0], &pageId, sizeof(int));
    std::memcpy(&payload[0] + sizeof(int), (unsigned char*)rp, PAGE_SIZE);
    AppendWalEntry(walPath, payload);

    return bp->UnpinPage(pageId, true);
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

    if (se.length == 0) { bp->UnpinPage(pageId, false); return false; } // ya borrado

    // Enlazar este slot a la freelist: guardar la cabeza actual en offset, length=0.
    SlotEntry freeEntry;
    freeEntry.offset = rp->freeSlotHead;
    freeEntry.length = 0;
    std::memcpy(rp->data + slotPos, &freeEntry, slotEntrySize);
    rp->freeSlotHead = (int16_t)slot;

    rp->header.freeBytes = (int)(sizeof(rp->data) - rp->freeSpaceOffset - rp->slotCount * slotEntrySize);
    rp->header.checksum = SimpleChecksum(((unsigned char*)rp) + sizeof(PageHeader), PAGE_SIZE - sizeof(PageHeader));

    std::string payload;
    payload.resize(sizeof(int) + PAGE_SIZE);
    std::memcpy(&payload[0], &pageId, sizeof(int));
    std::memcpy(&payload[0] + sizeof(int), (unsigned char*)rp, PAGE_SIZE);
    AppendWalEntry(walPath, payload);

    bool ok = bp->UnpinPage(pageId, true);
    return ok;
}
