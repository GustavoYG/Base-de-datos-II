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
        std::cout << "3. Salir\n";
        std::cout << "Seleccione una opcion: ";

        std::string line;
        if (!std::getline(std::cin, line)) break;
        int op = std::atoi(Trim(line).c_str());

        if (op == 1) {
            LoadCsvModule(catalog);
        } else if (op == 2) {
            RunSqlCli(catalog);
        } else if (op == 3) {
            break;
        } else {
            std::cout << "Opcion invalida.\n";
        }
    }

    return 0;
}
