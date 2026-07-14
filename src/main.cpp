#include <iostream>
#include <cstdio>
#include <string>
#include <vector>

#include "io/csv_reader.h"
#include "io/serializer.h"
#include "storage/record_manager.h"
#include "index/simple_index.h"

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

int main(int argc, char** argv) {
    ReplacementPolicy policy = ParsePolicy(argc, argv);

    std::vector<std::vector<std::string>> rows = CsvReader::ReadAll("data/raw/titanic.csv");
    if (rows.empty()) {
        rows = CsvReader::ReadAll("titanic.csv");
    }
    if (rows.empty()) {
        std::cout << "CSV no encontrado." << std::endl;
        return 1;
    }

    std::vector<PassengerRecord> csvRecords = RowsToRecords(rows);
    if (csvRecords.empty()) {
        std::cout << "No hay registros validos." << std::endl;
        return 1;
    }

    // Usar el Storage Manager real (persistencia binaria + Buffer Pool con
    // la politica de reemplazo elegida). Se limpia el archivo previo para
    // no acumular inserciones en cada ejecucion.
    const std::string binPath = "data/storage/tables/titanic.bin";
    std::remove(binPath.c_str());
    std::remove((binPath + ".wal").c_str());

    RecordManager rm(binPath, policy);
    SimpleIndex idx;
    std::vector<PassengerRecord> records; // cache para escaneos por rango
    records.reserve(csvRecords.size());
    for (const auto& r : csvRecords) {
        int pid = 0, slot = 0;
        rm.InsertRecord(r, pid, slot);
        idx.Add(r.passengerId, pid, slot);
        records.push_back(r);
    }
    idx.Build();

    const char* policyName = (policy == ReplacementPolicy::CLOCK) ? "CLOCK" : "LRU";

    while (true) {
        std::cout << "\nMenu (politica de reemplazo: " << policyName << "):\n";
        std::cout << "1) Buscar PassengerId\n";
        std::cout << "2) Buscar rango de edades\n";
        std::cout << "3) Listar sobrevivientes\n";
        std::cout << "4) Salir\n";
        std::cout << "Opcion: ";

        int op = 0;
        std::cin >> op;

        if (op == 1) {
            int id = 0;
            std::cout << "PassengerId: ";
            std::cin >> id;

            IndexEntry e;
            if (!idx.Find(id, e)) {
                std::cout << "No encontrado." << std::endl;
                continue;
            }

            PassengerRecord r;
            if (!rm.ReadRecord(e.pageId, e.slot, r)) {
                std::cout << "Error de lectura en pagina " << e.pageId << " slot " << e.slot << "." << std::endl;
                continue;
            }
            std::cout << "ID " << r.passengerId << " | " << r.name
                      << " | edad " << r.age << " | survived " << r.survived << std::endl;
        } else if (op == 2) {
            float minAge = 0.0f;
            float maxAge = 0.0f;
            std::cout << "Edad minima: ";
            std::cin >> minAge;
            std::cout << "Edad maxima: ";
            std::cin >> maxAge;

            int count = 0;
            for (size_t i = 0; i < records.size(); ++i) {
                const PassengerRecord& r = records[i];
                if (r.age >= minAge && r.age <= maxAge) {
                    std::cout << "ID " << r.passengerId << " | " << r.name
                              << " | edad " << r.age << "\n";
                    count++;
                }
            }
            std::cout << "Total: " << count << std::endl;
        } else if (op == 3) {
            int count = 0;
            for (size_t i = 0; i < records.size(); ++i) {
                const PassengerRecord& r = records[i];
                if (r.survived == 1) {
                    std::cout << "ID " << r.passengerId << " | " << r.name
                              << " | edad " << r.age << "\n";
                    count++;
                }
            }
            std::cout << "Total: " << count << std::endl;
        } else if (op == 4) {
            break;
        } else {
            std::cout << "Opcion invalida." << std::endl;
        }
    }

    return 0;
}
