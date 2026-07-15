#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Tipos de columna soportados por el esquema. El almacenamiento usa registros
// de ANCHO FIJO: cada columna ocupa un numero de bytes conocido en tiempo de
// diseno del esquema, de modo que el slot directory y la gestion de espacio
// libre del RecordManager siguen funcionando sin cambios.
enum class ColumnType : int16_t {
    INT32 = 0,
    FLOAT = 1,
    STRING = 2, // largo fijo: usa ColumnDef::length
    BOOL = 3    // se guarda como int32 (0/1) por simplicidad
};

struct ColumnDef {
    std::string name;
    ColumnType type;
    int32_t length = 0; // STRING: largo en bytes; otros: se ignora
    int32_t offset = 0; // byte dentro de la fila de ancho fijo (lo pone Finalize)
};

// Descripcion de una tabla. Es la pieza central que desacopla el storage del
// esquema de Titanic: cualquier CSV/table se describe con un Schema y el resto
// de las capas opera sobre bytes + Schema.
class Schema {
public:
    std::vector<ColumnDef> columns;
    int32_t rowSize = 0; // ancho fijo de una fila en bytes (lo pone Finalize)

    void AddColumn(const std::string& name, ColumnType type, int32_t length = 0);
    // Calcula offsets (empaquetado seguido, sin padding) y rowSize.
    void Finalize();
    int GetColumnIndex(const std::string& name) const; // -1 si no existe
    bool HasColumn(const std::string& name) const;
};
