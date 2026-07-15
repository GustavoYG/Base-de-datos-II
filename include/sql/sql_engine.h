#pragma once

#include <string>

class Catalog;

// Ejecuta el bucle del SQL CLI (prompt "SQL>"). Termina cuando el usuario
// escribe QUIT y regresa al menu principal.
void RunSqlCli(Catalog& catalog);

// Ejecuta una unica consulta y la imprime. Util para pruebas.
void ExecuteAndPrintQuery(Catalog& catalog, const std::string& query);
