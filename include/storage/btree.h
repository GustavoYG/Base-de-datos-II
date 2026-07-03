#ifndef BTREE_H
#define BTREE_H

#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include "page_manager.h"
#include "buffer_pool.h"

namespace storage {

// Simplified B+ Tree implementation
// Stores key -> (pageId, slotNum) mappings using in-memory B+ structure backed by pages

class BPlusTree {
public:
    // Constructor takes PageManager and BufferPool references
    BPlusTree(PageManager* pageManager, BufferPool* bufferPool);
    ~BPlusTree();
    
    // Core operations
    bool Insert(int64_t key, uint32_t pageId, uint16_t slotNum);
    bool Search(int64_t key, uint32_t& outPageId, uint16_t& outSlotNum);
    bool Delete(int64_t key);
    
    // Tree info
    int GetNodeCount() const { return static_cast<int>(keyIndex.size()); }
    int GetTreeHeight() const;
    void PrintTree() const;
    
private:
    PageManager* pageManager;
    BufferPool* bufferPool;
    
    // In-memory index mapping keys to (pageId, slotNum)
    std::map<int64_t, std::pair<uint32_t, uint16_t>> keyIndex;
    
    // Persistent storage identifier
    uint32_t treeRootPageId;
    
    // Load/Save tree from/to persistent storage
    void SaveTreeIndex();
    void LoadTreeIndex();
};

}  // namespace storage

#endif  // BTREE_H
