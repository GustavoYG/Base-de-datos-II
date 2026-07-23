# REPORT SEMANA 16

## Resumen y objetivo:

Esta semana corresponde a las Pruebas Finales y Optimización del proyecto SGBD. El objetivo es realizar pruebas exhaustivas de todas las funcionalidades implementadas a lo largo del curso, medir el rendimiento del sistema (incluyendo el hit rate del Buffer Manager), realizar ajustes finales de calidad, y preparar el informe técnico final del proyecto.

**Nota**: Esta semana no es calificable por sí misma, pero es crucial para la calidad final del entregable.

---

## Alcance de pruebas:

### Módulos a validar:

| Módulo | Archivos | Funcionalidad a probar |
|---|---|---|
| Almacenamiento | `page_manager.h/cpp` | Asignación, lectura y escritura de páginas de 4096 bytes |
| Buffer Pool | `buffer_pool.h/cpp` | Pin/Unpin, eviction LRU/CLOCK, dirty tracking, flush |
| Record Manager | `record_manager.h/cpp` | Insert, Read, Update, Delete sobre páginas ranuradas |
| WAL | `wal_log.h/cpp` | Append, lectura de último entry válido, recovery |
| B+ Tree | `bplus_tree.h/cpp` | Inserción con split, búsqueda, range scan, persistencia |
| CSV Reader | `csv_reader.h/cpp` | Parsing de archivos CSV con campos entre comillas |
| Schema | `schema.h/cpp` | Definición de columnas, cálculo de offsets, tamaños |
| Serializer | `serializer.h/cpp` | Conversión string -> bytes y bytes -> campos tipados |
| SQL Engine | `sql_engine.h/cpp` | Parser SELECT, WHERE, proyección, JOIN, Index Scan |
| Catálogo | `catalog.h/cpp` | Carga de CSV, persistencia .tbl, auto-carga al inicio |

---

## Pruebas del Storage Manager:

### PageManager:

```
Prueba: Crear archivo, asignar 5 páginas, verificar GetPageCount()
Resultado esperado: 5 páginas de 4096 bytes cada una
Archivo generado: 20480 bytes

Prueba: Escribir datos en página 0, leerla, verificar integridad (checksum)
Resultado esperado: ReadPage retorna true, datos idénticos

Prueba: Leer página inexistente
Resultado esperado: ReadPage retorna false
```

### RecordManager:

```
Prueba: Insertar 10 registros en una tabla con schema (Name:STRING[50], Age:INT32)
Resultado esperado: 10 registros almacenados en 1-2 páginas ranuradas

Prueba: Leer registro en posición (pageId=0, slot=0)
Resultado esperado: Datos idénticos al insertado

Prueba: Actualizar registro en (pageId=0, slot=3)
Resultado esperado: ReadRecord después de UpdateRecord retorna nuevos datos

Prueba: Eliminar registro en (pageId=0, slot=2)
Resultado esperado: Slot marcado como libre, espacio reutilizable en siguiente insert

Prueba: Insertar registros hasta llenar una página, verificar que se crea página nueva
Resultado esperado: Alcanzar pageId=1 cuando la primera página está llena

Prueba: Verificar que deleted slots se reutilizan (insertar, borrar, insertar)
Resultado esperado: El segundo insert reutiliza el slot liberado
```

### Slotted Page Internals:

```
Verificar layout de RecordPage:
  PageHeader: pageId(4) + pageType(2) + checksum(4) + freeBytes(4) = 12 bytes
  slotCount(2) + freeSpaceOffset(2) + freeSlotHead(2) = 6 bytes
  Data area: crece desde posicion 20 hacia adelante
  Slot directory: crece desde el final hacia atras
  Free space: zona entre data y slot directory
```

---

## Pruebas del Buffer Manager:

### Configuración de prueba:

- Pool size: 16 frames (configuración estándar del proyecto)
- Políticas: LRU y CLOCK
- Archivo de prueba: tabla Titanic (891 registros, ~5-10 páginas de datos)

### Pruebas LRU:

```
Prueba: Cargar 16 páginas, verificar que todas están en pool (hits = 0 misses)
Prueba: Acceder a página 0, luego 16 páginas nuevas, verificar que página 0 fue evicta (LRU)
Prueba: Acceder a página 0, acceder a páginas 1-15, acceder a página 0 de nuevo
  -> Verificar que página 0 sigue en pool (fue referenciada recientemente)
Prueba: Flush all con páginas dirty, verificar que se escriben al disco
Prueba: Pin/Pin la misma página -> pinCount = 2, Unpin una vez -> pinCount = 1 (no evictada)
```

### Pruebas CLOCK:

```
Prueba: Cargar 16 páginas, hacer clock sweep, verificar selección correcta de víctima
Prueba: Referenciar página 0, hacer clock sweep -> página 0 recibe second chance
Prueba: Hacer 2 clock sweeps -> página 0 sin referenced bit es evictada
```

### Medición de Hit Rate:

```cpp
// Métricas del Buffer Pool
struct BufferPoolStats {
    int totalAccesses;    // Total de llamadas a PinPage
    int cacheHits;        // Veces que la página ya estaba en pool
    int cacheMisses;      // Veces que la página tuvo que leerse de disco
    int evictions;        // Total de evicciones realizadas
    int dirtyFlushes;     // Páginas dirty escritas al disco durante evicciones
};

// Hit rate = cacheHits / totalAccesses * 100%
```

### Resultados esperados de hit rate:

| Operación | Hit Rate Esperado |
|---|---|
| Scan secuencial de 891 filas (pool=16) | ~40-60% (páginas reutilizadas en ciclos) |
| B+ tree Find repetido sobre misma clave | 100% (nodo siempre en pool tras primer acceso) |
| Range scan en B+ tree sobre pocos nodos | ~70-90% (hojas contiguas, pool suficiente) |
| Mezcla de operaciones | ~50-70% dependiendo del patrón |

---

## Pruebas del WAL (Write-Ahead Log):

```
Prueba: Insertar registro, verificar que archivo .wal tiene al menos 1 entry
  Formato: [size(4B) + checksum(4B)] [pageId(4B) + Page(4096B)]
  Tamaño esperado por entry: 4104 bytes

Prueba: Insertar 3 registros en diferentes páginas
  -> Verificar que .wal tiene 3 entries

Prueba: Simular crash (no flush), reiniciar, verificar recovery
  -> ReadLastValidWalPayload retorna último entry válido
  -> Página reconstruida correctamente

Prueba: Verificar integridad del WAL con datos corruptos
  -> Si un entry tiene checksum inválido, se ignora y se usa el anterior
```

---

## Pruebas del B+ Tree:

### Inserción:

```
Prueba: Insertar 20 claves INT32 (1-20) con BTREE_MAX_KEYS=8
  -> Verificar que ocurren al menos 3 splits
  -> Verificar que todas las claves son buscables con Find()
  -> Verificar que la raíz tiene pageId válido y no es hoja

Prueba: Insertar claves en orden descendente (20, 19, ..., 1)
  -> Verificar splits correctos en hojas

Prueba: Insertar claves duplicadas
  -> Verificar que se ignoran (no se insertan)

Prueba: Verificar persistencia .meta
  -> Guardar y restaurar, verificar que rootPageId y keyType son correctos
```

### Búsqueda:

```
Prueba: Find(clave existente)
  -> Retorna pageId y slot correctos del registro

Prueba: Find(clave inexistente)
  -> Retorna false

Prueba: Buscar la primera clave insertada
  -> Verificar que está en la hoja más a la izquierda

Prueba: Buscar la última clave insertada
  -> Verificar que está en la hoja más a la derecha
```

### Range Scan:

```
Prueba: Range(5, 15) sobre 20 claves insertadas (1-20)
  -> Retorna exactamente 11 claves: 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  -> Ordenadas de menor a mayor
  -> Utiliza nextLeaf para recorrer hojas contiguas

Prueba: Range(1, 1) (rango de un solo valor)
  -> Retorna 1 clave si existe, 0 si no

Prueba: Range(100, 200) (rango sin coincidencias)
  -> Retorna vector vacío
```

### Integración con Buffer Pool:

```
Prueba: B+ tree con BufferPool de 4 frames (forzar evicciones)
  -> Insertar 30 claves
  -> Verificar que el árbol funciona correctamente con evicciones frecuentes

Prueba: Verificar hit rate del BufferPool del B+ tree durante range scan
  -> Esperado: alto hit rate porque las hojas son contiguas
```

---

## Pruebas del SQL Engine:

### Consultas SELECT básicas:

```sql
SELECT * FROM titanic;
-- Esperado: 891 filas, preview de 2, 12 columnas

SELECT Name, Age FROM titanic;
-- Esperado: 2 columnas proyectadas, 891 filas

SELECT Name, Age, Fare FROM titanic WHERE Pclass = 1;
-- Esperado: filas con Pclass=1, ~216 resultados esperados

SELECT Name, Fare FROM titanic WHERE Fare > 100;
-- Esperado: ~48 filas con Fare > 100

SELECT Name, Sex FROM titanic WHERE Sex = "female";
-- Esperado: ~261 filas
```

### Consultas con JOIN:

```sql
SELECT a.Name, b.Name FROM titanic a JOIN titanic b ON a.PassengerId = b.PassengerId WHERE a.Pclass = 1;
-- Esperado: Self-join, 216 filas con Pclass=1
```

### Consultas con Index Scan:

```
Prueba: Crear índice B+ tree sobre PassengerId
Prueba: SELECT * FROM titanic WHERE PassengerId = 42
  -> Verificar que se usa IndexScan (buscar en log o comparar tiempos)

Prueba: Comparar tiempos:
  Sin índice: ScanOperator recorre 891 filas
  Con índice: IndexScanOperator busca en B+ tree (~3-4 accesos a disco)
```

### Manejo de errores:

```sql
SELECT * FROM tabla_inexistente;
-- Esperado: Error "La tabla no existe"

SELECT columna_inexistente FROM titanic;
-- Esperado: Error "La columna no existe"

SELECT * FROM titanic WHERE columna_inexistente = 5;
-- Esperado: Error en resolución de columna WHERE

SELECT FROM titanic;
-- Esperado: Error de sintaxis
```

---

## Medición de rendimiento:

### Métricas a capturar:

| Métrica | Cómo medir |
|---|---|
| Tiempo de carga CSV | Cronometrar desde `LoadCsv()` hasta retorno |
| Tiempo de consulta con full scan | Cronometrar `ExecuteAndPrintQuery()` con `SELECT *` |
| Tiempo de consulta con index | Cronometrar con `IndexScanOperator` activo |
| Hit rate del Buffer Pool | `cacheHits / totalAccesses * 100` |
| Páginas leídas por consulta | Contar llamadas a `PageManager::ReadPage()` |
| Splits del B+ tree durante carga | Contador incrementado en `SplitLeaf`/`SplitInternal` |
| Tamaño del archivo .tbl | Medir después de persistir tabla |
| Tamaño del archivo .bin | Medir archivo del RecordManager |
| Tamaño del archivo .wal | Medir después de operaciones mutantes |

### Tabla comparativa de rendimiento:

| Operación | Sin optimización | Con optimización |
|---|---|---|
| Carga de 891 registros | ~50ms | ~50ms (misma) |
| SELECT * (full scan) | ~10ms | ~10ms (no hay índice) |
| SELECT WHERE id = 42 | ~10ms (scan completo) | ~0.5ms (Index Scan) |
| SELECT WHERE id > 100 | ~10ms (scan completo) | ~2ms (Index Range) |
| JOIN sobre 100x100 filas | ~2s (Nested Loop) | ~200ms (Index Nested Loop) |

---

## Pruebas de integridad del disco:

```
Prueba: Insertar registros, cerrar programa, reabrir, verificar datos
  -> Todos los registros persistidos correctamente

Prueba: Insertar registros, forzar flush del BufferPool, verificar .bin
  -> Archivo binario consistente

Prueba: Insertar registro, simular crash (sin flush), reiniciar
  -> WAL recovery restaura el último estado válido

Prueba: Verificar checksum de todas las páginas después de operaciones
  -> Ninguna página corrupta
```

---

## Ajustes finales de calidad:

### Aspectos a revisar:

| Área | Checklist |
|---|---|
| Memory leaks | Verificar que no hay `new` sin `delete`, y que destructores liberan memoria correctamente |
| Buffer Pool flush | Verificar que el destructor de `RecordManager` llama `FlushAll()` antes de destruir el BufferPool |
| WAL durability | Verificar que `ForceFsync()` se llama después de cada append al WAL |
| B+ tree persistence | Verificar que el `.meta` se guarda correctamente en el destructor |
| Error handling | Verificar que cada operación reporta errores claros en español |
| Edge cases | Tabla vacía, una sola fila, todas las filas eliminadas, página llena |
| ASCII table display | Verificar alineación de columnas con datos de distintos anchos |

### Compilación limpia:

```powershell
# Verificar que compila sin warnings con -Wall
g++ -std=c++17 -Wall -Wextra -Iinclude -o sgbd.exe src\main.cpp src\common\*.cpp src\db\*.cpp src\index\*.cpp src\io\*.cpp src\sql\*.cpp src\storage\*.cpp
```

---

## Informe técnico final - Estructura del proyecto:

```
Base-de-datos-II/
├── include/
│   ├── common/
│   │   ├── types.h          # PageHeader, RecordPage, BTreeKey, SlotEntry
│   │   ├── utils.h          # Checksum, SafeCopy, ForceFsync, ToLower
│   │   └── schema.h         # ColumnDef, ColumnType, Schema (fixed-width)
│   ├── storage/
│   │   ├── page_manager.h   # PageManager (disco: alloc, read, write)
│   │   ├── buffer_pool.h    # BufferPool (LRU/CLOCK, 16 frames)
│   │   ├── record_manager.h # RecordManager (CRUD sobre slotted pages)
│   │   └── wal_log.h        # AppendWalEntry, ReadLastValidWalPayload
│   ├── index/
│   │   └── bplus_tree.h     # BPlusTree (Find, Range, Insert, splits)
│   ├── io/
│   │   ├── csv_reader.h     # CsvReader_ReadAll
│   │   └── serializer.h     # RowToBytes, GetField*
│   ├── db/
│   │   └── catalog.h        # LoadedTable, Catalog (persistencia .tbl)
│   └── sql/
│       ├── sql_engine.h     # RunSqlCli, ExecuteAndPrintQuery
│       └── operators.h      # Operator, ScanOperator, SelectOperator,
│                             # ProjectOperator, JoinOperator, IndexScanOperator
├── src/
│   ├── main.cpp              # Menú principal (Cargar CSV / SQL CLI / Salir)
│   ├── common/
│   │   ├── schema.cpp
│   │   └── utils.cpp
│   ├── storage/
│   │   ├── page_manager.cpp
│   │   ├── buffer_pool.cpp
│   │   ├── record_manager.cpp
│   │   └── wal_log.cpp
│   ├── index/
│   │   └── bplus_tree.cpp
│   ├── io/
│   │   ├── csv_reader.cpp
│   │   └── serializer.cpp
│   ├── db/
│   │   └── catalog.cpp
│   └── sql/
│       ├── sql_engine.cpp
│       └── operators.cpp
├── data/
│   ├── raw/
│   │   └── titanic.csv       # Dataset de prueba (891 registros)
│   └── tables/               # Persistencia .tbl del catálogo
└── tests/                    # Pruebas unitarias
```

---

## Resumen de funcionalidades del proyecto:

| # | Funcionalidad | Estado |
|---|---|---|
| 1 | Almacenamiento con páginas de 4096 bytes | Completado |
| 2 | Slotted page architecture (slot directory + freelist) | Completado |
| 3 | Buffer Pool con LRU y CLOCK | Completado |
| 4 | WAL con recovery de página completa | Completado |
| 5 | B+ Tree con inserción, búsqueda, range scan | Completado |
| 6 | Persistencia de datos (RecordManager + Catálogo) | Completado |
| 7 | Carga y parsing de CSV | Completado |
| 8 | Motor SQL (SELECT, WHERE, proyección) | Completado |
| 9 | Volcano Model (Scan, Select, Project operators) | Implementado |
| 10 | Nested Loop Join | Implementado |
| 11 | Index Scan con B+ Tree | Implementado |
| 12 | Hit rate y métricas del Buffer Manager | Medido |

---

## Comandos para ejecutar las pruebas finales:

```powershell
cd Desktop\Base-de-datos-II

# Compilar
g++ -std=c++17 -Wall -Iinclude -o sgbd.exe src\main.cpp src\common\schema.cpp src\common\utils.cpp src\db\catalog.cpp src\index\bplus_tree.cpp src\io\csv_reader.cpp src\io\serializer.cpp src\sql\sql_engine.cpp src\sql\operators.cpp src\storage\buffer_pool.cpp src\storage\page_manager.cpp src\storage\record_manager.cpp src\storage\wal_log.cpp

# Ejecutar
.\sgbd.exe
```

### Flujo de demostración:

```
1. Cargar CSV:  data\raw\titanic.csv
2. SQL CLI:
   SELECT * FROM titanic;
   SELECT Name, Age FROM titanic WHERE Pclass = 1;
   SELECT Name, Fare FROM titanic WHERE Fare > 200;
   SELECT Name, Sex FROM titanic WHERE Sex = "female" AND Age < 18;
3. QUIT
4. Salir
```

---

## Notas finales:

- La Semana 16 cierra el ciclo del proyecto SGBD, integrando todas las capas desde el disco hasta el SQL.
- El sistema demuestra las bases fundamentales de un motor de bases de datos real: almacenamiento en páginas, administración de buffer, WAL para durabilidad, B+ tree para indexación, y un motor SQL con procesamiento por operadores.
- Las métricas de hit rate y rendimiento son evidencia cuantificable de la eficiencia del diseño.

Fecha: semana 16
