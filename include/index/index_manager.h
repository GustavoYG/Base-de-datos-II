#pragma once

#include <string>
#include "db/catalog.h"

class IndexManager {
public:
    // Retorna la ruta estandar del archivo de indice (.idx) para una tabla y columna
    static std::string GetIndexPath(const std::string& tableName, const std::string& colName);

    // Comprueba si existe un indice B+ Tree para la columna especificada de la tabla
    static bool HasIndex(const std::string& tableName, const std::string& colName);

    // Construye un indice B+ Tree escaneando todas las filas de la tabla existente
    static bool BuildIndex(Catalog& catalog, const std::string& tableName, const std::string& colName);
};
