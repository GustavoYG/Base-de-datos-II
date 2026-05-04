#include "io/csv_reader.h"

#include <fstream>

static std::vector<std::string> SplitCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

std::vector<std::vector<std::string>> CsvReader::ReadAll(const std::string& path) {
    std::ifstream in(path.c_str());
    std::vector<std::vector<std::string>> rows;
    if (!in) return rows;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        rows.push_back(SplitCsvLine(line));
    }
    return rows;
}
