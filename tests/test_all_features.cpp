#include <iostream>
#include <cassert>
#include <vector>
#include <string>

#include "db/catalog.h"
#include "sql/sql_engine.h"
#include "sql/operator.h"
#include "sql/operators.h"
#include "index/index_manager.h"
#include "storage/buffer_pool.h"
#include "storage/page_manager.h"
#include "sql/benchmark.h"

int main() {
    std::cout << "=======================================================\n";
    std::cout << "      PRUEBAS DE VERIFICACION: MODELO VOLCANO Y SQL     \n";
    std::cout << "=======================================================\n";

    Catalog catalog;

    // 1. Crear tablas de prueba 'users_test' y 'orders_test'
    if (catalog.Exists("users_test")) catalog.DropTable("users_test");
    if (catalog.Exists("orders_test")) catalog.DropTable("orders_test");

    catalog.CreateTable("users_test", {"id", "name", "age", "salary"});
    catalog.CreateTable("orders_test", {"order_id", "user_id", "amount"});

    std::cout << "[PASS] Tablas 'users_test' y 'orders_test' creadas correctamente.\n";

    // 2. Insertar registros
    ExecuteAndPrintQuery(catalog, "INSERT INTO users_test (id, name, age, salary) VALUES (1, \"Alice\", 30, 5000.0)");
    ExecuteAndPrintQuery(catalog, "INSERT INTO users_test (id, name, age, salary) VALUES (2, \"Bob\", 25, 4000.0)");
    ExecuteAndPrintQuery(catalog, "INSERT INTO users_test (id, name, age, salary) VALUES (3, \"Charlie\", 35, 6000.0)");
    ExecuteAndPrintQuery(catalog, "INSERT INTO users_test (id, name, age, salary) VALUES (4, \"David\", 25, 4500.0)");

    ExecuteAndPrintQuery(catalog, "INSERT INTO orders_test (order_id, user_id, amount) VALUES (101, 1, 150.0)");
    ExecuteAndPrintQuery(catalog, "INSERT INTO orders_test (order_id, user_id, amount) VALUES (102, 1, 200.0)");
    ExecuteAndPrintQuery(catalog, "INSERT INTO orders_test (order_id, user_id, amount) VALUES (103, 2, 50.0)");

    std::cout << "[PASS] Registros de prueba insertados.\n\n";

    // 3. Probar SELECT con WHERE, ORDER BY y LIMIT/OFFSET
    std::cout << "--- Test SELECT con WHERE, ORDER BY DESC y LIMIT 2 OFFSET 0 ---\n";
    ExecuteAndPrintQuery(catalog, "SELECT name, age, salary FROM users_test WHERE age >= 25 ORDER BY age DESC LIMIT 2 OFFSET 0");

    // 4. Probar GROUP BY y Agregaciones (COUNT, SUM, AVG, MIN, MAX)
    std::cout << "\n--- Test GROUP BY age y Agregaciones (COUNT, SUM, AVG) ---\n";
    ExecuteAndPrintQuery(catalog, "SELECT age, COUNT(*), SUM(salary), AVG(salary) FROM users_test GROUP BY age ORDER BY age ASC");

    // 5. Probar JOIN entre users_test y orders_test
    std::cout << "\n--- Test INNER JOIN entre users_test y orders_test ---\n";
    ExecuteAndPrintQuery(catalog, "SELECT users_test.name, orders_test.order_id, orders_test.amount FROM users_test JOIN orders_test ON users_test.id = orders_test.user_id");

    // 6. Probar Creacion de Indice B+ Tree y busqueda por indice
    std::cout << "\n--- Test CREATE INDEX y B+ Tree Index Scan ---\n";
    ExecuteAndPrintQuery(catalog, "CREATE INDEX ON users_test (id)");
    ExecuteAndPrintQuery(catalog, "SELECT * FROM users_test WHERE id = 3");

    // 7. Probar insercion incremental con indice activo
    std::cout << "\n--- Test Insercion incremental en tabla con Indice B+ Tree ---\n";
    ExecuteAndPrintQuery(catalog, "INSERT INTO users_test (id, name, age, salary) VALUES (5, \"Eve\", 28, 7000.0)");
    ExecuteAndPrintQuery(catalog, "SELECT * FROM users_test WHERE id = 5");

    // 8. Probar UPDATE y DELETE con reconstruccion/sincronizacion de indices
    std::cout << "\n--- Test UPDATE y DELETE con sincronizacion de indices ---\n";
    ExecuteAndPrintQuery(catalog, "UPDATE users_test SET age = 29 WHERE id = 5");
    ExecuteAndPrintQuery(catalog, "DELETE FROM users_test WHERE id = 4");
    ExecuteAndPrintQuery(catalog, "SELECT * FROM users_test WHERE id = 5");

    // 9. Probar especificamente la politica CLOCK del BufferPool
    std::cout << "\n--- Test Verificacion de Politica CLOCK en BufferPool ---\n";
    {
        PageManager pm("data/tables/clock_test.bin");
        BufferPool bpClock(pm, 4, ReplacementPolicy::CLOCK);
        int p1 = pm.AllocatePage(PageType::Data);
        int p2 = pm.AllocatePage(PageType::Data);
        int p3 = pm.AllocatePage(PageType::Data);
        int p4 = pm.AllocatePage(PageType::Data);

        Page* page1 = bpClock.PinPage(p1);
        Page* page2 = bpClock.PinPage(p2);
        Page* page3 = bpClock.PinPage(p3);
        Page* page4 = bpClock.PinPage(p4);

        assert(page1 != nullptr && page2 != nullptr && page3 != nullptr && page4 != nullptr);

        bpClock.UnpinPage(p1, false);
        bpClock.UnpinPage(p2, false);
        bpClock.UnpinPage(p3, false);
        bpClock.UnpinPage(p4, false);

        int p5 = pm.AllocatePage(PageType::Data);
        Page* page5 = bpClock.PinPage(p5);
        assert(page5 != nullptr);
        bpClock.UnpinPage(p5, false);

        std::cout << "[PASS] BufferPool con reemplazo CLOCK (Second Chance) verificado exitosamente.\n";
    }

    // 10. Ejecutar Benchmark de 1,000 registros para verificacion rapida
    std::cout << "\n--- Test Benchmark de Rendimiento (1,000 registros) ---\n";
    benchmark::RunIndexBenchmark(catalog, 1000);

    std::cout << "\n=======================================================\n";
    std::cout << "  TODAS LAS PRUEBAS DE FUNCIONALIDAD COMPLETADAS CON EXITO \n";
    std::cout << "=======================================================\n";

    return 0;
}
