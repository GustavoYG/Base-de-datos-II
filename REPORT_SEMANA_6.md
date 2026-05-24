REPORT SEMANA 6

Resumen de cambios:

- Integración de WAL (Write-Ahead Log):
  - Nuevo uso de `include/storage/wal_log.h` y `src/storage/wal_log.cpp` (previo).
  - Antes de marcar una página como duramente actualizada, se escribe una entrada WAL con el `pageId` y la imagen de página (flush garantizado por `AppendWalEntry`).
  - En el constructor de `RecordManager` se aplica una recuperación simple: se lee la última entrada WAL válida y se reaplica para asegurar durabilidad después de un fallo.

- Reutilización de slots (freelist lógico):
- Reutilización de slots (freelist persistente):
  - Se añadió el campo `freeSlotHead` en la estructura `RecordPage` (`include/common/types.h`) para mantener la cabeza del freelist.
  - `DeleteRecord` enlaza el slot eliminado en la lista libre: el `offset` del `SlotEntry` libre almacena el siguiente índice libre y `length` se pone a 0.
  - `InsertRecord` reutiliza el primer slot del freelist si existe (actualiza `freeSlotHead`), evitando incrementar `slotCount`.
  - El espacio de datos del cuerpo de la página no se compacta inmediatamente; la reutilización es del índice de slot y es segura y persistente.

- Integración de `BufferPool` en `RecordManager`:
  - `RecordManager` crea un `BufferPool` interno y usa `PinPage`/`UnpinPage` para lectura/escritura de páginas.
  - Se añade escritura WAL en cada modificación antes de `UnpinPage(..., true)` para respetar la semántica write-ahead.
  - `BufferPool` ya actualiza checksum en `UnpinPage` cuando se marca `dirty`.

Notas de implementación:

- Formato WAL: payload binario = 4 bytes `pageId` (int) + PAGE_SIZE bytes con la imagen de la página.
- Recovery: se lee la última entrada válida y se escribe la página con `PageManager::WritePage`.
- La estrategia de reutilización de slots es simple y segura: reusa índices de slot (length==0); no se implementa compactación de datos en esta semana.

Pruebas realizadas / pendientes:

- Requiere recompilar y ejecutar la batería de tests: `tests/test_storage_manager`, `tests/test_slot_directory`, `tests/test_buffer_pool`.
- Tests esperados a pasar si la integración es consistente: inserciones/lecturas/borrados, comportamiento del BufferPool y persistencia con WAL.
- Tests ejecutados y resultados:
  - `tests/test_storage_manager.exe` → All storage tests passed.
  - `tests/test_slot_directory.exe` → All 200 records inserted and verified successfully.
  - `tests/test_buffer_pool.exe` → BufferPool test passed.
  - `tests/test_wal_recovery.exe` → WAL recovery test passed (simula crash entre WAL append y page write).

Siguientes pasos recomendados:

- Ejecutar los tests y corregir errores de integración si aparecen.
- Implementar truncado o reciclado del WAL tras checkpoint para evitar crecimiento indefinido.
- Implementar compactación/recuperación avanzada (reaplicar múltiples entradas WAL y/o checkpointing).
- Añadir tests que simulen crash entre WAL append y página escrita, para validar recovery.

Fecha: semana 6
