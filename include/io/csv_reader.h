#pragma once

#include <string>
#include <vector>

namespace io {
std::vector<std::vector<std::string>> CsvReader_ReadAll(const std::string& path);
// Parsea UNA sola linea CSV (respeta comillas). Usado para medicion de anchos
// en modo streaming sin cargar el archivo completo en RAM.
std::vector<std::string> CsvReader_ReadAll_one(const std::string& line);
}
