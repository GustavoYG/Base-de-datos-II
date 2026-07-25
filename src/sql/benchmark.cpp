#include "sql/benchmark.h"

#include <iostream>
#include <vector>
#include <iomanip>
#include <filesystem>

#include "db/catalog.h"
#include "index/index_manager.h"
#include "sql/operators.h"
#include "storage/record_manager.h"
#include "common/utils.h"

namespace fs = std::filesystem;

namespace benchmark {

void RunIndexBenchmark(Catalog& catalog, int recordCount) {
    std::cout << "\n=======================================================\n";
    std::cout << "      BENCHMARK DE RENDIMIENTO: B+ TREE INDEX VS FULL SCAN\n";
    std::cout << "=======================================================\n";
    std::cout << "Generando tabla de prueba con " << recordCount << " registros...\n";

    std::string tableName = "bench_table";

    // Si ya existe la tabla de benchmark, eliminarla para asegurar frescura de datos
    if (catalog.Exists(tableName)) {
        catalog.DropTable(tableName);
    }

    // Crear esquema: id (INT32), age (INT32), name (VARCHAR), salary (FLOAT)
    std::vector<std::string> cols = {"id", "age", "name", "salary"};
    if (!catalog.CreateTable(tableName, cols)) {
        std::cout << "Error creando tabla de benchmark.\n";
        return;
    }

    const StoredTable* t = catalog.Get(tableName);
    if (!t) return;

    // Poblar 10,000 registros directamente via RecordManager
    RecordManager rm(t->binPath, t->schema, ReplacementPolicy::LRU);
    std::vector<unsigned char> row(t->schema.rowSize);

    int targetId = recordCount * 3 / 4; // Ej. 7500 si recordCount = 10000

    Timer genTimer;
    genTimer.Start();

    for (int i = 1; i <= recordCount; ++i) {
        std::fill(row.begin(), row.end(), 0);

        int32_t idVal = i;
        int32_t ageVal = 20 + (i % 50);
        std::string nameVal = "User_" + std::to_string(i);
        float salVal = 1000.0f + (float)(i % 500);

        int pId, slot;
        std::memcpy(row.data() + t->schema.columns[0].offset, &idVal, sizeof(idVal));
        std::memcpy(row.data() + t->schema.columns[1].offset, &ageVal, sizeof(ageVal));
        size_t nLen = std::min(nameVal.size(), (size_t)t->schema.columns[2].length - 1);
        std::memcpy(row.data() + t->schema.columns[2].offset, nameVal.c_str(), nLen);
        std::memcpy(row.data() + t->schema.columns[3].offset, &salVal, sizeof(salVal));

        rm.InsertRecord(row, pId, slot);
    }

    std::cout << "-> Tabla '" << tableName << "' creada en " << std::fixed << std::setprecision(2)
              << genTimer.ElapsedMs() << " ms.\n\n";

    std::string targetIdStr = std::to_string(targetId);

    // 1. EJECUCION CON FULL SCAN (SIN INDICE)
    std::cout << "1. Ejecutando consulta (FULL SCAN) para id = " << targetIdStr << "...\n";
    Timer fullScanTimer;
    fullScanTimer.Start();

    ScanOperator scanOp(*t);
    SelectOperator selectOp(std::make_unique<ScanOperator>(*t), "id", CmpOperator::EQ, targetIdStr);

    selectOp.Open();
    Tuple resTuple;
    int fullScanCount = 0;
    while (selectOp.Next(resTuple)) {
        fullScanCount++;
    }
    selectOp.Close();

    double fullScanMs = fullScanTimer.ElapsedMs();
    std::cout << "   - Filas encontradas: " << fullScanCount << "\n";
    std::cout << "   - Tiempo Full Scan: " << std::setprecision(4) << fullScanMs << " ms\n\n";

    // 2. CONSTRUCCION DEL INDICE B+ TREE
    std::cout << "2. Construyendo indice B+ Tree sobre columna 'id'...\n";
    Timer indexBuildTimer;
    indexBuildTimer.Start();

    IndexManager::BuildIndex(catalog, tableName, "id");
    double buildMs = indexBuildTimer.ElapsedMs();
    std::cout << "   - Indice construido en: " << buildMs << " ms\n\n";

    // 3. EJECUCION CON BUSQUEDA POR INDICE B+ TREE
    std::cout << "3. Ejecutando consulta (B+ TREE INDEX SCAN) para id = " << targetIdStr << "...\n";
    Timer indexScanTimer;
    indexScanTimer.Start();

    IndexScanOperator indexOp(*t, "id", targetIdStr);
    indexOp.Open();
    int indexScanCount = 0;
    while (indexOp.Next(resTuple)) {
        indexScanCount++;
    }
    indexOp.Close();

    double indexScanMs = indexScanTimer.ElapsedMs();
    std::cout << "   - Filas encontradas: " << indexScanCount << "\n";
    std::cout << "   - Tiempo Index Scan: " << std::setprecision(4) << indexScanMs << " ms\n\n";

    // 4. RESUMEN DE COMPARACION DE RENDIMIENTO
    double speedup = (indexScanMs > 0.0001) ? (fullScanMs / indexScanMs) : (fullScanMs / 0.0001);

    std::cout << "=======================================================\n";
    std::cout << "              RESULTADOS COMPARATIVOS                  \n";
    std::cout << "=======================================================\n";
    std::cout << " Metodo           | Registros | Tiempo (ms) | Speedup   \n";
    std::cout << "------------------+-----------+-------------+-----------\n";
    std::cout << " Full Table Scan  | " << std::setw(9) << recordCount << " | "
              << std::setw(11) << std::setprecision(4) << fullScanMs << " | 1.0x (Base)\n";
    std::cout << " B+ Tree Index    | " << std::setw(9) << recordCount << " | "
              << std::setw(11) << std::setprecision(4) << indexScanMs << " | "
              << std::setprecision(1) << speedup << "x MAS RAPIDO!\n";
    std::cout << "=======================================================\n\n";
}

} // namespace benchmark
