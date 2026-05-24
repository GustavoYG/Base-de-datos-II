# Informe - Semana 4

## Objetivo
Implementar el Slot Directory dentro del Storage Manager (tarea 4 del tutorial "Build Your Own Database") y verificar su funcionamiento con pruebas automáticas.

## Resumen de cambios
Se introdujo un esquema de página tipo "RecordPage" con un directorio de slots (Slot Directory). Los registros se escriben desde el inicio del área de datos y las entradas del directorio de slots se escriben desde el final (creciendo hacia atrás). Esto permite manejar inserciones y lecturas por (pageId, slot) sin depender de un layout de registros estrictamente contiguo.

Cambios principales (archivos modificados / añadidos):

- [include/common/types.h](include/common/types.h)
- [src/storage/page_manager.cpp](src/storage/page_manager.cpp)
- [src/storage/record_manager.cpp](src/storage/record_manager.cpp)
- [tests/test_slot_directory.cpp](tests/test_slot_directory.cpp) (nuevo)


## Detalle de los cambios por archivo

**File:** [include/common/types.h](include/common/types.h)

- Añadidos/contenido modificado:

```cpp
#pragma once

#include <cstdint>

const int PAGE_SIZE = 4096;

struct PassengerRecord {
    int32_t passengerId;
    int32_t survived;
    int32_t pclass;
    char name[64];
    char sex[8];
    float age;
    int32_t sibSp;
    int32_t parch;
    char ticket[32];
    float fare;
    char cabin[16];
    char embarked[4];
};

struct PageHeader {
    int32_t pageId;
    int32_t checksum;
    int32_t freeBytes;
};

struct Page {
    PageHeader header;
    unsigned char data[PAGE_SIZE - sizeof(PageHeader)];
};

struct SlotEntry {
    int16_t offset;
    int16_t length;
};

struct RecordPage {
    PageHeader header;
    int16_t slotCount;
    int16_t freeSpaceOffset;
    unsigned char data[PAGE_SIZE - sizeof(PageHeader) - sizeof(int16_t) * 2];
};

struct IndexEntry {
    int32_t key;
    int32_t pageId;
    int32_t slot;
};
```

- Razonamiento:
  - `slotCount` y `freeSpaceOffset` controlan el directorio de slots y el espacio libre para registros.
  - `SlotEntry` almacena `offset` y `length` (16-bit cada uno) y se coloca al final del área `data`.


**File:** [src/storage/page_manager.cpp](src/storage/page_manager.cpp)

- Cambio: al crear/allocar una página ahora inicializamos una `RecordPage` con `slotCount = 0` y `freeSpaceOffset = 0`. Además ajustamos el cálculo del checksum para cubrir exactamente la región posterior al `PageHeader`.

Fragmento clave (simplificado):

```cpp
RecordPage rp;
std::memset(&rp, 0, sizeof(RecordPage));
rp.header.pageId = newPageId;
rp.slotCount = 0;
rp.freeSpaceOffset = 0;
rp.header.freeBytes = sizeof(rp.data);
// checksum sobre la región posterior al header (post-header)
rp.header.checksum = SimpleChecksum(((unsigned char*)&rp) + sizeof(PageHeader), PAGE_SIZE - sizeof(PageHeader));
f.seekp(newPageId * PAGE_SIZE, std::ios::beg);
f.write((char*)&rp, sizeof(RecordPage));
```

- Razonamiento:
  - Al crear una página, es importante dejar inicializados los campos del `RecordPage` para evitar valores basura al leerla posteriormente.
  - El checksum se calcula sobre exactamente los bytes que se verifican en la lectura de página.


**File:** [src/storage/record_manager.cpp](src/storage/record_manager.cpp)

- Reescribí `InsertRecord` y `ReadRecord` para usar el Slot Directory.

Fragmentos clave (resumidos):

InsertRecord:

```cpp
const int recordSize = (int)sizeof(PassengerRecord);
const int slotEntrySize = (int)sizeof(SlotEntry);

// calcular espacio libre: sizeof(data) - usedData - usedSlotDir
int usedData = rp->freeSpaceOffset;
int usedSlotDir = rp->slotCount * slotEntrySize;
int freeSpace = (int)sizeof(rp->data) - usedData - usedSlotDir;

int required = recordSize + slotEntrySize;
if (freeSpace < required) { /* allocate new page */ }

// escribir registro al offset libre
int recordOffset = rp->freeSpaceOffset;
std::memcpy(rp->data + recordOffset, &r, recordSize);
rp->freeSpaceOffset += recordSize;

// escribir entrada de slot al final (crecimiento hacia atrás)
SlotEntry se;
se.offset = (int16_t)recordOffset;
se.length = (int16_t)recordSize;
int slotPos = (int)sizeof(rp->data) - (rp->slotCount + 1) * slotEntrySize;
std::memcpy(rp->data + slotPos, &se, slotEntrySize);

rp->slotCount++;

// actualizar freeBytes y checksum
rp->header.freeBytes = (int)(sizeof(rp->data) - rp->freeSpaceOffset - rp->slotCount * slotEntrySize);
rp->header.checksum = SimpleChecksum(((unsigned char*)rp) + sizeof(PageHeader), PAGE_SIZE - sizeof(PageHeader));

pm.WritePage(currentPageId, *(Page*)rp);
```

ReadRecord:

```cpp
if (slot < 0 || slot >= rp->slotCount) return false;
int slotPos = (int)sizeof(rp->data) - (slot + 1) * slotEntrySize;
SlotEntry se;
std::memcpy(&se, rp->data + slotPos, slotEntrySize);

if (se.offset < 0 || se.offset + se.length > (int)sizeof(rp->data)) return false;
std::memcpy(&out, rp->data + se.offset, sizeof(PassengerRecord));
```

- Razonamiento:
  - Este layout permite insertar registros de tamaño fijo o variable (si se adapta length), reutilizar slots en futuras mejoras y localizar registros por slot index.


**File:** [tests/test_slot_directory.cpp](tests/test_slot_directory.cpp) (nuevo)

- El test crea `data/storage/tables/test_slot_directory.tbl`, inserta 200 registros, guarda la ubicación `(pageId, slot)` de cada inserción, crea un nuevo `RecordManager` para forzar lectura desde disco y verifica que cada registro leído coincide con el original (principalmente `passengerId`).

Contenido resumido (archivo completo disponible en el repo):
- Crea directorio `data/storage/tables` si no existe.
- Elimina archivo previo de prueba si existe.
- Inserta 200 registros con campos rellenados.
- Reabre y valida lectura de cada registro.


## Cómo compilar y ejecutar la prueba

Desde la raíz del proyecto (Windows PowerShell):

```powershell
g++ -std=c++17 -Iinclude src/storage/page_manager.cpp src/storage/record_manager.cpp src/common/utils.cpp tests/test_slot_directory.cpp -o tests/test_slot_directory.exe
./tests/test_slot_directory.exe
```

Salida esperada:
```
All 200 records inserted and verified successfully.
```


## Notas técnicas y decisiones

- Checksum: el checksum se calcula sobre la región posterior al `PageHeader` (esto incluye `slotCount`, `freeSpaceOffset` y todo `data`), y la lectura valida que el checksum coincide con el almacenado. Esto previene corrupciones de página.
- Layout de página: Elegí `int16_t` para `SlotEntry` ya que el tamaño de página y offsets caben en 16 bits (PAGE_SIZE=4096). Si se requiere soportar páginas mayores o offsets > 32767, cambiar a `int32_t`.
- `freeSpaceOffset` crece hacia delante, `slot` entries crecen hacia atrás, lo que evita fragmentación hasta que no haya espacio para ambos.


## Pruebas adicionales recomendadas

- Insertar registros de tamaño variable y verificar que `length` del `SlotEntry` se respete.
- Implementar `DeleteRecord(pageId, slot)` que marque un slot como libre y permita reutilización.
- Pruebas de concurrencia y estrés con múltiples inserciones/lecturas.
- Integración con Buffer Manager (paso 6) para medir rendimiento y caché de páginas.


## Siguientes pasos propuestos

- Implementar eliminación y reutilización de slots.
- Comenzar con paso 6 (Buffer Manager I): diseñar `BufferPool` y `PageTable` y adaptar `PageManager` y `RecordManager` para trabajar con el buffer.


---

Si quieres, puedo:

- Añadir el test al `CMakeLists.txt` si lo deseas.
- Implementar `DeleteRecord` y su prueba.
- Empezar con el Buffer Manager (paso 6) y preparar el diseño y código base.

Dime cuál de estas tareas quieres que haga a continuación.


## Cambios recientes y artefactos añadidos

He añadido los siguientes archivos y ejecutado pruebas adicionales durante la semana:

- `tests/test_slot_directory.cpp` — prueba automática que inserta 200 registros y verifica su lectura.
- `tools/dump_page.cpp` — utilidad para volcar una página (por `pageId`) mostrando `PageHeader`, `slotCount`, entradas de slot y el primer `PassengerRecord` para inspección manual.

Resultados relevantes:

- La prueba automática pasó: `All 200 records inserted and verified successfully.`
- Ejecución de `tools/dump_page.exe` sobre las páginas 0 y 1 devolvió:

  - Página 0: `pageId=0 checksum=74157 freeBytes=24`, `slotCount=26`, `freeSpaceOffset=3952`. Primer registro leído: `passengerId=83 survived=1`.
  - Página 1: `pageId=1 checksum=74740 freeBytes=24`, `slotCount=26`, `freeSpaceOffset=3952`. Primer registro leído: `passengerId=83 survived=27`.

Notas sobre el repositorio y próximos pasos:

- Si quieres que suba los cambios al repositorio remoto sin incluir los archivos `.md` (informes), puedo hacer el commit y push excluyendo `*.md`.
- A continuación (si autorizas) haré el commit de todo excepto los `.md` y empujaré a tu remoto.
