#include <iostream>
#include <cstdio>
#include <string>
#include <vector>

#include "common/schema.h"
#include "io/csv_reader.h"
#include "io/serializer.h"
#include "storage/table.h"
#include "index/bplus_tree.h"

static ReplacementPolicy ParsePolicy(int argc, char** argv) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--policy") {
            std::string p = argv[i + 1];
            if (p == "clock") return ReplacementPolicy::CLOCK;
            if (p == "lru") return ReplacementPolicy::LRU;
            std::cerr << "Politica desconocida '" << p << "', usando LRU.\n";
            return ReplacementPolicy::LRU;
        }
    }
    return ReplacementPolicy::LRU;
}

// Esquema de la tabla (antes era el struct PassengerRecord hardcodeado). Ahora
// es datos: cualquier CSV se describe asi y el storage opera sobre bytes.
static Schema BuildTitanicSchema() {
    Schema s;
    s.AddColumn("passengerId", ColumnType::INT32);
    s.AddColumn("survived", ColumnType::INT32);
    s.AddColumn("pclass", ColumnType::INT32);
    s.AddColumn("name", ColumnType::STRING, 64);
    s.AddColumn("sex", ColumnType::STRING, 8);
    s.AddColumn("age", ColumnType::FLOAT);
    s.AddColumn("sibSp", ColumnType::INT32);
    s.AddColumn("parch", ColumnType::INT32);
    s.AddColumn("ticket", ColumnType::STRING, 32);
    s.AddColumn("fare", ColumnType::FLOAT);
    s.AddColumn("cabin", ColumnType::STRING, 16);
    s.AddColumn("embarked", ColumnType::STRING, 4);
    s.Finalize();
    return s;
}

int main(int argc, char** argv) {
    ReplacementPolicy policy = ParsePolicy(argc, argv);
    const Schema schema = BuildTitanicSchema();

    std::vector<std::vector<std::string>> rows = CsvReader::ReadAll("data/raw/titanic.csv");
    if (rows.empty()) {
        rows = CsvReader::ReadAll("titanic.csv");
    }
    if (rows.empty()) {
        std::cout << "CSV no encontrado." << std::endl;
        return 1;
    }

    // Almacenamiento: tabla en heap file + indice primario B+ Tree sobre
    // passengerId. Se limpian los archivos previos para no acumular inserciones.
    const std::string baseDir = "data/storage/tables";
    const std::string binPath = baseDir + "/titanic.bin";
    const std::string idxPath = baseDir + "/titanic_idx.bin";
    std::remove(binPath.c_str());
    std::remove((binPath + ".wal").c_str());
    std::remove(idxPath.c_str());
    std::remove((idxPath + ".meta").c_str());

    Table table("titanic", schema, baseDir, policy);
    BPlusTree idx(idxPath, ColumnType::INT32, policy); // indice sobre clave entera

    // Cache en memoria para seleccion por atributos no indexados (edad/survived).
    // En la fase de operadores relacionales esto se reemplaza por scans sobre el storage.
    std::vector<std::vector<unsigned char>> rowsCache;
    rowsCache.reserve(rows.size());

    for (size_t i = 1; i < rows.size(); ++i) { // salta encabezado
        if (rows[i].size() < 12) continue;
        std::vector<unsigned char> bytes = RowToBytes(schema, rows[i]);

        int pid = 0, slot = 0;
        if (!table.InsertRow(bytes, pid, slot)) continue;

        int32_t id = GetFieldInt32(schema, bytes, "passengerId");
        BTreeKey key;
        BTreeKeyFromInt32(key, id);
        idx.Insert(key, pid, (int16_t)slot);

        rowsCache.push_back(std::move(bytes));
    }

    auto PrintRow = [&](const std::vector<unsigned char>& r) {
        std::cout << "ID " << GetFieldInt32(schema, r, "passengerId")
                  << " | " << GetFieldString(schema, r, "name")
                  << " | edad " << GetFieldFloat(schema, r, "age")
                  << " | survived " << GetFieldInt32(schema, r, "survived") << "\n";
    };

    const char* policyName = (policy == ReplacementPolicy::CLOCK) ? "CLOCK" : "LRU";

    while (true) {
        std::cout << "\nMenu (politica de reemplazo: " << policyName << "):\n";
        std::cout << "1) Buscar PassengerId (B+ Tree)\n";
        std::cout << "2) Buscar rango de edades\n";
        std::cout << "3) Listar sobrevivientes\n";
        std::cout << "4) Rango de PassengerIds (B+ Tree Range)\n";
        std::cout << "5) Salir\n";
        std::cout << "Opcion: ";

        int op = 0;
        std::cin >> op;

        if (op == 1) {
            int id = 0;
            std::cout << "PassengerId: ";
            std::cin >> id;

            BTreeKey key;
            BTreeKeyFromInt32(key, id);
            int pid = 0;
            int16_t slot = 0;
            if (!idx.Find(key, pid, slot)) {
                std::cout << "No encontrado." << std::endl;
                continue;
            }

            std::vector<unsigned char> r;
            if (!table.ReadRow(pid, slot, r)) {
                std::cout << "Error de lectura en pagina " << pid << " slot " << slot << "." << std::endl;
                continue;
            }
            PrintRow(r);
        } else if (op == 2) {
            float minAge = 0.0f, maxAge = 0.0f;
            std::cout << "Edad minima: ";
            std::cin >> minAge;
            std::cout << "Edad maxima: ";
            std::cin >> maxAge;

            int count = 0;
            for (const auto& r : rowsCache) {
                float age = GetFieldFloat(schema, r, "age");
                if (age >= minAge && age <= maxAge) {
                    PrintRow(r);
                    count++;
                }
            }
            std::cout << "Total: " << count << std::endl;
        } else if (op == 3) {
            int count = 0;
            for (const auto& r : rowsCache) {
                if (GetFieldInt32(schema, r, "survived") == 1) {
                    PrintRow(r);
                    count++;
                }
            }
            std::cout << "Total: " << count << std::endl;
        } else if (op == 4) {
            int minId = 0, maxId = 0;
            std::cout << "PassengerId minimo: ";
            std::cin >> minId;
            std::cout << "PassengerId maximo: ";
            std::cin >> maxId;

            BTreeKey minKey, maxKey;
            BTreeKeyFromInt32(minKey, minId);
            BTreeKeyFromInt32(maxKey, maxId);

            std::vector<IndexEntry> res;
            idx.Range(minKey, maxKey, res);
            int count = 0;
            for (const auto& e : res) {
                std::vector<unsigned char> r;
                if (table.ReadRow(e.pageId, (int16_t)e.slot, r)) {
                    PrintRow(r);
                    count++;
                }
            }
            std::cout << "Total: " << count << std::endl;
        } else if (op == 5) {
            break;
        } else {
            std::cout << "Opcion invalida." << std::endl;
        }
    }

    return 0;
}
