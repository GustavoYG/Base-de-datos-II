#include <iostream>
#include <filesystem>
#include <vector>

#include "storage/page_manager.h"
#include "storage/buffer_pool.h"
#include "common/types.h"

int main() {
    std::string path = "data/storage/tables/test_bp.tbl";
    std::filesystem::create_directories("data/storage/tables");
    if (std::filesystem::exists(path)) std::filesystem::remove(path);

    PageManager pm(path);
    // allocate 5 pages
    for (int i = 0; i < 5; ++i) pm.AllocatePage();

    BufferPool bp(pm, 3); // small pool to force eviction

    // Pin pages 0,1,2
    Page* p0 = bp.PinPage(0);
    Page* p1 = bp.PinPage(1);
    Page* p2 = bp.PinPage(2);
    if (!p0 || !p1 || !p2) { std::cerr<<"Pin failed"<<std::endl; return 2; }

    // modify p0
    p0->data[0] = 0x11;
    bp.UnpinPage(0, true);

    // Pin page 3 -> should evict one of pages 0/1/2 (LRU makes 1 or 2 candidate)
    Page* p3 = bp.PinPage(3);
    if (!p3) { std::cerr<<"Pin 3 failed"<<std::endl; return 3; }
    bp.UnpinPage(3, false);

    // Pin page 0 again, should load from disk with modification persisted
    Page* p0b = bp.PinPage(0);
    if (!p0b) { std::cerr<<"Pin 0b failed"<<std::endl; return 4; }
    if ((unsigned char)p0b->data[0] != 0x11) { std::cerr<<"Data not persisted"<<std::endl; return 5; }
    bp.UnpinPage(0, false);

    bp.FlushAll();
    std::cout << "BufferPool test passed" << std::endl;
    return 0;
}
