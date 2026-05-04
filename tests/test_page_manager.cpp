#include <cassert>
#include <cstring>

#include "storage/page_manager.h"
#include "common/utils.h"

int main() {
    PageManager pm("test_pages.bin");
    int pageId = pm.AllocatePage();

    Page page;
    std::memset(&page, 0, sizeof(Page));
    page.header.pageId = pageId;
    page.header.freeBytes = sizeof(page.data);
    page.data[0] = 42;
    page.header.checksum = SimpleChecksum(page.data, sizeof(page.data));

    assert(pm.WritePage(pageId, page));

    Page out;
    assert(pm.ReadPage(pageId, out));
    assert(out.data[0] == 42);

    return 0;
}
