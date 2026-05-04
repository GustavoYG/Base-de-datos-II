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
    std::vector<unsigned char> bytes = SerializeRecords(records);
    assert(bytes.size() == records.size() * sizeof(PassengerRecord));

    return 0;
}
