#include "io/serializer.h"

#include <cstring>
#include <fstream>
#include <sstream>

#include "common/utils.h"

static float ToFloatOrZero(const std::string& s) {
    if (s.empty()) return 0.0f;
    return static_cast<float>(std::atof(s.c_str()));
}

PassengerRecord RowToRecord(const std::vector<std::string>& row) {
    PassengerRecord r;
    std::memset(&r, 0, sizeof(PassengerRecord));

    r.passengerId = std::atoi(row[0].c_str());
    r.survived = std::atoi(row[1].c_str());
    r.pclass = std::atoi(row[2].c_str());

    SafeCopy(r.name, sizeof(r.name), row[3]);
    SafeCopy(r.sex, sizeof(r.sex), row[4]);

    r.age = ToFloatOrZero(row[5]);
    r.sibSp = std::atoi(row[6].c_str());
    r.parch = std::atoi(row[7].c_str());

    SafeCopy(r.ticket, sizeof(r.ticket), row[8]);
    r.fare = ToFloatOrZero(row[9]);

    SafeCopy(r.cabin, sizeof(r.cabin), row[10]);
    SafeCopy(r.embarked, sizeof(r.embarked), row[11]);

    return r;
}

std::vector<PassengerRecord> RowsToRecords(const std::vector<std::vector<std::string>>& rows) {
    std::vector<PassengerRecord> out;
    if (rows.size() <= 1) return out;

    // Salta encabezado
    for (size_t i = 1; i < rows.size(); ++i) {
        if (rows[i].size() < 12) continue;
        out.push_back(RowToRecord(rows[i]));
    }
    return out;
}

std::vector<unsigned char> SerializeRecords(const std::vector<PassengerRecord>& records) {
    std::vector<unsigned char> out;
    if (records.empty()) return out;

    out.resize(records.size() * sizeof(PassengerRecord));
    std::memcpy(out.data(), records.data(), out.size());
    return out;
}

static std::string CleanField(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '|') out.push_back('/');
        else out.push_back(c);
    }
    return out;
}

static std::string ToStringField(const char* s) {
    if (!s) return "";
    return CleanField(std::string(s));
}

std::string SerializeRecordsText(const std::vector<PassengerRecord>& records) {
    std::ostringstream out;
    out << "PassengerId|Survived|Pclass|Name|Sex|Age|SibSp|Parch|Ticket|Fare|Cabin|Embarked";
    out << "\n";

    for (size_t i = 0; i < records.size(); ++i) {
        const PassengerRecord& r = records[i];
        out << r.passengerId << "|" << r.survived << "|" << r.pclass << "|";
        out << ToStringField(r.name) << "|" << ToStringField(r.sex) << "|";
        out << r.age << "|" << r.sibSp << "|" << r.parch << "|";
        out << ToStringField(r.ticket) << "|" << r.fare << "|";
        out << ToStringField(r.cabin) << "|" << ToStringField(r.embarked);
        out << "\n";
    }

    return out.str();
}

static std::vector<std::string> SplitTextLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '|') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

std::vector<PassengerRecord> LoadRecordsText(const std::string& path) {
    std::ifstream in(path.c_str());
    std::vector<PassengerRecord> records;
    if (!in) return records;

    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (first) {
            first = false;
            continue;
        }

        std::vector<std::string> row = SplitTextLine(line);
        if (row.size() < 12) continue;

        PassengerRecord r;
        std::memset(&r, 0, sizeof(PassengerRecord));

        r.passengerId = std::atoi(row[0].c_str());
        r.survived = std::atoi(row[1].c_str());
        r.pclass = std::atoi(row[2].c_str());
        SafeCopy(r.name, sizeof(r.name), row[3]);
        SafeCopy(r.sex, sizeof(r.sex), row[4]);
        r.age = ToFloatOrZero(row[5]);
        r.sibSp = std::atoi(row[6].c_str());
        r.parch = std::atoi(row[7].c_str());
        SafeCopy(r.ticket, sizeof(r.ticket), row[8]);
        r.fare = ToFloatOrZero(row[9]);
        SafeCopy(r.cabin, sizeof(r.cabin), row[10]);
        SafeCopy(r.embarked, sizeof(r.embarked), row[11]);

        records.push_back(r);
    }

    return records;
}
