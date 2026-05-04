#pragma once

#include <string>
#include <vector>

#include "common/types.h"

PassengerRecord RowToRecord(const std::vector<std::string>& row);
std::vector<PassengerRecord> RowsToRecords(const std::vector<std::vector<std::string>>& rows);
std::vector<unsigned char> SerializeRecords(const std::vector<PassengerRecord>& records);
std::string SerializeRecordsText(const std::vector<PassengerRecord>& records);
std::vector<PassengerRecord> LoadRecordsText(const std::string& path);
