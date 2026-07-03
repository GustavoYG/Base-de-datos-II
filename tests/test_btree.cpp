#include <iostream>
#include <cassert>
#include <cstring>
#include "../include/storage/page_manager.h"
#include "../include/storage/buffer_pool.h"
#include "../include/storage/btree.h"

using namespace storage;

void TestBPlusTreeBasics() {
    std::cout << "Testing B+ Tree Basics..." << std::endl;
    
    // Create temporary test files
    const char* testFile = "data/storage/tables/test_btree.tbl";
    
    // Clean up if they exist
    std::remove(testFile);
    
    // Initialize storage
    PageManager pageManager(testFile);
    BufferPool bufferPool(pageManager, 10);
    BPlusTree tree(&pageManager, &bufferPool);
    
    // Test 1: Single insertion
    std::cout << "  Test 1: Single insertion..." << std::endl;
    if (!tree.Insert(5, 1, 0)) {
        std::cout << "    ✗ Failed to insert key 5" << std::endl;
        return;
    }
    std::cout << "    ✓ Inserted key 5" << std::endl;
    
    uint32_t pageId;
    uint16_t slotNum;
    
    if (!tree.Search(5, pageId, slotNum)) {
        std::cout << "    ✗ Failed to find key 5" << std::endl;
        return;
    }
    
    if (pageId != 1) {
        std::cout << "    ✗ Key 5 has wrong pageId: " << pageId << " (expected 1)" << std::endl;
        return;
    }
    std::cout << "    ✓ Found key 5 at pageId=" << pageId << ", slotNum=" << slotNum << std::endl;
    
    // Test 2: More insertions
    std::cout << "  Test 2: Multiple insertions..." << std::endl;
    tree.Insert(3, 2, 1);
    tree.Insert(7, 3, 2);
    tree.Insert(2, 4, 3);
    tree.Insert(6, 5, 4);
    std::cout << "    ✓ Inserted 4 more keys" << std::endl;
    
    if (!tree.Search(3, pageId, slotNum) || pageId != 2) {
        std::cout << "    ✗ Failed to find key 3" << std::endl;
        return;
    }
    std::cout << "    ✓ All keys found" << std::endl;
    
    // Test 3: Non-existent key
    std::cout << "  Test 3: Search non-existent key..." << std::endl;
    if (tree.Search(100, pageId, slotNum)) {
        std::cout << "    ✗ Found non-existent key 100" << std::endl;
        return;
    }
    std::cout << "    ✓ Non-existent key correctly not found" << std::endl;
    
    std::cout << "✓ All B+ Tree tests passed!" << std::endl;
}

int main() {
    try {
        TestBPlusTreeBasics();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
