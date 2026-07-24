#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

// Tabla cargada en memoria para la fase interactiva. El spec indica que en esta
// etapa todo se maneja como texto/strings, sin tipos estrictos, asi que las
// filas son vectores de strings. El almacenamiento en paginas (Table /
// RecordManager / B+ Tree) queda como base para la fase de persistencia e
// indexado que viene despues.
struct LoadedTable {
    std::string name;
    std::vector<std::string> columns;                // nombres de la cabecera
    std::vector<std::vector<std::string>> rows;      // datos (sin la cabecera)

    // Indice de columna por nombre (insensible a mayusculas/minusculas). -1 si no existe.
    int GetColumnIndex(const std::string& col) const;

    // Nombre de tabla sugerido a partir de una ruta (basename sin .csv).
    static std::string TableNameFromPath(const std::string& path);
};

// Catalogo de tablas cargadas (en memoria) accesible desde el SQL CLI.
// AL INICIAR se auto-cargan las tablas persistidas en disco, de modo que una
// tabla cargada en una ejecucion anterior sigue disponible en el SQL CLI sin
// tener que volver a cargar el CSV.
class Catalog {
public:
    // Al construirse, carga todas las tablas persistidas en
    // kTablesDir (si existen).
    Catalog();

    // Carga un CSV como tabla. 'path' puede ser relativo; se prueban varias
    // ubicaciones (./, data/, data/raw/). Devuelve true si tuvo exito. La tabla
    // queda persistida en disco automaticamente.
    bool LoadCsv(const std::string& path);

    bool Exists(const std::string& name) const;
    LoadedTable* Get(const std::string& name);
    const LoadedTable* Get(const std::string& name) const;
    size_t TableCount() const { return tables_.size(); }

    // Crea una tabla vacia con las columnas especificadas.
    bool CreateTable(const std::string& name, const std::vector<std::string>& columns);

    // Elimina una tabla del catalogo y su archivo persistido.
    bool DropTable(const std::string& name);

    void PrintLoadedTables() const;

    // --- Persistencia ---
    // Guarda una tabla en disco dentro de kTablesDir/<nombre>.tbl.
    bool SaveTable(const LoadedTable& t);
    // Carga una tabla desde un archivo .tbl concreto. Devuelve true si ok.
    bool LoadTableFile(const std::string& file);
    // Guarda todas las tablas en memoria (por si se desea forzar un volcado).
    void SaveAllTables();
    // Carga todas las *.tbl del directorio de persistencia.
    void LoadAllTables();

    // Directorio donde se persisten las tablas del catalogo interactivo. Es
    // independiente del directorio de trabajo del motor de storage (que usa
    // archivos .bin/.wal de paginas) para no cargar archivos de prueba como
    // tablas fantasma.
    static const std::string kTablesDir;

private:
    std::map<std::string, LoadedTable> tables_;
};
