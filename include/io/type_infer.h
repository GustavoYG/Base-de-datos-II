#pragma once

#include <string>
#include <vector>

#include "common/schema.h"

// Analisis de tipos para carga dinamica de CSV (sin depender de Titanic).
namespace typeinfer {

// Comprueba si un valor es un entero (opcionalmente con signo).
bool IsInt(const std::string& s);
bool IsFloat(const std::string& s);
bool IsBool(const std::string& s);
// Fechas: YYYY-MM-DD  o  YYYY-MM-DD HH:MM:SS  (tambien acepta '/' y '.').
bool IsDate(const std::string& s);
bool IsDateTime(const std::string& s);

// Infiere el ColumnType de un valor aislado (usado por fila de muestra).
ColumnType InferValue(const std::string& s);

// Une dos inferencias de columna: si alguna vez es "mas rico" que el otro,
// se queda con el mas general. Orden de generalidad (menor -> mayor):
//   BOOL < INT32 < INT64 < FLOAT < DOUBLE < CHAR < VARCHAR/DATE/DATETIME/TEXT
ColumnType Combine(ColumnType a, ColumnType b);

// Dado el encabezado y las primeras 'sampleRows' filas, deduce el tipo de cada
// columna. Devuelve un vector con el tipo inferido por columna.
std::vector<ColumnType> InferColumnTypes(
    const std::vector<std::string>& header,
    const std::vector<std::vector<std::string>>& sampleRows);

// Mide el ancho maximo real (en bytes) de cada columna de TEXTO recorriendo el
// archivo completo en modo streaming (sin cargarlo en RAM). 'skipHeader' indica
// si la primera linea es encabezado. 'numColumns' es el nro de columnas esperado
// (del encabezado). Las columnas no-texto se ignoran (ancho 0).
// Devuelve vector con el ancho maximo por columna (solo tiene sentido para
// columnas de tipo STRING; para las numericas se deja en 0).
std::vector<int> MeasureMaxWidths(
    const std::string& csvPath,
    size_t numColumns,
    bool skipHeader,
    const std::vector<ColumnType>& types);

} // namespace typeinfer
