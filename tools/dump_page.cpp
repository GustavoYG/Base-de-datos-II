#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include "common/types.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: dump_page <file> <pageId>\n";
        return 1;
    }
    const char* path = argv[1];
    int pageId = std::stoi(argv[2]);

    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "Cannot open file: " << path << "\n"; return 2; }

    f.seekg((std::streamoff)pageId * PAGE_SIZE, std::ios::beg);
    std::vector<unsigned char> buf(PAGE_SIZE);
    f.read((char*)buf.data(), PAGE_SIZE);
    if (!f) { std::cerr << "Failed to read page " << pageId << "\n"; return 3; }

    PageHeader* ph = (PageHeader*)buf.data();
    std::cout << "Page header: pageId=" << ph->pageId << " checksum=" << ph->checksum << " freeBytes=" << ph->freeBytes << "\n";

    // Interpret as RecordPage
    RecordPage* rp = (RecordPage*)buf.data();
    std::cout << "slotCount=" << rp->slotCount << " freeSpaceOffset=" << rp->freeSpaceOffset << "\n";

    int slotEntrySize = (int)sizeof(SlotEntry);
    int dataSize = PAGE_SIZE - sizeof(PageHeader) - sizeof(int16_t) * 2; // size of rp->data
    std::cout << "dataSize=" << dataSize << " bytes; slotEntrySize=" << slotEntrySize << "\n";

    for (int s = 0; s < rp->slotCount; ++s) {
        int slotPos = dataSize - (s + 1) * slotEntrySize;
        SlotEntry se;
        std::memcpy(&se, ((unsigned char*)rp) + sizeof(PageHeader) + slotPos, slotEntrySize);
        std::cout << "slot " << s << ": offset=" << se.offset << " length=" << se.length << "\n";
    }

    if (rp->slotCount > 0) {
        int slotPos = dataSize - (1) * slotEntrySize;
        SlotEntry se;
        std::memcpy(&se, ((unsigned char*)rp) + sizeof(PageHeader) + slotPos, slotEntrySize);
        if (se.offset + se.length <= dataSize) {
            PassengerRecord rec;
            std::memcpy(&rec, ((unsigned char*)rp) + sizeof(PageHeader) + se.offset, sizeof(PassengerRecord));
            std::cout << "First record: passengerId=" << rec.passengerId << " name='" << rec.name << "' survived=" << rec.survived << "\n";
        } else {
            std::cout << "First slot points outside data area.\n";
        }
    }

    return 0;
}
