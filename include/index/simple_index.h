#pragma once

#include <vector>

#include "common/types.h"

class SimpleIndex {
private:
    std::vector<IndexEntry> entries;

public:
    void Add(int key, int pageId, int slot);
    void Build();
    bool Find(int key, IndexEntry& out) const;
    std::vector<IndexEntry> Range(int minKey, int maxKey) const;
    const std::vector<IndexEntry>& Entries() const;
};
