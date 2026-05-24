#include "storage/page_manager.h"

#include <fstream>
#include <cstring>

#include "common/utils.h"

PageManager::PageManager(const std::string& filePath) : path(filePath) {}

int PageManager::GetPageCount() const {
    std::ifstream f(path.c_str(), std::ios::binary | std::ios::ate);
    if (!f) return 0;
    int size = (int)f.tellg();
    return size / PAGE_SIZE;
}

int PageManager::AllocatePage() {
    std::fstream f(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!f) {
        f.open(path.c_str(), std::ios::out | std::ios::binary);
        f.close();
        f.open(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    }

    f.seekg(0, std::ios::end);
    int size = (int)f.tellg();
    int newPageId = size / PAGE_SIZE;
    // Initialize a RecordPage so slot directory fields start at zero
    RecordPage rp;
    std::memset(&rp, 0, sizeof(RecordPage));
    rp.header.pageId = newPageId;
    rp.slotCount = 0;
    rp.freeSpaceOffset = 0;
    rp.freeSlotHead = -1;
    rp.header.freeBytes = sizeof(rp.data);
    rp.header.checksum = SimpleChecksum(((unsigned char*)&rp) + sizeof(PageHeader), PAGE_SIZE - sizeof(PageHeader));

    f.seekp(newPageId * PAGE_SIZE, std::ios::beg);
    f.write((char*)&rp, sizeof(RecordPage));
    f.close();

    return newPageId;
}

bool PageManager::WritePage(int pageId, const Page& page) {
    std::fstream f(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!f) return false;

    f.seekp(pageId * PAGE_SIZE, std::ios::beg);
    f.write((char*)&page, sizeof(Page));
    f.close();
    return true;
}

bool PageManager::ReadPage(int pageId, Page& outPage) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;

    f.seekg(pageId * PAGE_SIZE, std::ios::beg);
    f.read((char*)&outPage, sizeof(Page));
    if (!f) return false;

    int chk = SimpleChecksum(outPage.data, sizeof(outPage.data));
    return chk == outPage.header.checksum;
}
