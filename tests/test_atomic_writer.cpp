#include <cassert>
#include <fstream>
#include <vector>

#include "storage/atomic_writer.h"

static std::vector<unsigned char> ReadAll(const std::string& path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return {};
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
}

int main() {
    std::vector<unsigned char> a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);

    bool ok = WriteAtomic("test_atomic.bin", a);
    assert(ok);

    std::vector<unsigned char> out = ReadAll("test_atomic.bin");
    assert(out == a);

    return 0;
}
