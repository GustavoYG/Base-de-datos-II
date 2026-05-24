# Reporte - Fundamentos de Almacenamiento

## Alcance
Este reporte resume lo implementado hasta ahora en el proyecto, enfocado en:
- Persistencia basica con escritura atomica.
- Storage Manager I (paginas fijas, lectura y escritura en disco).
- Relacion con el capitulo "From Files To Databases".

---

# 1) Fundamentos de Almacenamiento

## 1.1 Persistencia basica
Persistencia significa guardar datos en disco para que sobrevivan al cierre del programa o un apagado.

## 1.2 Atomicidad y durabilidad
- Atomicidad: la escritura ocurre completa o no ocurre.
- Durabilidad: una vez escrita, la informacion permanece aunque haya fallos.

## 1.3 Riesgo de escribir en el mismo archivo
Sobrescribir un archivo puede dejarlo vacio o incompleto si ocurre un corte durante la escritura. Por eso se evita el update in-place.

---

# 2) Implementacion de persistencia atomica

## 2.1 Flujo aplicado
1) Crear archivo temporal.
2) Escribir datos (en nuestro caso, TXT).
3) Forzar escritura fisica con fsync.
4) Reemplazar el archivo final con rename.
5) Limpiar temporales si ocurre error.

## 2.2 Codigo funcional
Se implementa con `WriteAtomic` en:
- include/storage/atomic_writer.h
- src/storage/atomic_writer.cpp

Comportamiento clave:
- Usa archivo temporal `*.tmp`.
- Llama a `fsync` (_commit en Windows).
- Reemplaza el archivo destino con `rename`.

---

# 3) Relacion con "From Files To Databases"

- Se evita update in-place mediante rename atomico.
- Se usa fsync para asegurar durabilidad del archivo.
- Se reconoce la necesidad de fsync en directorios (pendiente si se requiere total fidelidad).
- Se implementa WAL append-only con checksum como extra de robustez.

---

# 4) Storage Manager I

## 4.1 Definicion de pagina fija
Se usa pagina de 4096 bytes (4KB), con header y data:

- page_id
- checksum
- free_bytes
- data

## 4.2 Operaciones implementadas
- AllocatePage(): crea una pagina vacia.
- ReadPage(): lee una pagina desde disco y valida checksum.
- WritePage(): escribe una pagina completa en disco.

Codigo en:
- include/storage/page_manager.h
- src/storage/page_manager.cpp

## 4.3 Calculo de offsets
Cada pagina `N` se ubica en:

$$
N * 4096
$$

Esto permite leer y escribir en posiciones directas del archivo.

---

# 5) Codigo del Page Manager basico (referencia)

Se implementa en el proyecto como clase `PageManager`, con `PAGE_SIZE = 4096`.
Incluye:
- definicion de `PageHeader` y `Page`.
- `AllocatePage`, `ReadPage`, `WritePage`.
- checksum simple para validar integridad.

---

# 6) Estado actual
- Persistencia atomica: COMPLETA.
- Storage Manager I (paginas fijas + lectura/escritura): COMPLETO.
- Codigo funcional listo en el proyecto.
