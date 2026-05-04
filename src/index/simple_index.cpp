#include "index/simple_index.h"

#include <algorithm>

void SimpleIndex::Add(int key, int pageId, int slot) {
    IndexEntry e;
    e.key = key;
    e.pageId = pageId;
    e.slot = slot;
    entries.push_back(e);
}

void SimpleIndex::Build() {
    std::sort(entries.begin(), entries.end(),
        [](const IndexEntry& a, const IndexEntry& b) {
            return a.key < b.key;
        });
}

bool SimpleIndex::Find(int key, IndexEntry& out) const {
    int left = 0;
    int right = (int)entries.size() - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        if (entries[mid].key == key) {
            out = entries[mid];
            return true;
        } else if (entries[mid].key < key) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return false;
}

std::vector<IndexEntry> SimpleIndex::Range(int minKey, int maxKey) const {
    std::vector<IndexEntry> out;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].key >= minKey && entries[i].key <= maxKey) {
            out.push_back(entries[i]);
        }
    }
    return out;
}

const std::vector<IndexEntry>& SimpleIndex::Entries() const {
    return entries;
}
