#include "storage/buffer_pool.h"

#include <cstring>
#include <algorithm>
#include <iostream>
#include "common/utils.h"

BufferPool::BufferPool(PageManager& pm_, int poolSize, ReplacementPolicy policy_)
    : pm(pm_), capacity(poolSize), policy(policy_), clockHand(0) {
    frames.resize(capacity);
}

BufferPool::~BufferPool() {
    FlushAll();
}

int BufferPool::FindVictim() {
    if (policy == ReplacementPolicy::CLOCK) return FindVictimClock();
    return FindVictimLRU();
}

int BufferPool::FindVictimLRU() {
    // Buscar un frame desfijado (pinCount==0) desde el final de la LRU
    for (auto it = lru.rbegin(); it != lru.rend(); ++it) {
        int idx = *it;
        if (frames[idx].pinCount == 0) return idx;
    }
    // Si no hay entrada LRU evictable, buscar un frame libre (pageId == -1)
    for (int i = 0; i < (int)frames.size(); ++i) {
        if (frames[i].pageId == -1) return i;
    }
    // Todas las paginas estan fijadas: ampliar el pool para no interbloquearse
    // durante operaciones que fijan varias paginas a la vez (p.ej. split de
    // un B+ Tree). deque mantiene estables los Page* ya entregados.
    frames.push_back(Frame{});
    return (int)frames.size() - 1;
}

int BufferPool::FindVictimClock() {
    const int n = (int)frames.size();
    // 1) Un frame libre tiene prioridad sobre el recorrido del reloj
    for (int i = 0; i < n; ++i) {
        if (frames[i].pageId == -1) return i;
    }
    // 2) Recorrer el reloj dando "second chance": si el bit de referencia
    //    esta en 1 lo ponemos en 0 y avanzamos; si esta en 0, es la victima.
    int scanned = 0;
    const int limit = n * 2; // hasta dos vueltas completas para garantizar progreso
    while (scanned < limit) {
        int idx = clockHand % n;
        clockHand = (clockHand + 1) % n;
        scanned++;
        if (frames[idx].pinCount != 0) continue; // fijado: no elegible
        if (frames[idx].referenced) {
            frames[idx].referenced = false;
        } else {
            return idx;
        }
    }
    // Todas fijadas: ampliar el pool (igual que LRU) para no interbloquear.
    frames.push_back(Frame{});
    return (int)frames.size() - 1;
}

void BufferPool::TouchFrameLRU(int frameIdx) {
    // move frameIdx to front (most recent)
    for (auto it = lru.begin(); it != lru.end(); ++it) {
        if (*it == frameIdx) { lru.erase(it); break; }
    }
    lru.push_front(frameIdx);
}

Page* BufferPool::PinPage(int pageId) {
    // if present
    auto it = pageTable.find(pageId);
    if (it != pageTable.end()) {
        int idx = it->second;
        frames[idx].pinCount++;
        if (policy == ReplacementPolicy::LRU) TouchFrameLRU(idx);
        else frames[idx].referenced = true;
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
    frames[victim].referenced = true;
    pageTable[pageId] = victim;
    if (policy == ReplacementPolicy::LRU) TouchFrameLRU(victim);
    return &frames[victim].page;
}

bool BufferPool::UnpinPage(int pageId, bool isDirty) {
    auto it = pageTable.find(pageId);
    if (it == pageTable.end()) return false;
    int idx = it->second;
    if (isDirty) {
        frames[idx].dirty = true;
        // update checksum for the page before it gets flushed
        frames[idx].page.header.checksum = SimpleChecksum(frames[idx].page.data, sizeof(frames[idx].page.data));
    }
    if (frames[idx].pinCount > 0) frames[idx].pinCount--;
    // Al quedar desfijado: LRU reordena; CLOCK ya tiene el bit de referencia.
    if (frames[idx].pinCount == 0 && policy == ReplacementPolicy::LRU) TouchFrameLRU(idx);
    return true;
}

void BufferPool::FlushAll() {
    for (int i = 0; i < (int)frames.size(); ++i) {
        if (frames[i].pageId != -1 && frames[i].dirty) {
            pm.WritePage(frames[i].pageId, frames[i].page);
            frames[i].dirty = false;
        }
    }
}
