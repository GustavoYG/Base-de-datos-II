#include <cassert>

#include "io/csv_reader.h"
#include "io/serializer.h"

int main() {
    std::vector<std::vector<std::string>> rows = CsvReader::ReadAll("data/raw/titanic.csv");
    if (rows.empty()) {
        rows = CsvReader::ReadAll("titanic.csv");
    }
    assert(!rows.empty());

    std::vector<PassengerRecord> records = RowsToRecords(rows);
    assert(!records.empty());

    return 0;
}
