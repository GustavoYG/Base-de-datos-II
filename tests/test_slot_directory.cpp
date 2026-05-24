#include <iostream>
#include <vector>
#include <cstdio>
#include <filesystem>
#include <cassert>

#include "storage/record_manager.h"
#include "common/types.h"

int main() {
    std::string path = "data/storage/tables/test_slot_directory.tbl";

    // ensure directory exists
    std::filesystem::create_directories("data/storage/tables");
    // remove existing file
    if (std::filesystem::exists(path)) std::filesystem::remove(path);

    RecordManager rm(path);

    const int N = 200;
    std::vector<std::pair<int,int>> locs;
    locs.reserve(N);

    for (int i = 0; i < N; ++i) {
        PassengerRecord r{};
        r.passengerId = i;
        r.survived = i % 2;
        r.pclass = (i % 3) + 1;
        snprintf(r.name, sizeof(r.name), "Passenger %d", i);
        snprintf(r.sex, sizeof(r.sex), "%s", (i%2)?"male":"female");
        r.age = 20.0f + (i % 50);
        r.sibSp = i % 5;
        r.parch = i % 3;
        snprintf(r.ticket, sizeof(r.ticket), "TKT-%d", i);
        r.fare = 10.0f + i;
        snprintf(r.cabin, sizeof(r.cabin), "C%d", i);
        snprintf(r.embarked, sizeof(r.embarked), "S");

        int pageId, slot;
        bool ok = rm.InsertRecord(r, pageId, slot);
        if (!ok) {
            std::cerr << "Insert failed at " << i << "\n";
            return 2;
        }
        locs.emplace_back(pageId, slot);
    }

    // reopen manager to ensure persistence
    RecordManager rm2(path);

    for (int i = 0; i < N; ++i) {
        PassengerRecord out{};
        int pageId = locs[i].first;
        int slot = locs[i].second;
        bool ok = rm2.ReadRecord(pageId, slot, out);
        if (!ok) {
            std::cerr << "Read failed for index " << i << " (page " << pageId << ", slot " << slot << ")\n";
            return 3;
        }
        if (out.passengerId != i) {
            std::cerr << "Data mismatch at " << i << ": got " << out.passengerId << "\n";
            return 4;
        }
    }

    std::cout << "All " << N << " records inserted and verified successfully.\n";
    return 0;
}
