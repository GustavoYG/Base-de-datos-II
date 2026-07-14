#pragma once

#include <deque>
#include <cstring>
#include <unordered_map>
#include <list>
#include "common/types.h"
#include "storage/page_manager.h"

enum class ReplacementPolicy {
    LRU,
    CLOCK
};

class BufferPool {
public:
    BufferPool(PageManager& pm, int poolSize, ReplacementPolicy policy = ReplacementPolicy::LRU);
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
        bool referenced; // bit de referencia para la politica CLOCK
        Frame() : pageId(-1), dirty(false), pinCount(0), referenced(false) { ::memset(&page, 0, sizeof(Page)); }
    };

    PageManager& pm;
    int capacity;
    ReplacementPolicy policy;
    std::deque<Frame> frames;
    std::unordered_map<int, int> pageTable; // pageId -> frame index
    std::list<int> lru; // frame indices (solo LRU); front = mas reciente
    int clockHand;      // puntero del reloj (solo CLOCK)

    int FindVictim();
    int FindVictimLRU();
    int FindVictimClock();
    void TouchFrameLRU(int frameIdx);
};
