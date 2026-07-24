#pragma once

#include <cstddef>
#include <cstdio>
#include <string>

int SimpleChecksum(const unsigned char* data, int len);
void SafeCopy(char* dest, size_t destSize, const std::string& src);
bool ForceFsync(FILE* fp);
std::string ToLower(const std::string& s);
