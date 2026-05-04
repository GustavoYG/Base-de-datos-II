#pragma once

#include <string>
#include <vector>

class CsvReader {
public:
    static std::vector<std::vector<std::string>> ReadAll(const std::string& path);
};
