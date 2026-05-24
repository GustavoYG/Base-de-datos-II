# Informe - Semana 5

## Objetivo
Implementar y verificar pruebas unitarias del Storage Manager (PageManager y RecordManager). Generar evidencias de funcionamiento y comandos para reproducir localmente.

## Cambios realizados
- Añadido test unitario `tests/test_storage_manager.cpp` que cubre:
  - `PageManager`: AllocatePage, ReadPage, WritePage y detección de corrupción vía checksum.
  - `RecordManager`: inserción de 300 registros, persistencia y lectura por `(pageId, slot)`.
- Compilada y ejecutada la utilidad `tools/dump_page.cpp` para inspeccionar páginas.

## Archivos añadidos / modificados
- `tests/test_storage_manager.cpp` (nuevo)
- `tests/test_slot_directory.cpp` (ya existente, prueba de 200 registros)
- `tools/dump_page.cpp` (nueva utilidad)
- Código fuente modificado previamente para Slot Directory: `include/common/types.h`, `src/storage/page_manager.cpp`, `src/storage/record_manager.cpp`.

## Cómo compilar y ejecutar (Windows PowerShell)
Desde la raíz del proyecto ejecuta:

```powershell
# Compilar tests y utilidad
g++ -std=c++17 -Iinclude src/storage/page_manager.cpp src/storage/record_manager.cpp src/common/utils.cpp tests/test_storage_manager.cpp -o tests/test_storage_manager.exe

g++ -std=c++17 -Iinclude src/storage/page_manager.cpp src/storage/record_manager.cpp src/common/utils.cpp tests/test_slot_directory.cpp -o tests/test_slot_directory.exe

g++ -std=c++17 -Iinclude tools/dump_page.cpp -o tools/dump_page.exe

# Ejecutar tests
./tests/test_storage_manager.exe
./tests/test_slot_directory.exe

# Volcar página 0 para inspección
./tools/dump_page.exe data/storage/tables/test_slot_directory.tbl 0
```

## Resultados de ejecución (ejemplo en mi entorno)
- `TestPageManager: OK`
- `TestRecordManager: OK`
- `All storage tests passed.`
- `All 200 records inserted and verified successfully.`
- `tools/dump_page.exe` (página 0): `pageId=0 checksum=74157 freeBytes=24`, `slotCount=26`, `freeSpaceOffset=3952`, primera entrada `passengerId=83`.

## Observaciones y recomendaciones
- Los tests crean archivos dentro de `data/storage/tables/` como artefactos de prueba. Se pueden borrar después si se desea.
- Recomiendo añadir `*.exe` a `.gitignore` y eliminar los ejecutables del repo para mantener el repositorio liviano.
- Siguiente paso sugerido: implementar `DeleteRecord` y pruebas para reutilización de slots, y comenzar con Buffer Manager (paso 6).

---

Informe generado automáticamente por las tareas realizadas en la semana 5.
