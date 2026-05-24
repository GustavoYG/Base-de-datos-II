#pragma once

#include <vector>
#include <cstring>
#include <unordered_map>
#include <list>
#include <mutex>
#include "common/types.h"
#include "storage/page_manager.h"

class BufferPool {
public:
    explicit BufferPool(PageManager& pm, int poolSize);
    // Pin a page into the buffer pool. Returns pointer to Page in frame or nullptr on failure.
    Page* PinPage(int pageId);
    // Unpin a page, optionally mark dirty
    bool UnpinPage(int pageId, bool isDirty);
    // Force flush all dirty pages to disk
    void FlushAll();

    ~BufferPool();

private:
    struct Frame {
        int pageId;
        Page page;
        bool dirty;
        int pinCount;
        Frame() : pageId(-1), dirty(false), pinCount(0) { ::memset(&page, 0, sizeof(Page)); }
    };

    PageManager& pm;
    int capacity;
    std::vector<Frame> frames;
    std::unordered_map<int, int> pageTable; // pageId -> frame index
    std::list<int> lru; // stores frame indices, front = most recent, back = least recent
    std::mutex mu;

    int FindVictim();
    void TouchFrameLRU(int frameIdx);
};
