#include <iostream>
#include <string>
#include <cctype>

#include "common/utils.h"
#include "db/catalog.h"
#include "sql/sql_engine.h"

// Recorta espacios al inicio/final.
static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Modulo 1: cargar archivos CSV como tablas en bucle hasta QUIT.
static void LoadCsvModule(Catalog& catalog) {
    while (true) {
        std::cout << "\n[MODULO CARGAR CSV]\n";
        std::cout << "Escriba la ruta del archivo (o 'QUIT' para regresar): ";
        std::string path;
        if (!std::getline(std::cin, path)) return;
        path = Trim(path);
        if (path.empty()) continue;
        if (ToLower(path) == "quit") break;

        if (catalog.LoadCsv(path)) {
            std::string name = StoredTable::TableNameFromPath(path);
            const StoredTable* t = catalog.Get(name);
            std::cout << "-> Archivo cargado correctamente.\n";
            std::cout << "-> Tabla creada: \"" << name << "\"\n";
            std::cout << "-> Esquema detectado (" << t->schema.columns.size() << " columnas): [";
            for (size_t i = 0; i < t->schema.columns.size(); ++i) {
                std::cout << (i ? ", " : "") << t->schema.columns[i].name;
            }
            std::cout << "]\n";
        } else {
            std::cout << "Error: no se pudo cargar '" << path
                      << "'. Verifique la ruta (se busca en ./, data/ y data/raw/).\n";
        }
    }
}

// Modulo 3: gestionar tablas (CREATE/DROP) interactivo.
static void RunTableManager(Catalog& catalog) {
    std::cout << "\n[GESTIONAR TABLAS]\n";
    std::cout << "Comandos disponibles:\n";
    std::cout << "  CREATE TABLE nombre (col1, col2, ...)\n";
    std::cout << "  DROP TABLE nombre\n";
    std::cout << "  LIST  (ver tablas existentes)\n";
    std::cout << "  QUIT  (regresar al menu principal)\n";

    while (true) {
        std::cout << "\nTABLAS> ";
        std::string line;
        if (!std::getline(std::cin, line)) break;
        line = Trim(line);
        if (line.empty()) continue;
        if (ToLower(line) == "quit") break;

        std::string low = ToLower(line);

        if (low == "list") {
            std::cout << "Tablas existentes:\n";
            catalog.PrintLoadedTables();
            continue;
        }

        // CREATE TABLE nombre (col1, col2, ...)
        if (low.substr(0, 6) == "create") {
            std::string result = ExecuteModifyQuery(catalog, line);
            std::cout << result << "\n";
            continue;
        }

        // DROP TABLE nombre
        if (low.substr(0, 4) == "drop") {
            std::string result = ExecuteModifyQuery(catalog, line);
            std::cout << result << "\n";
            continue;
        }

        std::cout << "Comando no reconocido. Use CREATE TABLE, DROP TABLE, LIST o QUIT.\n";
    }
}

int main() {
    Catalog catalog;

    if (catalog.TableCount() > 0) {
        std::cout << "\nTablas persistidas disponibles:\n";
        catalog.PrintLoadedTables();
        std::cout << "Puede ir directo al SQL CLI (opcion 2) sin recargar el CSV.\n";
    }

    while (true) {
        std::cout << "\nSGDB\n";
        std::cout << "1. Cargar archivo CSV\n";
        std::cout << "2. SQL CLI\n";
        std::cout << "3. Gestionar tablas\n";
        std::cout << "4. Salir\n";
        std::cout << "Seleccione una opcion: ";

        std::string line;
        if (!std::getline(std::cin, line)) break;
        int op = std::atoi(Trim(line).c_str());

        if (op == 1) {
            LoadCsvModule(catalog);
        } else if (op == 2) {
            RunSqlCli(catalog);
        } else if (op == 3) {
            RunTableManager(catalog);
        } else if (op == 4) {
            break;
        } else {
            std::cout << "Opcion invalida.\n";
        }
    }

    return 0;
}
