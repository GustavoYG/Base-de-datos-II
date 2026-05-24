# Reporte - Explicacion de archivos del proyecto

Este documento explica, de forma simple, que hace cada archivo del codigo.

---

# Archivos principales (raiz)

- REPORT_SEMANA_3.md: reporte general con fases, explicaciones y avances.
- REPORT_FUNDAMENTOS_ALMACENAMIENTO.md: reporte especifico de persistencia y Page Manager.
- titanic.csv: dataset base en formato CSV.
- sgbd.exe: ejecutable generado al compilar.

---

# include/common

- include/common/types.h: define estructuras base (`PassengerRecord`, `PageHeader`, `Page`, `RecordPage`, `IndexEntry`) y `PAGE_SIZE`.
- include/common/utils.h: declara utilidades simples como checksum y copiado seguro de strings.

# src/common

- src/common/utils.cpp: implementa checksum simple y copiado seguro para arreglos `char`.

---

# include/io

- include/io/csv_reader.h: declara la clase `CsvReader` para leer filas del CSV.
- include/io/serializer.h: declara conversion de filas a registros, serializacion a binario y a texto, y carga desde TXT.

# src/io

- src/io/csv_reader.cpp: implementa lectura basica de CSV con soporte de comillas.
- src/io/serializer.cpp: convierte filas a `PassengerRecord`, serializa a binario, genera TXT y recarga desde TXT.

---

# include/storage

- include/storage/file_io.h: declara lectura completa de archivo binario.
- include/storage/atomic_writer.h: declara escritura atomica a archivo con temp + fsync + rename.
- include/storage/page_manager.h: declara el `PageManager` para paginas fijas (4096 bytes).
- include/storage/record_manager.h: declara insercion y lectura de registros dentro de paginas.
- include/storage/wal_log.h: declara funciones de WAL (append-only log) con checksum.

# src/storage

- src/storage/file_io.cpp: lee un archivo completo en memoria (vector de bytes).
- src/storage/atomic_writer.cpp: implementa escritura atomica con fsync.
- src/storage/page_manager.cpp: implementa `AllocatePage`, `ReadPage`, `WritePage` y valida checksum.
- src/storage/record_manager.cpp: inserta y lee `PassengerRecord` en paginas, calcula offsets.
- src/storage/wal_log.cpp: implementa WAL con header (size + checksum) y recuperacion del ultimo payload valido.

---

# include/index

- include/index/simple_index.h: declara un indice simple basado en array ordenado.

# src/index

- src/index/simple_index.cpp: implementa insercion, ordenamiento y busqueda binaria por `PassengerId`.

---

# include/query

- include/query/query_engine.h: declara el motor de consultas sobre registros.

# src/query

- src/query/query_engine.cpp: implementa consultas por id, rango de edad y sobrevivientes.

---

# src

- src/main.cpp: flujo principal. Lee CSV, guarda en TXT (atomico), escribe WAL, recarga desde TXT y ofrece menu de consultas.

---

# tests

- tests/test_atomic_writer.cpp: prueba basica de escritura atomica.
- tests/test_csv_serialization.cpp: prueba de lectura CSV y serializacion.
- tests/test_page_manager.cpp: prueba de lectura/escritura de paginas.
