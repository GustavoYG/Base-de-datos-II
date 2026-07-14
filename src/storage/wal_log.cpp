#include "storage/wal_log.h"

#include <cstdio>
#include <vector>

#include "common/utils.h"

struct WalHeader {
    unsigned int size;
    unsigned int checksum;
};

bool AppendWalEntry(const std::string& path, const std::string& payload) {
    FILE* fp = std::fopen(path.c_str(), "ab");
    if (!fp) return false;

    WalHeader h;
    h.size = (unsigned int)payload.size();
    h.checksum = (unsigned int)SimpleChecksum((const unsigned char*)payload.data(), (int)h.size);

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

        unsigned int chk = (unsigned int)SimpleChecksum(buf.data(), (int)h.size);
        if (chk != h.checksum) break;

        last.assign((const char*)buf.data(), (size_t)h.size);
    }

    std::fclose(fp);
    return last;
}
