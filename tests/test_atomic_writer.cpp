#include <cassert>
#include <vector>

#include "storage/atomic_writer.h"
#include "storage/file_io.h"

int main() {
    std::vector<unsigned char> a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);

    bool ok = WriteAtomic("test_atomic.bin", a);
    assert(ok);

    std::vector<unsigned char> out = ReadAllBinary("test_atomic.bin");
    assert(out == a);

    return 0;
}
