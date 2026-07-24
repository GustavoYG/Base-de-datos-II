#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

#include "common/schema.h"

// Tabla persistida en el almacenamiento paginado (heap file). Ya NO se guarda
// en memoria: el esquema vive en <tabla>.schema y los datos en <tabla>.bin
// (paginas de 4 KB gestionadas por el RecordManager/Table). El SQL CLI recorre
// las filas directamente desde disco, de modo que una tabla de 10 GB se consulta
// en una VM de 1 GB sin cargarla toda en RAM.
struct StoredTable {
    std::string name;
    Schema schema;                          // columnas + tipos + anchos (ancho fijo)
    std::string binPath;                    // heap file en disco

    // Indice de columna por nombre (insensible a mayusculas/minusculas). -1 si no existe.
    int GetColumnIndex(const std::string& col) const;

    // Nombre de tabla sugerido a partir de una ruta (basename sin .csv).
    static std::string TableNameFromPath(const std::string& path);
};

// Catalogo de tablas disponibles para el SQL CLI. AL INICIAR se auto-registran
// las tablas persistidas en kTablesDir (lee sus .schema), de modo que una tabla
// cargada en una ejecucion anterior sigue disponible sin volver a cargar el CSV.
class Catalog {
public:
    // Al construirse, registra todas las tablas persistidas en kTablesDir.
    Catalog();

    // Carga un CSV como tabla (carga dinamica: infiere tipos, mide anchos y
    // vuelca al heap file en streaming). 'path' puede ser relativo; se prueban
    // varias ubicaciones. Devuelve true si tuvo exito.
    bool LoadCsv(const std::string& path);

    bool Exists(const std::string& name) const;
    StoredTable* Get(const std::string& name);
    const StoredTable* Get(const std::string& name) const;
    size_t TableCount() const { return tables_.size(); }

    // Crea una tabla vacia con las columnas especificadas.
    bool CreateTable(const std::string& name, const std::vector<std::string>& columns);

    // Elimina una tabla del catalogo y su archivo persistido.
    bool DropTable(const std::string& name);

    void PrintLoadedTables() const;

    // Directorio donde se persisten las tablas (heap files .bin + .schema).
    static const std::string kTablesDir;

private:
    // Registra una tabla ya persistida leyendo su .schema.
    bool RegisterFromSchema(const std::string& tableName, const std::string& schemaPath);

    std::map<std::string, StoredTable> tables_;
};
