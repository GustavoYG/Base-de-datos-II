#pragma once

#include <string>

class Catalog;

// Ejecuta el bucle del SQL CLI (prompt "SQL>"). Termina cuando el usuario
// escribe QUIT y regresa al menu principal.
void RunSqlCli(Catalog& catalog);

// Ejecuta una unica consulta SELECT y la imprime.
void ExecuteAndPrintQuery(Catalog& catalog, const std::string& query);

// Ejecuta una consulta de modificacion (INSERT/DELETE/UPDATE) y retorna un
// mensaje de resultado (numero de filas afectadas, etc.).
std::string ExecuteModifyQuery(Catalog& catalog, const std::string& query);
