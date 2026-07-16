#pragma once

#include <string>
#include <vector>

namespace io {
std::vector<std::vector<std::string>> CsvReader_ReadAll(const std::string& path);
}
