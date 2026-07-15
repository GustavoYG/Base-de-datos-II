#include "storage/table.h"

#include <string>

Table::Table(const std::string& name, const Schema& schema, const std::string& baseDir, ReplacementPolicy policy)
    : name_(name), schema_(schema), rm_(baseDir + "/" + name + ".bin", schema, policy) {
}

bool Table::InsertRow(const std::vector<unsigned char>& row, int& outPageId, int& outSlot) {
    if ((int)row.size() != schema_.rowSize) return false;
    return rm_.InsertRecord(row, outPageId, outSlot);
}

bool Table::ReadRow(int pageId, int slot, std::vector<unsigned char>& outRow) {
    return rm_.ReadRecord(pageId, slot, outRow);
}

bool Table::UpdateRow(int pageId, int slot, const std::vector<unsigned char>& row) {
    if ((int)row.size() != schema_.rowSize) return false;
    return rm_.UpdateRecord(pageId, slot, row);
}

bool Table::DeleteRow(int pageId, int slot) {
    return rm_.DeleteRecord(pageId, slot);
}
