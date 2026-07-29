#include "sql/operators.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <unordered_map>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "storage/record_manager.h"
#include "index/bplus_tree.h"
#include "index/index_manager.h"
#include "io/serializer.h"
#include "common/utils.h"

namespace {

static bool IsNumeric(const std::string& s) {
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0';
}

static bool IsBoolVal(const std::string& s, bool& out) {
    std::string low = ToLower(s);
    if (low == "true" || low == "1") { out = true; return true; }
    if (low == "false" || low == "0") { out = false; return true; }
    return false;
}

static int CompareValues(const std::string& a, const std::string& b) {
    bool boolA = false, boolB = false;
    if (IsBoolVal(a, boolA) && IsBoolVal(b, boolB)) {
        if (boolA == boolB) return 0;
        return boolA ? 1 : -1;
    }
    if (IsNumeric(a) && IsNumeric(b)) {
        double x = std::strtod(a.c_str(), nullptr);
        double y = std::strtod(b.c_str(), nullptr);
        if (x < y) return -1;
        if (x > y) return 1;
        return 0;
    }
    return a.compare(b);
}

static bool EvaluateCondition(const std::string& cell, CmpOperator op, const std::string& val) {
    int c = CompareValues(cell, val);
    switch (op) {
        case CmpOperator::EQ: return c == 0;
        case CmpOperator::NE: return c != 0;
        case CmpOperator::GT: return c > 0;
        case CmpOperator::LT: return c < 0;
        case CmpOperator::GE: return c >= 0;
        case CmpOperator::LE: return c <= 0;
    }
    return false;
}

} // namespace

// ---- 1. ScanOperator ----
ScanOperator::ScanOperator(const StoredTable& table) : table_(table) {
    for (const auto& col : table_.schema.columns) {
        outputCols_.push_back(col.name);
    }
}

void ScanOperator::Open() {
    materializedRows_.clear();
    currentIndex_ = 0;

    RecordManager rm(table_.binPath, table_.schema, ReplacementPolicy::LRU);
    std::vector<std::pair<int, int>> locs;
    rm.ScanAll(locs);

    for (const auto& loc : locs) {
        std::vector<unsigned char> rowBytes;
        if (!rm.ReadRecord(loc.first, loc.second, rowBytes)) continue;
        if ((int)rowBytes.size() != table_.schema.rowSize) continue;

        Tuple t;
        t.values.reserve(table_.schema.columns.size());
        for (const auto& col : table_.schema.columns) {
            switch (col.type) {
                case ColumnType::INT32:  t.values.push_back(std::to_string(GetFieldInt32(table_.schema, rowBytes, col.name))); break;
                case ColumnType::INT64:  t.values.push_back(std::to_string(GetFieldInt64(table_.schema, rowBytes, col.name))); break;
                case ColumnType::FLOAT:  t.values.push_back(std::to_string(GetFieldFloat(table_.schema, rowBytes, col.name))); break;
                case ColumnType::DOUBLE: t.values.push_back(std::to_string(GetFieldDouble(table_.schema, rowBytes, col.name))); break;
                case ColumnType::BOOL:   t.values.push_back(GetFieldBool(table_.schema, rowBytes, col.name) ? "true" : "false"); break;
                default:                 t.values.push_back(GetFieldString(table_.schema, rowBytes, col.name)); break;
            }
        }
        materializedRows_.push_back(std::move(t));
    }
}

bool ScanOperator::Next(Tuple& outTuple) {
    if (currentIndex_ < materializedRows_.size()) {
        outTuple = materializedRows_[currentIndex_++];
        return true;
    }
    return false;
}

void ScanOperator::Close() {
    materializedRows_.clear();
    currentIndex_ = 0;
}


// ---- 2. IndexScanOperator ----
IndexScanOperator::IndexScanOperator(const StoredTable& table, const std::string& colName, const std::string& targetVal)
    : table_(table), colName_(colName), targetVal_(targetVal) {
    for (const auto& col : table_.schema.columns) {
        outputCols_.push_back(col.name);
    }
}

void IndexScanOperator::Open() {
    resultRows_.clear();
    currentIndex_ = 0;

    std::string cleanCol = StripTablePrefix(colName_);
    int colIdx = table_.GetColumnIndex(cleanCol);
    if (colIdx < 0) return;
    const ColumnDef& cdef = table_.schema.columns[colIdx];

    std::string indexPath = IndexManager::GetIndexPath(table_.name, cleanCol);

    BPlusTree bTree(indexPath, cdef.type);

    BTreeKey key;
    if (cdef.type == ColumnType::INT32) {
        BTreeKeyFromInt32(key, (int32_t)std::strtol(targetVal_.c_str(), nullptr, 10));
    } else if (cdef.type == ColumnType::FLOAT) {
        BTreeKeyFromFloat(key, std::strtof(targetVal_.c_str(), nullptr));
    } else {
        BTreeKeyFromString(key, targetVal_, cdef.length);
    }

    int32_t pageId = -1;
    int16_t slot = -1;
    if (bTree.Find(key, pageId, slot)) {
        RecordManager rm(table_.binPath, table_.schema, ReplacementPolicy::LRU);
        std::vector<unsigned char> rowBytes;
        if (rm.ReadRecord(pageId, slot, rowBytes) && (int)rowBytes.size() == table_.schema.rowSize) {
            Tuple t;
            t.values.reserve(table_.schema.columns.size());
            for (const auto& col : table_.schema.columns) {
                switch (col.type) {
                    case ColumnType::INT32:  t.values.push_back(std::to_string(GetFieldInt32(table_.schema, rowBytes, col.name))); break;
                    case ColumnType::INT64:  t.values.push_back(std::to_string(GetFieldInt64(table_.schema, rowBytes, col.name))); break;
                    case ColumnType::FLOAT:  t.values.push_back(std::to_string(GetFieldFloat(table_.schema, rowBytes, col.name))); break;
                    case ColumnType::DOUBLE: t.values.push_back(std::to_string(GetFieldDouble(table_.schema, rowBytes, col.name))); break;
                    case ColumnType::BOOL:   t.values.push_back(GetFieldBool(table_.schema, rowBytes, col.name) ? "true" : "false"); break;
                    default:                 t.values.push_back(GetFieldString(table_.schema, rowBytes, col.name)); break;
                }
            }
            resultRows_.push_back(std::move(t));
        }
    }
}

bool IndexScanOperator::Next(Tuple& outTuple) {
    if (currentIndex_ < resultRows_.size()) {
        outTuple = resultRows_[currentIndex_++];
        return true;
    }
    return false;
}

void IndexScanOperator::Close() {
    resultRows_.clear();
    currentIndex_ = 0;
}


// ---- 3. SelectOperator ----
SelectOperator::SelectOperator(std::unique_ptr<Operator> child, const std::string& colName, CmpOperator op, const std::string& value)
    : child_(std::move(child)), colName_(colName), op_(op), value_(value) {}

void SelectOperator::Open() {
    child_->Open();
    const auto& cols = child_->GetOutputColumns();
    colIndex_ = -1;
    for (size_t i = 0; i < cols.size(); ++i) {
        if (ToLower(StripTablePrefix(cols[i])) == ToLower(StripTablePrefix(colName_))) {
            colIndex_ = (int)i;
            break;
        }
    }
}

bool SelectOperator::Next(Tuple& outTuple) {
    if (colIndex_ < 0) return false;
    Tuple t;
    while (child_->Next(t)) {
        if ((size_t)colIndex_ < t.values.size()) {
            if (EvaluateCondition(t.values[colIndex_], op_, value_)) {
                outTuple = std::move(t);
                return true;
            }
        }
    }
    return false;
}

void SelectOperator::Close() {
    child_->Close();
}


// ---- 4. ProjectOperator ----
ProjectOperator::ProjectOperator(std::unique_ptr<Operator> child, const std::vector<std::string>& projectCols)
    : child_(std::move(child)), projectCols_(projectCols) {}

void ProjectOperator::Open() {
    child_->Open();
    const auto& childCols = child_->GetOutputColumns();
    colIndices_.clear();

    for (const auto& pCol : projectCols_) {
        int idx = -1;
        for (size_t i = 0; i < childCols.size(); ++i) {
            if (ToLower(StripTablePrefix(childCols[i])) == ToLower(StripTablePrefix(pCol))) {
                idx = (int)i;
                break;
            }
        }
        colIndices_.push_back(idx);
    }
}

bool ProjectOperator::Next(Tuple& outTuple) {
    Tuple childTuple;
    if (child_->Next(childTuple)) {
        outTuple.values.clear();
        outTuple.values.reserve(colIndices_.size());
        for (int idx : colIndices_) {
            if (idx >= 0 && (size_t)idx < childTuple.values.size()) {
                outTuple.values.push_back(childTuple.values[idx]);
            } else {
                outTuple.values.push_back("NULL");
            }
        }
        return true;
    }
    return false;
}

void ProjectOperator::Close() {
    child_->Close();
}


// ---- 5. SortOperator ----
SortOperator::SortOperator(std::unique_ptr<Operator> child, const std::vector<SortKey>& sortKeys)
    : child_(std::move(child)), sortKeys_(sortKeys) {}

void SortOperator::Open() {
    child_->Open();
    sortedTuples_.clear();
    currentIndex_ = 0;

    Tuple t;
    while (child_->Next(t)) {
        sortedTuples_.push_back(std::move(t));
    }

    const auto& cols = child_->GetOutputColumns();
    std::vector<std::pair<int, bool>> keyIndices;
    for (const auto& sk : sortKeys_) {
        for (size_t i = 0; i < cols.size(); ++i) {
            if (ToLower(StripTablePrefix(cols[i])) == ToLower(StripTablePrefix(sk.colName))) {
                keyIndices.push_back({(int)i, sk.isAscending});
                break;
            }
        }
    }

    std::sort(sortedTuples_.begin(), sortedTuples_.end(), [&](const Tuple& a, const Tuple& b) {
        for (const auto& ki : keyIndices) {
            int idx = ki.first;
            bool isAsc = ki.second;
            if (idx < 0 || (size_t)idx >= a.values.size() || (size_t)idx >= b.values.size()) continue;

            int cmp = CompareValues(a.values[idx], b.values[idx]);
            if (cmp != 0) {
                return isAsc ? (cmp < 0) : (cmp > 0);
            }
        }
        return false;
    });
}

bool SortOperator::Next(Tuple& outTuple) {
    if (currentIndex_ < sortedTuples_.size()) {
        outTuple = sortedTuples_[currentIndex_++];
        return true;
    }
    return false;
}

void SortOperator::Close() {
    sortedTuples_.clear();
    currentIndex_ = 0;
    child_->Close();
}


// ---- 6. HashAggregateOperator ----
HashAggregateOperator::HashAggregateOperator(std::unique_ptr<Operator> child,
                                             const std::vector<std::string>& groupByCols,
                                             const std::vector<AggregateExpr>& aggExprs)
    : child_(std::move(child)), groupByCols_(groupByCols), aggExprs_(aggExprs) {
    for (const auto& g : groupByCols_) outputCols_.push_back(g);
    for (const auto& a : aggExprs_) outputCols_.push_back(a.alias);
}

void HashAggregateOperator::Open() {
    child_->Open();
    aggregatedTuples_.clear();
    currentIndex_ = 0;

    const auto& childCols = child_->GetOutputColumns();

    std::vector<int> groupIndices;
    for (const auto& g : groupByCols_) {
        for (size_t i = 0; i < childCols.size(); ++i) {
            if (ToLower(StripTablePrefix(childCols[i])) == ToLower(StripTablePrefix(g))) {
                groupIndices.push_back((int)i);
                break;
            }
        }
    }

    struct AggState {
        long long count = 0;
        double sum = 0.0;
        double minVal = 1e308;
        double maxVal = -1e308;
        std::string minStr;
        std::string maxStr;
        bool hasValue = false;
    };

    // key -> list of AggState per AggregateExpr
    std::map<std::vector<std::string>, std::vector<AggState>> groupMap;

    Tuple t;
    while (child_->Next(t)) {
        std::vector<std::string> key;
        for (int idx : groupIndices) {
            if (idx >= 0 && (size_t)idx < t.values.size()) {
                key.push_back(t.values[idx]);
            } else {
                key.push_back("NULL");
            }
        }

        auto& states = groupMap[key];
        if (states.empty()) states.resize(aggExprs_.size());

        for (size_t i = 0; i < aggExprs_.size(); ++i) {
            const auto& expr = aggExprs_[i];
            auto& st = states[i];

            std::string valStr;
            if (expr.colName != "*") {
                for (size_t c = 0; c < childCols.size(); ++c) {
                    if (ToLower(StripTablePrefix(childCols[c])) == ToLower(StripTablePrefix(expr.colName))) {
                        if (c < t.values.size()) valStr = t.values[c];
                        break;
                    }
                }
            }

            st.count++;
            if (!valStr.empty() && IsNumeric(valStr)) {
                double num = std::strtod(valStr.c_str(), nullptr);
                st.sum += num;
                if (!st.hasValue || num < st.minVal) st.minVal = num;
                if (!st.hasValue || num > st.maxVal) st.maxVal = num;
                st.hasValue = true;
            }
        }
    }

    for (const auto& kv : groupMap) {
        Tuple resTuple;
        resTuple.values = kv.first; // Claves del GROUP BY

        for (size_t i = 0; i < aggExprs_.size(); ++i) {
            const auto& expr = aggExprs_[i];
            const auto& st = kv.second[i];

            switch (expr.type) {
                case AggType::COUNT:
                    resTuple.values.push_back(std::to_string(st.count));
                    break;
                case AggType::SUM:
                    resTuple.values.push_back(std::to_string(st.sum));
                    break;
                case AggType::AVG:
                    resTuple.values.push_back(std::to_string(st.count > 0 ? st.sum / st.count : 0.0));
                    break;
                case AggType::MIN:
                    resTuple.values.push_back(st.hasValue ? std::to_string(st.minVal) : "0");
                    break;
                case AggType::MAX:
                    resTuple.values.push_back(st.hasValue ? std::to_string(st.maxVal) : "0");
                    break;
            }
        }
        aggregatedTuples_.push_back(std::move(resTuple));
    }
}

bool HashAggregateOperator::Next(Tuple& outTuple) {
    if (currentIndex_ < aggregatedTuples_.size()) {
        outTuple = aggregatedTuples_[currentIndex_++];
        return true;
    }
    return false;
}

void HashAggregateOperator::Close() {
    aggregatedTuples_.clear();
    currentIndex_ = 0;
    child_->Close();
}


// ---- 7. NestedLoopJoinOperator ----
NestedLoopJoinOperator::NestedLoopJoinOperator(std::unique_ptr<Operator> leftChild,
                                               std::unique_ptr<Operator> rightChild,
                                               const std::string& leftCol,
                                               const std::string& rightCol)
    : leftChild_(std::move(leftChild)), rightChild_(std::move(rightChild)), leftCol_(leftCol), rightCol_(rightCol) {}

void NestedLoopJoinOperator::Open() {
    leftChild_->Open();
    rightChild_->Open();
    joinedTuples_.clear();
    currentIndex_ = 0;

    const auto& lCols = leftChild_->GetOutputColumns();
    const auto& rCols = rightChild_->GetOutputColumns();

    outputCols_.clear();
    for (const auto& c : lCols) outputCols_.push_back(c);
    for (const auto& c : rCols) outputCols_.push_back(c);

    int lIdx = -1, rIdx = -1;
    for (size_t i = 0; i < lCols.size(); ++i) {
        if (ToLower(StripTablePrefix(lCols[i])) == ToLower(StripTablePrefix(leftCol_))) { lIdx = (int)i; break; }
    }
    for (size_t i = 0; i < rCols.size(); ++i) {
        if (ToLower(StripTablePrefix(rCols[i])) == ToLower(StripTablePrefix(rightCol_))) { rIdx = (int)i; break; }
    }

    if (lIdx < 0 || rIdx < 0) return;

    std::vector<Tuple> leftTuples, rightTuples;
    Tuple lt, rt;
    while (leftChild_->Next(lt)) leftTuples.push_back(std::move(lt));
    while (rightChild_->Next(rt)) rightTuples.push_back(std::move(rt));

    for (const auto& l : leftTuples) {
        for (const auto& r : rightTuples) {
            if ((size_t)lIdx < l.values.size() && (size_t)rIdx < r.values.size()) {
                if (CompareValues(l.values[lIdx], r.values[rIdx]) == 0) {
                    Tuple jt;
                    jt.values = l.values;
                    for (const auto& v : r.values) jt.values.push_back(v);
                    joinedTuples_.push_back(std::move(jt));
                }
            }
        }
    }
}

bool NestedLoopJoinOperator::Next(Tuple& outTuple) {
    if (currentIndex_ < joinedTuples_.size()) {
        outTuple = joinedTuples_[currentIndex_++];
        return true;
    }
    return false;
}

void NestedLoopJoinOperator::Close() {
    joinedTuples_.clear();
    currentIndex_ = 0;
    leftChild_->Close();
    rightChild_->Close();
}


// ---- 8. LimitOffsetOperator ----
LimitOffsetOperator::LimitOffsetOperator(std::unique_ptr<Operator> child, int limit, int offset)
    : child_(std::move(child)), limit_(limit), offset_(offset) {}

void LimitOffsetOperator::Open() {
    child_->Open();
    returnedCount_ = 0;

    // Salta las primeras offset_ tuplas
    Tuple t;
    for (int i = 0; i < offset_; ++i) {
        if (!child_->Next(t)) break;
    }
}

bool LimitOffsetOperator::Next(Tuple& outTuple) {
    if (limit_ >= 0 && returnedCount_ >= limit_) return false;
    if (child_->Next(outTuple)) {
        returnedCount_++;
        return true;
    }
    return false;
}

void LimitOffsetOperator::Close() {
    child_->Close();
}
