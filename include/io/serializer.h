#pragma once

#include <string>
#include <vector>

#include "common/types.h"
#include "common/schema.h"

// Convierte una fila de CSV (vector de strings, una por columna) en un buffer de
// bytes de ancho fijo segun el esquema. El resultado es lo que guarda el
// RecordManager.
std::vector<unsigned char> RowToBytes(const Schema& schema, const std::vector<std::string>& row);

// Accesores tipados sobre una fila de bytes. Devuelven 0 / "" si la columna no
// existe. Se usan para proyeccion y para las consultas de la capa interactiva.
int32_t GetFieldInt32(const Schema& schema, const std::vector<unsigned char>& row, const std::string& col);
int64_t GetFieldInt64(const Schema& schema, const std::vector<unsigned char>& row, const std::string& col);
float GetFieldFloat(const Schema& schema, const std::vector<unsigned char>& row, const std::string& col);
double GetFieldDouble(const Schema& schema, const std::vector<unsigned char>& row, const std::string& col);
bool GetFieldBool(const Schema& schema, const std::vector<unsigned char>& row, const std::string& col);
std::string GetFieldString(const Schema& schema, const std::vector<unsigned char>& row, const std::string& col);
