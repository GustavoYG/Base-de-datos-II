#pragma once

#include <string>
#include <vector>

bool WriteAtomic(const std::string& targetPath, const std::vector<unsigned char>& data);
