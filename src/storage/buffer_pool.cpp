#include "storage/buffer_pool.h"

#include <cstring>
#include <algorithm>
#include <iostream>
#include "common/utils.h"

BufferPool::BufferPool(PageManager& pm_, int poolSize) : pm(pm_), capacity(poolSize) {
    frames.resize(capacity);
    // LRU initially empty
}

BufferPool::~BufferPool() {
    FlushAll();
}

int BufferPool::FindVictim() {
    // Find a frame that is unpinned (pinCount==0) from back of LRU
    for (auto it = lru.rbegin(); it != lru.rend(); ++it) {
        int idx = *it;
        if (frames[idx].pinCount == 0) return idx;
    }
    // If no LRU entry (or none evictable), find any free frame (pageId == -1)
    for (int i = 0; i < capacity; ++i) {
        if (frames[i].pageId == -1) return i;
    }
    return -1; // no victim
}

void BufferPool::TouchFrameLRU(int frameIdx) {
    // move frameIdx to front (most recent)
    for (auto it = lru.begin(); it != lru.end(); ++it) {
        if (*it == frameIdx) { lru.erase(it); break; }
    }
    lru.push_front(frameIdx);
}

Page* BufferPool::PinPage(int pageId) {
    std::lock_guard<std::mutex> guard(mu);
    // if present
    auto it = pageTable.find(pageId);
    if (it != pageTable.end()) {
        int idx = it->second;
        frames[idx].pinCount++;
        TouchFrameLRU(idx);
        return &frames[idx].page;
    }

    // Need to load page
    int victim = FindVictim();
    if (victim == -1) return nullptr;

    // If victim contains a page, evict it
    if (frames[victim].pageId != -1) {
        if (frames[victim].dirty) {
            pm.WritePage(frames[victim].pageId, frames[victim].page);
        }
        pageTable.erase(frames[victim].pageId);
    }

    // Read page from disk
    if (!pm.ReadPage(pageId, frames[victim].page)) {
        std::cerr << "BufferPool: ReadPage failed for page " << pageId << "\n";
        return nullptr;
    }
    frames[victim].pageId = pageId;
    frames[victim].dirty = false;
    frames[victim].pinCount = 1;
    pageTable[pageId] = victim;
    TouchFrameLRU(victim);
    return &frames[victim].page;
}

bool BufferPool::UnpinPage(int pageId, bool isDirty) {
    std::lock_guard<std::mutex> guard(mu);
    auto it = pageTable.find(pageId);
    if (it == pageTable.end()) return false;
    int idx = it->second;
    if (isDirty) {
        frames[idx].dirty = true;
        // update checksum for the page before it gets flushed
        frames[idx].page.header.checksum = SimpleChecksum(frames[idx].page.data, sizeof(frames[idx].page.data));
    }
    if (frames[idx].pinCount > 0) frames[idx].pinCount--;
    // if pinCount goes to 0, update LRU order (already touched on PinPage)
    if (frames[idx].pinCount == 0) TouchFrameLRU(idx);
    return true;
}

void BufferPool::FlushAll() {
    std::lock_guard<std::mutex> guard(mu);
    for (int i = 0; i < capacity; ++i) {
        if (frames[i].pageId != -1 && frames[i].dirty) {
            pm.WritePage(frames[i].pageId, frames[i].page);
            frames[i].dirty = false;
        }
    }
}
