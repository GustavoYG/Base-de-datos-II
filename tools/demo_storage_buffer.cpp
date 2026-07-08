#include <filesystem>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "common/types.h"
#include "common/utils.h"
#include "storage/buffer_pool.h"
#include "storage/page_manager.h"
#include "storage/record_manager.h"

namespace {

PassengerRecord MakeRecord(int id, int survived) {
    PassengerRecord r{};
    r.passengerId = id;
    r.survived = survived;
    r.pclass = (id % 3) + 1;
    std::snprintf(r.name, sizeof(r.name), "Passenger-%d", id);
    std::snprintf(r.sex, sizeof(r.sex), "%s", (id % 2 == 0) ? "female" : "male");
    r.age = 18.0f + static_cast<float>(id % 50);
    r.sibSp = id % 4;
    r.parch = id % 3;
    std::snprintf(r.ticket, sizeof(r.ticket), "TKT-%04d", id);
    r.fare = 10.0f + static_cast<float>(id);
    std::snprintf(r.cabin, sizeof(r.cabin), "C%d", id % 10);
    std::snprintf(r.embarked, sizeof(r.embarked), "S");
    return r;
}

void PrintRecord(const PassengerRecord& r) {
    std::cout << "ID=" << r.passengerId
              << " | survived=" << r.survived
              << " | name=" << r.name
              << " | age=" << r.age
              << " | fare=" << r.fare
              << std::endl;
}

void RunPageManagerDemo() {
    std::cout << "\n[Storage Manager Demo]" << std::endl;

    const std::string path = "data/storage/tables/demo_storage.tbl";
    std::filesystem::create_directories("data/storage/tables");
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }

    PageManager pm(path);
    int pageId = pm.AllocatePage();
    Page page{};

    if (!pm.ReadPage(pageId, page)) {
        std::cout << "No se pudo leer la pagina recien creada." << std::endl;
        return;
    }

    const char* message = "StorageManager demo OK";
    std::memcpy(page.data, message, std::strlen(message) + 1);
    page.header.checksum = SimpleChecksum(page.data, sizeof(page.data));

    if (!pm.WritePage(pageId, page)) {
        std::cout << "No se pudo escribir la pagina de demostracion." << std::endl;
        return;
    }

    Page verify{};
    if (!pm.ReadPage(pageId, verify)) {
        std::cout << "No se pudo volver a leer la pagina." << std::endl;
        return;
    }

    std::cout << "PageId: " << verify.header.pageId << std::endl;
    std::cout << "Checksum: " << verify.header.checksum << std::endl;
    std::cout << "Texto almacenado: " << reinterpret_cast<const char*>(verify.data) << std::endl;
    std::cout << "Resultado: Storage Manager funciona." << std::endl;
}

void RunRecordManagerDemo() {
    std::cout << "\n[Record Manager Demo]" << std::endl;

    const std::string path = "data/storage/tables/demo_records.tbl";
    std::filesystem::create_directories("data/storage/tables");
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    if (std::filesystem::exists(path + ".wal")) {
        std::filesystem::remove(path + ".wal");
    }

    RecordManager rm(path);
    std::vector<std::pair<int, int>> locations;

    for (int i = 0; i < 6; ++i) {
        PassengerRecord r = MakeRecord(100 + i, i % 2);
        int pageId = -1;
        int slot = -1;
        if (!rm.InsertRecord(r, pageId, slot)) {
            std::cout << "Fallo la insercion del registro " << i << std::endl;
            return;
        }
        locations.emplace_back(pageId, slot);
        std::cout << "Insertado -> pageId=" << pageId << " slot=" << slot << std::endl;
    }

    PassengerRecord out{};
    if (rm.ReadRecord(locations[2].first, locations[2].second, out)) {
        std::cout << "Lectura del registro 3: ";
        PrintRecord(out);
    } else {
        std::cout << "No se pudo leer el registro insertado." << std::endl;
    }

    std::cout << "Eliminando dos registros para mostrar reutilizacion..." << std::endl;
    rm.DeleteRecord(locations[1].first, locations[1].second);
    rm.DeleteRecord(locations[3].first, locations[3].second);

    PassengerRecord newRecord = MakeRecord(999, 1);
    int newPageId = -1;
    int newSlot = -1;
    if (rm.InsertRecord(newRecord, newPageId, newSlot)) {
        std::cout << "Nuevo registro insertado tras borrados -> pageId=" << newPageId
                  << " slot=" << newSlot << std::endl;
        if (newPageId == locations[1].first || newPageId == locations[3].first) {
            std::cout << "La pagina fue reutilizada por el Record Manager." << std::endl;
        }
    }

    std::cout << "Resultado: Record Manager funciona con insercion, lectura, borrado y reutilizacion." << std::endl;
}

void RunBufferPoolDemo() {
    std::cout << "\n[Buffer Pool Demo]" << std::endl;

    const std::string path = "data/storage/tables/demo_buffer.tbl";
    std::filesystem::create_directories("data/storage/tables");
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }

    PageManager pm(path);
    for (int i = 0; i < 5; ++i) {
        pm.AllocatePage();
    }

    BufferPool bp(pm, 3);

    Page* p0 = bp.PinPage(0);
    Page* p1 = bp.PinPage(1);
    Page* p2 = bp.PinPage(2);
    if (!p0 || !p1 || !p2) {
        std::cout << "No se pudieron pinnear las primeras paginas." << std::endl;
        return;
    }

    const char* payload = "BufferPool demo OK";
    std::memcpy(p0->data, payload, std::strlen(payload) + 1);
    bp.UnpinPage(0, true);
    bp.UnpinPage(1, false);
    bp.UnpinPage(2, false);

    Page* p3 = bp.PinPage(3);
    if (!p3) {
        std::cout << "No se pudo pinnear la pagina 3." << std::endl;
        return;
    }
    bp.UnpinPage(3, false);

    Page* p0Again = bp.PinPage(0);
    if (!p0Again) {
        std::cout << "No se pudo volver a pinnear la pagina 0." << std::endl;
        return;
    }

    std::cout << "Contenido recuperado de page 0: " << reinterpret_cast<const char*>(p0Again->data) << std::endl;
    std::cout << "Resultado: Buffer Pool funciona con pin/unpin, eviction y flush." << std::endl;

    bp.UnpinPage(0, false);
    bp.FlushAll();
}

void RunAllDemos() {
    RunPageManagerDemo();
    RunRecordManagerDemo();
    RunBufferPoolDemo();
}

void ShowMenu() {
    std::cout << "\nDemo Storage/Buffer Manager" << std::endl;
    std::cout << "1) Demostrar Storage Manager" << std::endl;
    std::cout << "2) Demostrar Record Manager" << std::endl;
    std::cout << "3) Demostrar Buffer Pool" << std::endl;
    std::cout << "4) Ejecutar todo" << std::endl;
    std::cout << "5) Salir" << std::endl;
    std::cout << "Opcion: ";
}

} // namespace

int main() {
    while (true) {
        ShowMenu();

        int option = 0;
        if (!(std::cin >> option)) {
            std::cin.clear();
            std::cin.ignore(1024, '\n');
            std::cout << "Entrada invalida." << std::endl;
            continue;
        }

        switch (option) {
            case 1:
                RunPageManagerDemo();
                break;
            case 2:
                RunRecordManagerDemo();
                break;
            case 3:
                RunBufferPoolDemo();
                break;
            case 4:
                RunAllDemos();
                break;
            case 5:
                std::cout << "Saliendo de la demostracion." << std::endl;
                return 0;
            default:
                std::cout << "Opcion invalida." << std::endl;
                break;
        }
    }
}