#include "storage/atomic_writer.h"

#include <cstdio>

#include "common/utils.h"

bool WriteAtomic(const std::string& targetPath, const std::vector<unsigned char>& data) {
    std::string tempPath = targetPath + ".tmp";

    FILE* fp = std::fopen(tempPath.c_str(), "wb");
    if (!fp) return false;

    if (!data.empty()) {
        size_t written = std::fwrite(data.data(), 1, data.size(), fp);
        if (written != data.size()) {
            std::fclose(fp);
            std::remove(tempPath.c_str());
            return false;
        }
    }

    if (!ForceFsync(fp)) {
        std::fclose(fp);
        std::remove(tempPath.c_str());
        return false;
    }

    std::fclose(fp);

    std::remove(targetPath.c_str());
    if (std::rename(tempPath.c_str(), targetPath.c_str()) != 0) {
        std::remove(tempPath.c_str());
        return false;
    }

    return true;
}
