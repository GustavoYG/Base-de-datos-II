#pragma once

#include <cstddef>
#include <string>

int SimpleChecksum(const unsigned char* data, int len);
void SafeCopy(char* dest, size_t destSize, const std::string& src);
