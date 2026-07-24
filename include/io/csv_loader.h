#pragma once

#include <string>
#include <vector>

#include "common/schema.h"

// Carga dinamica de cualquier CSV al almacenamiento paginado (heap file) en
// modo STREAMING: no carga el archivo completo en RAM (adecuado para 10 GB en
// una VM de 1 GB). Flujo:
//   1. Lee encabezado + ~10 lineas de muestra -> infiere tipos.
//   2. Mide el ancho maximo real de columnas de texto (1 pasada al archivo).
//   3. Construye el Schema (ancho fijo) y lo persiste en <tabla>.schema.
//   4. Vuelca cada fila al RecordManager/Table paginado (a disco), linea a linea.
namespace csvloader {

struct CsvLoadResult {
    bool ok = false;
    std::string tableName;
    Schema schema;
    long long rowCount = 0;
};

// Carga 'csvPath' como tabla 'tableName' en el directorio 'tablesDir'.
// 'policy' se usa para el buffer pool. Sobrescribe si la tabla ya existe.
CsvLoadResult LoadCsvToStorage(const std::string& csvPath,
                               const std::string& tableName,
                               const std::string& tablesDir,
                               int sampleRows = 10);

// Numero de filas estimado (para mensajes). No cuenta el encabezado.
long long CountDataRows(const std::string& csvPath);

} // namespace csvloader
