# Informe - Proyecto Base de Datos II — Semana 3

## Resumen
Hasta la semana 3 se implementó la lectura básica de archivos CSV, la inferencia simple de tipos por campo y una función de visualización que imprime cada fila con su tipo inferido. El código base se encuentra en `main.cpp` y se ha añadido una versión corregida en `sgbd.cpp`.

## Archivos agregados
- `sgbd.cpp` — versión corregida y segura del código con:
  - `getDataType` robusta (maneja signo y un solo punto decimal).
  - `saveColumnData` corregida (salta encabezados, búsqueda case-insensitive y escape de caracteres).
  - `filterColumnData` segura (salta encabezado y valida conversiones numéricas antes de `stoi`).
- `REPORT_SEMANA_3.md` — este informe.

## Funcionalidades entregadas (semana 3)
- Lectura de CSV y parsing básico (`DataB`).
- Inferencia de tipo por campo (`Esquemas::getDataType`).
- Impresión de la tabla con tipos inferidos (`Esquemas::printData`).

## Problemas detectados y correcciones aplicadas
- `getDataType` original no manejaba correctamente signos y múltiples puntos. Ahora:
  - Permite signo inicial `+`/`-`.
  - Acepta un único punto decimal para floats.
  - Clasifica como `string` si aparecen otros caracteres.
- `saveColumnData` original iteraba sobre la fila 0 (encabezados) como si fueran datos y comparaba nombres después de convertir a minúsculas de forma inconsistente. Ahora:
  - Busca la columna por nombre de forma case-insensitive.
  - Busca `ID` también case-insensitive.
  - Escribe un encabezado `ID\t<Column>` y luego las filas reales (saltando la primera fila).
- `filterColumnData` original hacía `stoi` sin validar (podía lanzar excepción sobre encabezados o valores no numéricos). Ahora:
  - Salta la primera línea (encabezado).
  - Valida que la cadena sea entero antes de convertirla.

## Código mínimo sugerido para la entrega
Se añadió `sgbd.cpp` con la versión corregida. Si deseas reemplazar `main.cpp` por la versión mínima, puedo aplicar el parche.

## Cómo compilar y probar (Windows con g++)
Abre PowerShell en la carpeta del proyecto y ejecuta:

```powershell
g++ sgbd.cpp -o sgbd.exe
.\sgbd.exe
```

## Próximos pasos recomendados
- Confirmar si quieres que reemplace `main.cpp` por la versión mínima preparada para la entrega.
- Si deseas, pruebo la compilación aquí y reporto errores o warnings.

---
Generado el 03/05/2026.

---

# FASE 1 — Planificacion y estructura del proyecto

## Objetivo de la fase
Definir una estructura clara y escalable para un mini-SGBD en C++ que permita, en fases futuras, implementar persistencia, page manager, indexacion, recuperacion y consultas. La estructura debe ser simple de entender y evitar funciones avanzadas.

## Estructura de carpetas propuesta

```
ProyectoBaseDeDatosII/
├─ data/
│  ├─ raw/
│  │  └─ titanic.csv
│  └─ storage/
│     ├─ tables/
│     ├─ indexes/
│     └─ wal/
├─ docs/
│  └─ REPORT_SEMANA_3.md
├─ include/
│  ├─ common/
│  │  ├─ types.h
│  │  └─ utils.h
│  ├─ io/
│  │  ├─ csv_reader.h
│  │  └─ serializer.h
│  ├─ storage/
│  │  ├─ file_io.h
│  │  ├─ atomic_writer.h
│  │  ├─ page_manager.h
│  │  └─ record_manager.h
│  ├─ index/
│  │  └─ simple_index.h
│  └─ query/
│     └─ query_engine.h
├─ src/
│  ├─ common/
│  │  ├─ types.cpp
│  │  └─ utils.cpp
│  ├─ io/
│  │  ├─ csv_reader.cpp
│  │  └─ serializer.cpp
│  ├─ storage/
│  │  ├─ file_io.cpp
│  │  ├─ atomic_writer.cpp
│  │  ├─ page_manager.cpp
│  │  └─ record_manager.cpp
│  ├─ index/
│  │  └─ simple_index.cpp
│  ├─ query/
│  │  └─ query_engine.cpp
│  └─ main.cpp
└─ tests/
   ├─ test_atomic_writer.cpp
   ├─ test_csv_serialization.cpp
   └─ test_page_manager.cpp
```

## Archivos principales y responsabilidad de cada modulo

- `src/main.cpp`
  - Punto de entrada. Coordina lectura del CSV, serializacion, escritura y consultas basicas.

- `include/common/types.h` / `src/common/types.cpp`
  - Define estructuras basicas como `PassengerRecord`, `PageHeader`, `RecordHeader`.

- `include/common/utils.h` / `src/common/utils.cpp`
  - Funciones simples: conversion segura, manejo de strings, checksum basico.

- `include/io/csv_reader.h` / `src/io/csv_reader.cpp`
  - Lee CSV y devuelve filas como vector de strings. No guarda en disco.

- `include/io/serializer.h` / `src/io/serializer.cpp`
  - Convierte estructuras internas a bytes y viceversa (serializacion binaria).

- `include/storage/file_io.h` / `src/storage/file_io.cpp`
  - Operaciones basicas de archivo: abrir, leer, escribir, seek. Sin logica avanzada.

- `include/storage/atomic_writer.h` / `src/storage/atomic_writer.cpp`
  - Escritura atomica: temp + fsync + rename. Base para durabilidad.

- `include/storage/page_manager.h` / `src/storage/page_manager.cpp`
  - Maneja paginas fijas de 4096 bytes: leer, escribir, asignar nuevas.

- `include/storage/record_manager.h` / `src/storage/record_manager.cpp`
  - Inserta y lee registros dentro de paginas. Calcula offsets.

- `include/index/simple_index.h` / `src/index/simple_index.cpp`
  - Indice basico (por ejemplo, array ordenado o hash simple) para busquedas iniciales.

- `include/query/query_engine.h` / `src/query/query_engine.cpp`
  - Consultas: por PassengerId, rangos, listados.

- `tests/`
  - Pruebas unitarias simples para validar persistencia, serializacion y paginas.

## Arquitectura escalable (vista simple)

```
CSV -> CSVReader -> Record (struct) -> Serializer -> AtomicWriter -> Storage file
                                                -> PageManager -> RecordManager
                                                       |-> Index -> QueryEngine
```

## Por que cada archivo existe

- `atomic_writer.*`: garantiza seguridad al escribir y evita corrupcion.
- `page_manager.*`: permite leer/escribir bloques fijos y escalar a grandes archivos.
- `record_manager.*`: separa logica de registros y evita mezclarla con paginas.
- `serializer.*`: evita depender del CSV como formato interno.
- `simple_index.*`: prepara el camino a indices mas avanzados.
- `query_engine.*`: separa consultas de almacenamiento para facilitar cambios futuros.

## Preparado para las siguientes fases

- Persistencia: `atomic_writer`, `file_io`.
- Page manager: `page_manager`.
- Indexacion: `simple_index`.
- Recuperacion: carpeta `wal/` lista para log de escritura.
- Consultas: `query_engine`.

---

# FASE 2 — Persistencia atomica

## Explicacion desde cero

### Que significa persistencia?
Persistencia es la capacidad de guardar datos de forma que existan incluso despues de cerrar el programa o apagar el equipo. Es decir, los datos viven en disco, no solo en memoria RAM.

### Que significa atomicidad?
Atomicidad significa que una operacion se aplica por completo o no se aplica. No existe un estado intermedio visible. Si falla, el sistema vuelve al estado anterior.

### Que significa durabilidad?
Durabilidad significa que, una vez confirmada una escritura, esta queda guardada en disco incluso si hay un corte de luz o un cierre inesperado.

### Por que escribir directamente un archivo puede corromper datos?
Si escribes directamente sobre el archivo original y ocurre un corte, el archivo puede quedar a medias, con bytes mezclados o incompletos. El resultado es un archivo corrupto que ni el programa ni el usuario pueden interpretar correctamente.

### Tipos de atomicidad (segun el capitulo 1)
- **Power-loss atomic**: despues de un corte, el lector nunca debe ver un estado corrupto.
- **Reader-writer atomic**: un lector concurrente nunca debe ver un estado corrupto.

Nuestro `WriteAtomic` evita estados intermedios visibles, pero para ser estrictos con power-loss atomic falta asegurar la durabilidad del directorio.

### Nota sobre fsync de directorios
Crear y renombrar archivos actualiza el directorio. Para garantizar durabilidad completa, tambien se debe hacer `fsync` del directorio que contiene el archivo. Esto es un punto pendiente si se busca fidelidad total con el capitulo 1.

## Mecanismo seguro de escritura atomica

### PASO 1: crear archivo temporal
Se escribe en un archivo nuevo para no tocar el original hasta el final.

### PASO 2: escribir datos binarios
Se guarda todo el contenido en un formato estable y compacto.

### PASO 3: forzar escritura fisica en disco usando fsync
Se obliga al sistema operativo a escribir en disco real, no solo en cache.

### PASO 4: renombrar/reemplazar el archivo original de forma atomica
El reemplazo del archivo se hace en un solo paso. Si el sistema cae antes, queda el archivo antiguo intacto.

### PASO 5: limpiar temporales si ocurre error
Si algo falla, se borra el temporal para evitar basura y confusiones.

## Codigo completo (C++ simple)

> Nota: este codigo es intencionalmente simple y entendible. Usa funciones basicas y no depende de librerias avanzadas.

```cpp
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

// Fuerza escritura fisica en disco
static bool force_fsync(FILE* fp) {
  if (!fp) return false;
  fflush(fp);
#ifdef _WIN32
  int fd = _fileno(fp);
  return _commit(fd) == 0;
#else
  int fd = fileno(fp);
  return fsync(fd) == 0;
#endif
}

// Escritura atomica: temp -> fsync -> rename
bool write_atomic(const std::string& targetPath, const std::vector<unsigned char>& data) {
  std::string tempPath = targetPath + ".tmp";

  // PASO 1: crear temporal
  FILE* fp = std::fopen(tempPath.c_str(), "wb");
  if (!fp) return false;

  // PASO 2: escribir binario
  if (!data.empty()) {
    size_t written = std::fwrite(data.data(), 1, data.size(), fp);
    if (written != data.size()) {
      std::fclose(fp);
      std::remove(tempPath.c_str());
      return false;
    }
  }

  // PASO 3: fsync
  if (!force_fsync(fp)) {
    std::fclose(fp);
    std::remove(tempPath.c_str());
    return false;
  }

  std::fclose(fp);

  // PASO 4: rename atomico
  // En Windows, primero borramos el destino si existe
  std::remove(targetPath.c_str());
  if (std::rename(tempPath.c_str(), targetPath.c_str()) != 0) {
    std::remove(tempPath.c_str());
    return false;
  }

  return true;
}
```

## Pruebas necesarias

### 1) Escritura correcta
- Se escribe un archivo y luego se lee para validar que todos los bytes coinciden.

### 2) Reemplazo correcto
- Se crea un archivo base con contenido A. Luego se reescribe con contenido B. Debe quedar B completo.

### 3) Integridad
- Se calcula un checksum simple antes y despues para validar que no hay cambios inesperados.

### 4) Recuperacion ante interrupciones simuladas
- Se simula un fallo antes de renombrar (por ejemplo, se escribe el temporal pero no se renombra). El archivo original debe quedar intacto.

## Codigo de pruebas (simple)

```cpp
#include <cassert>
#include <fstream>
#include <iostream>

static std::vector<unsigned char> read_all(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
}

static unsigned int checksum_simple(const std::vector<unsigned char>& data) {
  unsigned int sum = 0;
  for (size_t i = 0; i < data.size(); ++i) sum += data[i];
  return sum;
}

void test_write_ok() {
  std::vector<unsigned char> data = {1,2,3,4,5};
  bool ok = write_atomic("test.bin", data);
  assert(ok);
  std::vector<unsigned char> out = read_all("test.bin");
  assert(out == data);
}

void test_replace_ok() {
  std::vector<unsigned char> a = {9,9,9};
  std::vector<unsigned char> b = {7,7,7,7};
  assert(write_atomic("test.bin", a));
  assert(write_atomic("test.bin", b));
  std::vector<unsigned char> out = read_all("test.bin");
  assert(out == b);
}

void test_integrity() {
  std::vector<unsigned char> data = {10,20,30,40};
  unsigned int before = checksum_simple(data);
  assert(write_atomic("test.bin", data));
  std::vector<unsigned char> out = read_all("test.bin");
  unsigned int after = checksum_simple(out);
  assert(before == after);
}

void test_recovery_simulated() {
  // Simula interrupcion: creamos temp y NO renombramos
  std::vector<unsigned char> a = {1,1,1};
  std::vector<unsigned char> b = {2,2,2};

  assert(write_atomic("test.bin", a));

  // Escribimos temporal manualmente y lo dejamos ahi
  std::FILE* fp = std::fopen("test.bin.tmp", "wb");
  std::fwrite(b.data(), 1, b.size(), fp);
  std::fclose(fp);

  // El archivo original debe seguir siendo 'a'
  std::vector<unsigned char> out = read_all("test.bin");
  assert(out == a);

  // Limpieza
  std::remove("test.bin.tmp");
}

int main() {
  test_write_ok();
  test_replace_ok();
  test_integrity();
  test_recovery_simulated();
  std::cout << "OK" << std::endl;
  return 0;
}
```

---

# FASE 3 — Carga del CSV y serializacion

## Explicacion

### Que es serializar?
Serializar es convertir una estructura en memoria (por ejemplo un struct) a una secuencia de bytes para poder guardarla en disco o enviarla por red.

### Por que una base de datos no guarda texto CSV directamente?
El CSV es texto. Es grande, lento de leer, y no permite acceso rapido a campos sin parsear la linea completa. La base de datos usa un formato binario compacto que permite leer registros de forma directa y mas eficiente.

## Implementacion

### 1) Estructura interna del registro

```cpp
struct PassengerRecord {
  int passengerId;
  int survived;
  int pclass;
  char name[64];
  char sex[8];
  float age;
  int sibSp;
  int parch;
  char ticket[32];
  float fare;
  char cabin[16];
  char embarked[4];
};
```

### 2) Lectura simple de CSV y conversion

```cpp
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>

static std::vector<std::string> split_csv_line(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  bool inQuotes = false;
  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (c == '"') {
      inQuotes = !inQuotes;
    } else if (c == ',' && !inQuotes) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

static PassengerRecord to_record(const std::vector<std::string>& row) {
  PassengerRecord r;
  std::memset(&r, 0, sizeof(PassengerRecord));

  r.passengerId = std::stoi(row[0]);
  r.survived    = std::stoi(row[1]);
  r.pclass      = std::stoi(row[2]);

  std::strncpy(r.name, row[3].c_str(), sizeof(r.name) - 1);
  std::strncpy(r.sex,  row[4].c_str(), sizeof(r.sex) - 1);

  r.age   = row[5].empty() ? 0.0f : std::stof(row[5]);
  r.sibSp = std::stoi(row[6]);
  r.parch = std::stoi(row[7]);

  std::strncpy(r.ticket, row[8].c_str(), sizeof(r.ticket) - 1);
  r.fare = row[9].empty() ? 0.0f : std::stof(row[9]);

  std::strncpy(r.cabin, row[10].c_str(), sizeof(r.cabin) - 1);
  std::strncpy(r.embarked, row[11].c_str(), sizeof(r.embarked) - 1);

  return r;
}

static std::vector<PassengerRecord> load_csv(const std::string& path) {
  std::ifstream in(path);
  std::vector<PassengerRecord> records;
  std::string line;

  // Salta encabezado
  std::getline(in, line);

  while (std::getline(in, line)) {
    if (line.empty()) continue;
    std::vector<std::string> row = split_csv_line(line);
    if (row.size() < 12) continue;
    records.push_back(to_record(row));
  }
  return records;
}
```

### 3) Serializacion binaria y escritura persistente

```cpp
static std::vector<unsigned char> serialize_records(const std::vector<PassengerRecord>& records) {
  std::vector<unsigned char> out;
  out.resize(records.size() * sizeof(PassengerRecord));

  if (!records.empty()) {
    std::memcpy(out.data(), records.data(), out.size());
  }
  return out;
}

void save_records_binary(const std::string& path, const std::vector<PassengerRecord>& records) {
  std::vector<unsigned char> bytes = serialize_records(records);
  write_atomic(path, bytes);
}
```

## Como transformar PassengerId, Name, Age... a bytes

- `PassengerId` es `int` (4 bytes).
- `Age` es `float` (4 bytes).
- `Name` es un arreglo fijo de `char[64]`.

Por ejemplo, el registro:

```
PassengerId=1, Name="Braund, Mr. Owen Harris", Age=22
```

Se guarda asi (simplificado):

```
01 00 00 00  (int 1)
42 72 61 75 6e 64 2c 20 4d 72 2e 20 4f 77 65 6e 20 48 61 72 72 69 73 00 ... (name)
00 00 b0 41  (float 22.0)
```

En binario, cada campo tiene un tamanio fijo. Esto permite buscar y leer sin parsear texto.

---

# FASE 3.1 — WAL (Append-only log)

## Idea
Un log append-only permite actualizaciones incrementales sin modificar datos en el lugar. Cada entrada tiene un header con `size` y `checksum` para detectar escrituras incompletas.

## Uso en el proyecto
- Al guardar los registros, se agrega una entrada al WAL.
- Si el TXT no existe o esta corrupto, se toma el ultimo payload valido del WAL y se reconstruye.

## Archivos nuevos
- `include/storage/wal_log.h`
- `src/storage/wal_log.cpp`

---

# FASE 4 — Storage Manager I (Page Manager)

## Explicacion desde cero

### Que es una pagina?
Una pagina es un bloque fijo de bytes dentro de un archivo de almacenamiento. En lugar de leer/escribir en tamanios variables, la base de datos trabaja con paginas de tamanio fijo.

### Por que las bases de datos usan paginas?
- Reducen la cantidad de lecturas del disco.
- Simplifican la gestion de memoria.
- Permiten calcular posiciones directas por `page_id`.

### Por que usar 4096 bytes?
4096 bytes (4 KB) es el tamanio tipico de pagina del sistema operativo. Esto aprovecha el acceso a disco de forma eficiente.

## Diseno de pagina fija (4096 bytes)

Cada pagina contiene:

- `page_id` (int)
- `checksum` (int)
- `free_bytes` (int)
- `data` (resto de la pagina)

```
| page_id | checksum | free_bytes | data ... |
```

## Operaciones necesarias

- `WritePage(page_id, buffer)`
- `ReadPage(page_id, buffer)`
- `AllocatePage()`

## Explicacion de offsets

- Si cada pagina mide 4096 bytes, entonces la pagina `N` empieza en:

$$
offset = N * 4096
$$

- Para leer, se hace `seek(offset)` y luego se lee 4096 bytes.
- Para escribir, se hace `seek(offset)` y se escriben 4096 bytes.

## Codigo completo (C++ simple)

```cpp
#include <fstream>
#include <vector>
#include <cstring>

const int PAGE_SIZE = 4096;

struct PageHeader {
  int pageId;
  int checksum;
  int freeBytes;
};

struct Page {
  PageHeader header;
  unsigned char data[PAGE_SIZE - sizeof(PageHeader)];
};

static int simple_checksum(const unsigned char* data, int len) {
  int sum = 0;
  for (int i = 0; i < len; ++i) sum += data[i];
  return sum;
}

class PageManager {
private:
  std::string path;

public:
  PageManager(const std::string& filePath) : path(filePath) {}

  int AllocatePage() {
    std::fstream f(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!f) {
      // Si no existe, lo creamos
      f.open(path.c_str(), std::ios::out | std::ios::binary);
      f.close();
      f.open(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    }

    f.seekg(0, std::ios::end);
    int size = (int)f.tellg();
    int newPageId = size / PAGE_SIZE;

    // Crear pagina vacia
    Page page;
    std::memset(&page, 0, sizeof(Page));
    page.header.pageId = newPageId;
    page.header.freeBytes = sizeof(page.data);
    page.header.checksum = simple_checksum(page.data, sizeof(page.data));

    f.seekp(newPageId * PAGE_SIZE, std::ios::beg);
    f.write((char*)&page, sizeof(Page));
    f.close();

    return newPageId;
  }

  bool WritePage(int pageId, const Page& page) {
    std::fstream f(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!f) return false;

    f.seekp(pageId * PAGE_SIZE, std::ios::beg);
    f.write((char*)&page, sizeof(Page));
    f.close();
    return true;
  }

  bool ReadPage(int pageId, Page& outPage) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;

    f.seekg(pageId * PAGE_SIZE, std::ios::beg);
    f.read((char*)&outPage, sizeof(Page));
    if (!f) return false;

    // Validar checksum
    int chk = simple_checksum(outPage.data, sizeof(outPage.data));
    return chk == outPage.header.checksum;
  }
};
```

## Leer/escribir multiples paginas

- Para leer varias paginas consecutivas, solo se incrementa el offset por `PAGE_SIZE`.
- Para escribir multiples paginas, se repite la misma logica.

Esto permite recorrer el archivo como si fuera un arreglo de paginas.

---

# FASE 5 — Almacenamiento de registros

## Como decidir cuantos registros caben por pagina

Si una pagina tiene 4096 bytes y el encabezado usa 12 bytes, el espacio util es:

$$
usable = 4096 - 12
$$

Si cada registro `PassengerRecord` tiene un tamanio fijo, por ejemplo 160 bytes (depende de los campos), la cantidad maxima es:

$$
max = \left\lfloor \frac{usable}{sizeof(PassengerRecord)} \right\rfloor
$$

## Implementacion

### Estructura simple de pagina de registros

```cpp
struct RecordPage {
  PageHeader header;
  int recordCount;
  unsigned char data[PAGE_SIZE - sizeof(PageHeader) - sizeof(int)];
};
```

### Insercion de registros y lectura por page_id

```cpp
class RecordManager {
private:
  PageManager pm;

public:
  RecordManager(const std::string& path) : pm(path) {}

  bool InsertRecord(const PassengerRecord& r, int& outPageId, int& outSlot) {
    Page page;
    int pageId = pm.AllocatePage();

    if (!pm.ReadPage(pageId, page)) return false;

    // Convertimos Page a RecordPage
    RecordPage* rp = (RecordPage*)&page;
    if (rp->recordCount == 0) {
      rp->recordCount = 0;
    }

    int recordSize = (int)sizeof(PassengerRecord);
    int maxRecords = (int)((sizeof(rp->data)) / recordSize);
    if (rp->recordCount >= maxRecords) return false;

    int offset = rp->recordCount * recordSize;
    std::memcpy(rp->data + offset, &r, recordSize);
    rp->recordCount++;

    // Actualizar header
    rp->header.freeBytes = (int)(sizeof(rp->data) - rp->recordCount * recordSize);
    rp->header.checksum = simple_checksum(rp->data, sizeof(rp->data));

    if (!pm.WritePage(pageId, *(Page*)rp)) return false;

    outPageId = pageId;
    outSlot = rp->recordCount - 1;
    return true;
  }

  bool ReadRecord(int pageId, int slot, PassengerRecord& out) {
    Page page;
    if (!pm.ReadPage(pageId, page)) return false;

    RecordPage* rp = (RecordPage*)&page;
    int recordSize = (int)sizeof(PassengerRecord);
    if (slot < 0 || slot >= rp->recordCount) return false;

    int offset = slot * recordSize;
    std::memcpy(&out, rp->data + offset, recordSize);
    return true;
  }
};
```

## Ejemplo real con datos Titanic

Supongamos que insertamos el primer registro:

```
PassengerId=1, Name="Braund, Mr. Owen Harris", Age=22
```

Entonces el sistema calcula:

- `offset = 0 * sizeof(PassengerRecord)`
- Copia el registro en `data[0..recordSize-1]`
- `recordCount = 1`

Luego, para leerlo:

- Se lee la pagina
- Se hace `offset = 0 * sizeof(PassengerRecord)`
- Se copia a `PassengerRecord` y se obtiene el mismo contenido

## Validacion de integridad

- Se usa `checksum` para verificar que la pagina no cambio por error.
- Si el checksum no coincide, se considera corrupcion.

---

# FASE 6 — Indexacion

## Comparacion rapida de estructuras

### Hashtable
- Ventajas: busqueda O(1) promedio, simple de implementar.
- Desventajas: no soporta rangos ordenados, colisiones, mas memoria.

### Sorted Array
- Ventajas: simple, permite rangos con busqueda binaria.
- Desventajas: insercion O(n), reordenar es caro.

### B+Tree
- Ventajas: ordenado, bueno para rangos, persistente en disco.
- Desventajas: mas complejo, requiere manejo de nodos y splits.

### LSM-tree
- Ventajas: escrituras muy rapidas, bueno para grandes volumenes.
- Desventajas: mas complejo, requiere compaction.

## Recomendacion para proyecto educativo
Para un proyecto educativo y simple, es mejor empezar con **Sorted Array**. Es facil de entender, permite busquedas exactas y rangos con busqueda binaria, y no requiere estructuras complejas.

## Implementacion paso a paso (Sorted Array)

### Estructura de indice

```cpp
struct IndexEntry {
  int key;       // PassengerId
  int pageId;
  int slot;
};
```

### Construccion del indice

```cpp
#include <algorithm>

class SimpleIndex {
private:
  std::vector<IndexEntry> entries;

public:
  void Add(int key, int pageId, int slot) {
    IndexEntry e;
    e.key = key;
    e.pageId = pageId;
    e.slot = slot;
    entries.push_back(e);
  }

  void Build() {
    std::sort(entries.begin(), entries.end(),
      [](const IndexEntry& a, const IndexEntry& b) {
        return a.key < b.key;
      });
  }

  bool Find(int key, IndexEntry& out) const {
    int left = 0;
    int right = (int)entries.size() - 1;
    while (left <= right) {
      int mid = (left + right) / 2;
      if (entries[mid].key == key) {
        out = entries[mid];
        return true;
      } else if (entries[mid].key < key) {
        left = mid + 1;
      } else {
        right = mid - 1;
      }
    }
    return false;
  }

  std::vector<IndexEntry> Range(int minKey, int maxKey) const {
    std::vector<IndexEntry> out;
    for (size_t i = 0; i < entries.size(); ++i) {
      if (entries[i].key >= minKey && entries[i].key <= maxKey) {
        out.push_back(entries[i]);
      }
    }
    return out;
  }
};
```

## Como se usaria

1) Insertar registros y guardar el `pageId` y `slot`.
2) Agregar esos datos al indice.
3) Ordenar una vez con `Build()`.
4) Buscar con `Find()` o rango con `Range()`.

---

# FASE 7 — Consultas

## Consultas requeridas

1) Buscar PassengerId exacto
2) Buscar rango de edades
3) Listar sobrevivientes

## Como el indice acelera consultas

- Para PassengerId exacto, el indice permite busqueda binaria O(log n) sin recorrer todo.
- Para rangos, el indice permite filtrar por llave y luego leer solo los registros necesarios.
- Sin indice, todas las consultas serian O(n) recorriendo todos los registros.

## Implementacion simple de consultas

```cpp
class QueryEngine {
private:
  RecordManager& rm;
  SimpleIndex& idx;

public:
  QueryEngine(RecordManager& r, SimpleIndex& i) : rm(r), idx(i) {}

  bool FindByPassengerId(int passengerId, PassengerRecord& out) {
    IndexEntry e;
    if (!idx.Find(passengerId, e)) return false;
    return rm.ReadRecord(e.pageId, e.slot, out);
  }

  std::vector<PassengerRecord> RangeByAge(float minAge, float maxAge,
                      const std::vector<IndexEntry>& allEntries) {
    std::vector<PassengerRecord> out;
    for (size_t i = 0; i < allEntries.size(); ++i) {
      PassengerRecord r;
      if (rm.ReadRecord(allEntries[i].pageId, allEntries[i].slot, r)) {
        if (r.age >= minAge && r.age <= maxAge) {
          out.push_back(r);
        }
      }
    }
    return out;
  }

  std::vector<PassengerRecord> ListSurvivors(const std::vector<IndexEntry>& allEntries) {
    std::vector<PassengerRecord> out;
    for (size_t i = 0; i < allEntries.size(); ++i) {
      PassengerRecord r;
      if (rm.ReadRecord(allEntries[i].pageId, allEntries[i].slot, r)) {
        if (r.survived == 1) out.push_back(r);
      }
    }
    return out;
  }
};
```

## Ejemplo de uso

```cpp
PassengerRecord r;
if (qe.FindByPassengerId(1, r)) {
  // imprimir r.name, r.age, etc
}

std::vector<PassengerRecord> ages = qe.RangeByAge(20, 30, indexEntries);
std::vector<PassengerRecord> survivors = qe.ListSurvivors(indexEntries);
```

## Resultado esperado

- Busqueda exacta: devuelve un solo pasajero.
- Rango de edades: devuelve varios registros filtrados.
- Sobrevivientes: devuelve solo quienes tienen `survived == 1`.
