#include "storage/file_io.h"

#include <fstream>

std::vector<unsigned char> ReadAllBinary(const std::string& path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return std::vector<unsigned char>();

    std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    return data;
}
