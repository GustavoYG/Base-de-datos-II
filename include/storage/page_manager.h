#pragma once

#include <string>

#include "common/types.h"

class PageManager {
private:
    std::string path;

public:
    explicit PageManager(const std::string& filePath);
    int AllocatePage();
    bool WritePage(int pageId, const Page& page);
    bool ReadPage(int pageId, Page& outPage);
    int GetPageCount() const;
};
