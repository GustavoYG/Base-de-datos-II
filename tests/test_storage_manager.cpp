#include <iostream>
#include <vector>
#include <filesystem>
#include <cstring>
#include <fstream>

#include "storage/page_manager.h"
#include "storage/record_manager.h"
#include "common/utils.h"

bool TestPageManager() {
    std::string path = "data/storage/tables/test_pm.tbl";
    std::filesystem::create_directories("data/storage/tables");
    if (std::filesystem::exists(path)) std::filesystem::remove(path);

    PageManager pm(path);
    int p0 = pm.AllocatePage();
    Page page;
    if (!pm.ReadPage(p0, page)) {
        std::cerr << "PageManager: read after allocate failed" << std::endl;
        return false;
    }
    if (page.header.pageId != p0) {
        std::cerr << "PageManager: pageId mismatch" << std::endl;
        return false;
    }

    // modify data and write
    page.data[0] = 0xAB;
    page.header.checksum = SimpleChecksum(page.data, sizeof(page.data));
    if (!pm.WritePage(p0, page)) {
        std::cerr << "PageManager: write failed" << std::endl;
        return false;
    }

    Page page2;
    if (!pm.ReadPage(p0, page2)) {
        std::cerr << "PageManager: read after write failed" << std::endl;
        return false;
    }
    if ((unsigned char)page2.data[0] != 0xAB) {
        std::cerr << "PageManager: data mismatch after write" << std::endl;
        return false;
    }

    // corrupt a byte on disk to trigger checksum mismatch
    {
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!f) { std::cerr << "Cannot open file to corrupt" << std::endl; return false; }
        // seek to first data byte of page
        std::streamoff pos = (std::streamoff)p0 * PAGE_SIZE + sizeof(PageHeader);
        f.seekp(pos, std::ios::beg);
        char v = (char)((unsigned char)page2.data[0] ^ 0xFF);
        f.write(&v, 1);
        f.close();
    }

    Page page3;
    if (pm.ReadPage(p0, page3)) {
        std::cerr << "PageManager: checksum mismatch not detected" << std::endl;
        return false;
    }

    return true;
}

bool TestRecordManager() {
    std::string path = "data/storage/tables/test_rm.tbl";
    std::filesystem::create_directories("data/storage/tables");
    if (std::filesystem::exists(path)) std::filesystem::remove(path);

    RecordManager rm(path);
    const int N = 300;
    std::vector<std::pair<int,int>> locs;
    locs.reserve(N);

    for (int i = 0; i < N; ++i) {
        PassengerRecord r{};
        r.passengerId = i;
        r.survived = i % 2;
        r.pclass = (i % 3) + 1;
        snprintf(r.name, sizeof(r.name), "P-%d", i);
        snprintf(r.sex, sizeof(r.sex), "%s", (i%2)?"male":"female");
        r.age = 20.0f + (i % 50);
        r.sibSp = i % 5;
        r.parch = i % 3;
        snprintf(r.ticket, sizeof(r.ticket), "TKT-%d", i);
        r.fare = 10.0f + i;
        snprintf(r.cabin, sizeof(r.cabin), "C%d", i);
        snprintf(r.embarked, sizeof(r.embarked), "S");

        int pageId, slot;
        if (!rm.InsertRecord(r, pageId, slot)) {
            std::cerr << "RecordManager: insert failed at " << i << std::endl;
            return false;
        }
        locs.emplace_back(pageId, slot);
    }

    // reopen and validate
    RecordManager rm2(path);
    for (int i = 0; i < N; ++i) {
        PassengerRecord out{};
        int pageId = locs[i].first;
        int slot = locs[i].second;
        if (!rm2.ReadRecord(pageId, slot, out)) {
            std::cerr << "RecordManager: read failed for " << i << " ("<<pageId<<","<<slot<<")" << std::endl;
            return false;
        }
        if (out.passengerId != i) {
            std::cerr << "RecordManager: data mismatch at " << i << " got " << out.passengerId << std::endl;
            return false;
        }
    }

    // Delete every 10th record and ensure it is no longer readable
    for (int i = 0; i < N; i += 10) {
        int pageId = locs[i].first;
        int slot = locs[i].second;
        if (!rm2.DeleteRecord(pageId, slot)) {
            std::cerr << "RecordManager: delete failed for " << i << std::endl;
            return false;
        }
    }

    for (int i = 0; i < N; ++i) {
        PassengerRecord out{};
        int pageId = locs[i].first;
        int slot = locs[i].second;
        bool ok = rm2.ReadRecord(pageId, slot, out);
        if (i % 10 == 0) {
            if (ok) {
                std::cerr << "RecordManager: deleted record still readable " << i << std::endl;
                return false;
            }
        } else {
            if (!ok) {
                std::cerr << "RecordManager: read failed after delete for " << i << std::endl;
                return false;
            }
            if (out.passengerId != i) {
                std::cerr << "RecordManager: data mismatch at " << i << " got " << out.passengerId << std::endl;
                return false;
            }
        }
    }

    return true;
}

int main() {
    bool ok1 = TestPageManager();
    std::cout << "TestPageManager: " << (ok1?"OK":"FAIL") << std::endl;
    bool ok2 = TestRecordManager();
    std::cout << "TestRecordManager: " << (ok2?"OK":"FAIL") << std::endl;

    if (ok1 && ok2) {
        std::cout << "All storage tests passed." << std::endl;
        return 0;
    }
    return 1;
}
