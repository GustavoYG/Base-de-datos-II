#include "storage/record_manager.h"
#include <iostream>
#include <cstdio>

int main() {
    const std::string path = "data/storage/tables/test_wal.tbl";

    // remove existing files
    std::remove(path.c_str());
    std::remove((path + ".wal").c_str());

    // First manager: insert a record (this writes WAL before marking dirty)
    {
        RecordManager rm(path);
        PassengerRecord r{};
        r.passengerId = 12345;
        int pageId, slot;
        bool ok = rm.InsertRecord(r, pageId, slot);
        if (!ok) { std::cerr << "Insert failed\n"; return 1; }
        // Note: do NOT flush BufferPool / force page write to disk to simulate crash before page write
    }

    // Simulate restart: new RecordManager should read WAL and recover page
    {
        RecordManager rm2(path);
        PassengerRecord out{};
        bool ok = rm2.ReadRecord(0, 0, out);
        if (!ok) { std::cerr << "Recovery read failed\n"; return 2; }
        if (out.passengerId != 12345) { std::cerr << "Recovered value mismatch: " << out.passengerId << "\n"; return 3; }
        std::cout << "WAL recovery test passed" << std::endl;
    }

    return 0;
}
