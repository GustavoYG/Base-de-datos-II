#include "common/utils.h"

#include <numeric>
#include <cstring>
#include <cctype>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

int SimpleChecksum(const unsigned char* data, int len) {
    return (int)std::accumulate(data, data + len, 0);
}

void SafeCopy(char* dest, size_t destSize, const std::string& src) {
    if (!dest || destSize == 0) return;
    std::snprintf(dest, destSize, "%.*s", (int)src.size(), src.c_str());
    // El buffer de ancho fijo debe quedar limpio mas alla del null terminator.
    std::memset(dest + std::strlen(dest), 0, destSize - std::strlen(dest));
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

std::string ToLower(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}
