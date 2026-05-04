#include "storage/wal_log.h"

#include <cstdio>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

struct WalHeader {
    unsigned int size;
    unsigned int checksum;
};

static unsigned int WalChecksum(const unsigned char* data, unsigned int len) {
    unsigned int sum = 0;
    for (unsigned int i = 0; i < len; ++i) sum += data[i];
    return sum;
}

static bool ForceFsync(FILE* fp) {
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

bool AppendWalEntry(const std::string& path, const std::string& payload) {
    FILE* fp = std::fopen(path.c_str(), "ab");
    if (!fp) return false;

    WalHeader h;
    h.size = (unsigned int)payload.size();
    h.checksum = WalChecksum((const unsigned char*)payload.data(), h.size);

    size_t wh = std::fwrite(&h, 1, sizeof(WalHeader), fp);
    if (wh != sizeof(WalHeader)) {
        std::fclose(fp);
        return false;
    }

    if (h.size > 0) {
        size_t wd = std::fwrite(payload.data(), 1, h.size, fp);
        if (wd != h.size) {
            std::fclose(fp);
            return false;
        }
    }

    bool ok = ForceFsync(fp);
    std::fclose(fp);
    return ok;
}

std::string ReadLastValidWalPayload(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return "";

    std::string last;
    while (true) {
        WalHeader h;
        size_t rh = std::fread(&h, 1, sizeof(WalHeader), fp);
        if (rh == 0) break;
        if (rh != sizeof(WalHeader)) break;
        if (h.size == 0) {
            last = "";
            continue;
        }

        std::vector<unsigned char> buf;
        buf.resize(h.size);
        size_t rd = std::fread(buf.data(), 1, h.size, fp);
        if (rd != h.size) break;

        unsigned int chk = WalChecksum(buf.data(), h.size);
        if (chk != h.checksum) break;

        last.assign((const char*)buf.data(), (size_t)h.size);
    }

    std::fclose(fp);
    return last;
}
