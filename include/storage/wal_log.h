#pragma once

#include <string>

bool AppendWalEntry(const std::string& path, const std::string& payload);
std::string ReadLastValidWalPayload(const std::string& path);
