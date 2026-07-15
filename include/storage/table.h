#pragma once

#include <string>
#include <vector>

#include "common/types.h"
#include "common/schema.h"
#include "storage/record_manager.h"

// Abstraccion de tabla: une un Schema (descripcion de columnas) con un
// RecordManager (almacenamiento en heap file). Es el objeto natural sobre el que
// la capa interactiva construira tablas via CREATE TABLE, y al que despues se
// le podran adjuntar indices (B+ Tree) sobre columnas concretas.
class Table {
public:
    // Construye la tabla. El heap file queda en 'baseDir/name.bin'.
    Table(const std::string& name, const Schema& schema, const std::string& baseDir, ReplacementPolicy policy = ReplacementPolicy::LRU);

    bool InsertRow(const std::vector<unsigned char>& row, int& outPageId, int& outSlot);
    bool ReadRow(int pageId, int slot, std::vector<unsigned char>& outRow);
    bool UpdateRow(int pageId, int slot, const std::vector<unsigned char>& row);
    bool DeleteRow(int pageId, int slot);

    const Schema& GetSchema() const { return schema_; }
    const std::string& GetName() const { return name_; }

private:
    std::string name_;
    Schema schema_;
    RecordManager rm_;
};
