#pragma once

#include <vector>

#include "common/types.h"
#include "index/simple_index.h"
#include "storage/record_manager.h"

class QueryEngine {
private:
    RecordManager& rm;
    SimpleIndex& idx;

public:
    QueryEngine(RecordManager& r, SimpleIndex& i);
    bool FindByPassengerId(int passengerId, PassengerRecord& out);
    std::vector<PassengerRecord> RangeByAge(float minAge, float maxAge,
                                            const std::vector<IndexEntry>& allEntries);
    std::vector<PassengerRecord> ListSurvivors(const std::vector<IndexEntry>& allEntries);
};
