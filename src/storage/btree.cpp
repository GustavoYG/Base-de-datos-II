#include "storage/btree.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace storage {

BPlusTree::BPlusTree(PageManager* pageManager, BufferPool* bufferPool)
    : pageManager(pageManager), bufferPool(bufferPool), treeRootPageId(0) {
    if (pageManager) {
        // Initialize tree by loading any existing index
        LoadTreeIndex();
    }
}

BPlusTree::~BPlusTree() {
    // Save index before destruction
    SaveTreeIndex();
}

bool BPlusTree::Insert(int64_t key, uint32_t pageId, uint16_t slotNum) {
    // Check for duplicate
    if (keyIndex.find(key) != keyIndex.end()) {
        return false;  // Key already exists
    }
    
    // Insert into in-memory B+ tree index
    keyIndex[key] = {pageId, slotNum};
    
    // Periodically save to persistent storage
    if (keyIndex.size() % 100 == 0) {
        SaveTreeIndex();
    }
    
    return true;
}

bool BPlusTree::Search(int64_t key, uint32_t& outPageId, uint16_t& outSlotNum) {
    auto it = keyIndex.find(key);
    if (it == keyIndex.end()) {
        return false;  // Key not found
    }
    
    outPageId = it->second.first;
    outSlotNum = it->second.second;
    return true;
}

bool BPlusTree::Delete(int64_t key) {
    auto it = keyIndex.find(key);
    if (it == keyIndex.end()) {
        return false;  // Key not found
    }
    
    keyIndex.erase(it);
    
    // Periodically save to persistent storage
    if (keyIndex.size() % 100 == 0) {
        SaveTreeIndex();
    }
    
    return true;
}

int BPlusTree::GetTreeHeight() const {
    if (keyIndex.empty()) return 0;
    
    // Simple height estimation based on number of keys
    int numKeys = keyIndex.size();
    int height = 1;
    int capacity = 256;  // Approx keys per node
    
    while (capacity < numKeys) {
        height++;
        capacity *= 256;
    }
    
    return height;
}

void BPlusTree::PrintTree() const {
    std::cout << "B+ Tree Index" << std::endl;
    std::cout << "  Keys in index: " << keyIndex.size() << std::endl;
    std::cout << "  Estimated height: " << GetTreeHeight() << std::endl;
    
    if (keyIndex.size() > 0 && keyIndex.size() <= 20) {
        std::cout << "  Index contents:" << std::endl;
        for (const auto& entry : keyIndex) {
            std::cout << "    Key " << entry.first << " -> (pageId=" << entry.second.first
                      << ", slotNum=" << entry.second.second << ")" << std::endl;
        }
    }
}

void BPlusTree::SaveTreeIndex() {
    if (!pageManager || keyIndex.empty()) {
        return;
    }
    
    try {
        // Allocate a page for the index
        if (treeRootPageId == 0) {
            treeRootPageId = pageManager->AllocatePage();
        }
        
        Page indexPage;
        indexPage.header.pageId = treeRootPageId;
        
        // Simple serialization: store count + key pairs
        size_t offset = 0;
        unsigned char* dataPtr = indexPage.data;
        
        // Write number of keys
        uint32_t numKeys = static_cast<uint32_t>(keyIndex.size());
        if (offset + sizeof(uint32_t) <= PAGE_SIZE - sizeof(PageHeader)) {
            std::memcpy(dataPtr + offset, &numKeys, sizeof(uint32_t));
            offset += sizeof(uint32_t);
        }
        
        // Write key-value pairs
        for (const auto& entry : keyIndex) {
            if (offset + sizeof(int64_t) + sizeof(uint32_t) + sizeof(uint16_t) 
                > PAGE_SIZE - sizeof(PageHeader)) {
                break;  // Page full
            }
            
            // Write key
            int64_t key = entry.first;
            std::memcpy(dataPtr + offset, &key, sizeof(int64_t));
            offset += sizeof(int64_t);
            
            // Write pageId
            uint32_t pageId = entry.second.first;
            std::memcpy(dataPtr + offset, &pageId, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            
            // Write slotNum
            uint16_t slotNum = entry.second.second;
            std::memcpy(dataPtr + offset, &slotNum, sizeof(uint16_t));
            offset += sizeof(uint16_t);
        }
        
        pageManager->WritePage(treeRootPageId, indexPage);
    } catch (const std::exception& e) {
        std::cerr << "Error saving tree index: " << e.what() << std::endl;
    }
}

void BPlusTree::LoadTreeIndex() {
    if (!pageManager || treeRootPageId == 0) {
        return;
    }
    
    try {
        Page indexPage;
        if (!pageManager->ReadPage(treeRootPageId, indexPage)) {
            return;  // Page doesn't exist yet
        }
        
        keyIndex.clear();
        size_t offset = 0;
        unsigned char* dataPtr = indexPage.data;
        
        // Read number of keys
        if (offset + sizeof(uint32_t) > PAGE_SIZE - sizeof(PageHeader)) {
            return;
        }
        
        uint32_t numKeys;
        std::memcpy(&numKeys, dataPtr + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Read key-value pairs
        for (uint32_t i = 0; i < numKeys; ++i) {
            if (offset + sizeof(int64_t) + sizeof(uint32_t) + sizeof(uint16_t) 
                > PAGE_SIZE - sizeof(PageHeader)) {
                break;
            }
            
            // Read key
            int64_t key;
            std::memcpy(&key, dataPtr + offset, sizeof(int64_t));
            offset += sizeof(int64_t);
            
            // Read pageId
            uint32_t pageId;
            std::memcpy(&pageId, dataPtr + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            
            // Read slotNum
            uint16_t slotNum;
            std::memcpy(&slotNum, dataPtr + offset, sizeof(uint16_t));
            offset += sizeof(uint16_t);
            
            keyIndex[key] = {pageId, slotNum};
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading tree index: " << e.what() << std::endl;
    }
}

}  // namespace storage
