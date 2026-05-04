#include "query/query_engine.h"

QueryEngine::QueryEngine(RecordManager& r, SimpleIndex& i) : rm(r), idx(i) {}

bool QueryEngine::FindByPassengerId(int passengerId, PassengerRecord& out) {
    IndexEntry e;
    if (!idx.Find(passengerId, e)) return false;
    return rm.ReadRecord(e.pageId, e.slot, out);
}

std::vector<PassengerRecord> QueryEngine::RangeByAge(float minAge, float maxAge,
                                                     const std::vector<IndexEntry>& allEntries) {
    std::vector<PassengerRecord> out;
    for (size_t i = 0; i < allEntries.size(); ++i) {
        PassengerRecord r;
        if (rm.ReadRecord(allEntries[i].pageId, allEntries[i].slot, r)) {
            if (r.age >= minAge && r.age <= maxAge) {
                out.push_back(r);
            }
        }
    }
    return out;
}

std::vector<PassengerRecord> QueryEngine::ListSurvivors(const std::vector<IndexEntry>& allEntries) {
    std::vector<PassengerRecord> out;
    for (size_t i = 0; i < allEntries.size(); ++i) {
        PassengerRecord r;
        if (rm.ReadRecord(allEntries[i].pageId, allEntries[i].slot, r)) {
            if (r.survived == 1) out.push_back(r);
        }
    }
    return out;
}
