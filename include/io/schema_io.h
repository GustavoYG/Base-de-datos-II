#pragma once

#include <string>

#include "common/schema.h"

// Persistencia del esquema en un archivo de texto legible por lineas:
//   <nombreColumna> <tipo> <longitud>
// Una linea por columna. Permite reabrir la tabla al reiniciar sin re-inferenciar.
namespace schemaio {

// Guarda el esquema en 'path'. Devuelve true si ok.
bool SaveSchema(const std::string& path, const Schema& schema);

// Carga el esquema desde 'path'. Devuelve true si ok (schema queda Finalizado).
bool LoadSchema(const std::string& path, Schema& schema);

// Nombre de archivo de esquema para una tabla (en el mismo dir que los datos).
std::string SchemaPath(const std::string& tablesDir, const std::string& tableName);

} // namespace schemaio
