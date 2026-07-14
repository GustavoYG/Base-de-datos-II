#include "common/utils.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

int SimpleChecksum(const unsigned char* data, int len) {
    int sum = 0;
    for (int i = 0; i < len; ++i) sum += data[i];
    return sum;
}

void SafeCopy(char* dest, size_t destSize, const std::string& src) {
    if (!dest || destSize == 0) return;
    for (size_t i = 0; i < destSize; ++i) dest[i] = '\0';
    size_t limit = destSize - 1;
    size_t count = src.size() < limit ? src.size() : limit;
    for (size_t i = 0; i < count; ++i) dest[i] = src[i];
}

bool ForceFsync(FILE* fp) {
    if (!fp) return false;
    std::fflush(fp);
#ifdef _WIN32
    int fd = _fileno(fp);
    return _commit(fd) == 0;
#else
    int fd = fileno(fp);
    return fsync(fd) == 0;
#endif
}
