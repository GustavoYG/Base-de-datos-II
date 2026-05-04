#include <iostream>

#include "io/csv_reader.h"
#include "io/serializer.h"
#include "storage/atomic_writer.h"
#include "index/simple_index.h"
#include "storage/wal_log.h"

int main() {
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

    const std::string txtPath = "data/storage/tables/titanic.txt";
    const std::string walPath = "data/storage/wal/titanic.wal";

    std::string txt = SerializeRecordsText(csvRecords);
    if (!AppendWalEntry(walPath, txt)) {
        std::cout << "No se pudo escribir el WAL." << std::endl;
        return 1;
    }

    std::vector<unsigned char> bytes(txt.begin(), txt.end());
    if (!WriteAtomic(txtPath, bytes)) {
        std::cout << "No se pudo guardar el archivo TXT." << std::endl;
        return 1;
    }

    std::vector<PassengerRecord> records = LoadRecordsText(txtPath);
    if (records.empty()) {
        std::string recovered = ReadLastValidWalPayload(walPath);
        if (!recovered.empty()) {
            std::vector<unsigned char> recBytes(recovered.begin(), recovered.end());
            if (WriteAtomic(txtPath, recBytes)) {
                records = LoadRecordsText(txtPath);
            }
        }
    }

    if (records.empty()) {
        std::cout << "No se pudieron cargar registros desde TXT." << std::endl;
        return 1;
    }

    SimpleIndex idx;
    for (size_t i = 0; i < records.size(); ++i) {
        idx.Add(records[i].passengerId, 0, (int)i);
    }
    idx.Build();

    while (true) {
        std::cout << "\nMenu:\n";
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

            const PassengerRecord& r = records[(size_t)e.slot];
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
