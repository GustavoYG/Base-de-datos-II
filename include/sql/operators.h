#pragma once

#include "sql/operator.h"

#include <vector>
#include <string>
#include <memory>
#include <functional>

#include "db/catalog.h"
#include "common/schema.h"

enum class CmpOperator { EQ, NE, GT, LT, GE, LE };

enum class AggType { COUNT, SUM, AVG, MIN, MAX };

struct AggregateExpr {
    AggType type;
    std::string colName; // "*" para COUNT(*)
    std::string alias;
};

// 1. ScanOperator: Escaneo secuencial completo sobre una tabla en disco
class ScanOperator : public Operator {
public:
    ScanOperator(const StoredTable& table);
    ~ScanOperator() override = default;

    void Open() override;
    bool Next(Tuple& outTuple) override;
    void Close() override;
    const std::vector<std::string>& GetOutputColumns() const override { return outputCols_; }

private:
    const StoredTable& table_;
    std::vector<std::string> outputCols_;
    std::vector<Tuple> materializedRows_;
    size_t currentIndex_ = 0;
};

// 2. IndexScanOperator: Busqueda por indice B+ Tree para igualdad
class IndexScanOperator : public Operator {
public:
    IndexScanOperator(const StoredTable& table, const std::string& colName, const std::string& targetVal);
    ~IndexScanOperator() override = default;

    void Open() override;
    bool Next(Tuple& outTuple) override;
    void Close() override;
    const std::vector<std::string>& GetOutputColumns() const override { return outputCols_; }

private:
    const StoredTable& table_;
    std::string colName_;
    std::string targetVal_;
    std::vector<std::string> outputCols_;
    std::vector<Tuple> resultRows_;
    size_t currentIndex_ = 0;
};

// 3. SelectOperator: Filtra tuplas segun un predicado (col op val)
class SelectOperator : public Operator {
public:
    SelectOperator(std::unique_ptr<Operator> child, const std::string& colName, CmpOperator op, const std::string& value);
    ~SelectOperator() override = default;

    void Open() override;
    bool Next(Tuple& outTuple) override;
    void Close() override;
    const std::vector<std::string>& GetOutputColumns() const override { return child_->GetOutputColumns(); }

private:
    std::unique_ptr<Operator> child_;
    std::string colName_;
    CmpOperator op_;
    std::string value_;
    int colIndex_ = -1;
};

// 4. ProjectOperator: Proyecta y reordena columnas especificas
class ProjectOperator : public Operator {
public:
    ProjectOperator(std::unique_ptr<Operator> child, const std::vector<std::string>& projectCols);
    ~ProjectOperator() override = default;

    void Open() override;
    bool Next(Tuple& outTuple) override;
    void Close() override;
    const std::vector<std::string>& GetOutputColumns() const override { return projectCols_; }

private:
    std::unique_ptr<Operator> child_;
    std::vector<std::string> projectCols_;
    std::vector<int> colIndices_;
};

// 5. SortOperator: Ordenamiento multi-columna (ORDER BY col ASC/DESC)
class SortOperator : public Operator {
public:
    struct SortKey {
        std::string colName;
        bool isAscending;
    };

    SortOperator(std::unique_ptr<Operator> child, const std::vector<SortKey>& sortKeys);
    ~SortOperator() override = default;

    void Open() override;
    bool Next(Tuple& outTuple) override;
    void Close() override;
    const std::vector<std::string>& GetOutputColumns() const override { return child_->GetOutputColumns(); }

private:
    std::unique_ptr<Operator> child_;
    std::vector<SortKey> sortKeys_;
    std::vector<Tuple> sortedTuples_;
    size_t currentIndex_ = 0;
};

// 6. HashAggregateOperator: Agrupamiento (GROUP BY) y funciones de agregacion
class HashAggregateOperator : public Operator {
public:
    HashAggregateOperator(std::unique_ptr<Operator> child,
                          const std::vector<std::string>& groupByCols,
                          const std::vector<AggregateExpr>& aggExprs);
    ~HashAggregateOperator() override = default;

    void Open() override;
    bool Next(Tuple& outTuple) override;
    void Close() override;
    const std::vector<std::string>& GetOutputColumns() const override { return outputCols_; }

private:
    std::unique_ptr<Operator> child_;
    std::vector<std::string> groupByCols_;
    std::vector<AggregateExpr> aggExprs_;
    std::vector<std::string> outputCols_;
    std::vector<Tuple> aggregatedTuples_;
    size_t currentIndex_ = 0;
};

// 7. NestedLoopJoinOperator: Evaluacion de INNER JOIN entre dos operadores
class NestedLoopJoinOperator : public Operator {
public:
    NestedLoopJoinOperator(std::unique_ptr<Operator> leftChild,
                           std::unique_ptr<Operator> rightChild,
                           const std::string& leftCol,
                           const std::string& rightCol);
    ~NestedLoopJoinOperator() override = default;

    void Open() override;
    bool Next(Tuple& outTuple) override;
    void Close() override;
    const std::vector<std::string>& GetOutputColumns() const override { return outputCols_; }

private:
    std::unique_ptr<Operator> leftChild_;
    std::unique_ptr<Operator> rightChild_;
    std::string leftCol_;
    std::string rightCol_;
    std::vector<std::string> outputCols_;
    std::vector<Tuple> joinedTuples_;
    size_t currentIndex_ = 0;
};

// 8. LimitOffsetOperator: Paginacion (LIMIT N OFFSET M)
class LimitOffsetOperator : public Operator {
public:
    LimitOffsetOperator(std::unique_ptr<Operator> child, int limit, int offset);
    ~LimitOffsetOperator() override = default;

    void Open() override;
    bool Next(Tuple& outTuple) override;
    void Close() override;
    const std::vector<std::string>& GetOutputColumns() const override { return child_->GetOutputColumns(); }

private:
    std::unique_ptr<Operator> child_;
    int limit_;
    int offset_;
    int returnedCount_ = 0;
};
