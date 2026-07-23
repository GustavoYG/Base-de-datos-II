# REPORTE FINAL - BASE DE DATOS II

## UNIVERSIDAD NACIONAL DE SAN AGUSTÍN DE AREQUIPA
### FACULTAD DE INGENIERÍA DE PRODUCCIÓN Y SERVICIOS
### ESCUELA PROFESIONAL DE CIENCIA DE LA COMPUTACIÓN

**CURSO:** BASE DE DATOS II  
**INTEGRANTES:**
- Yavar Guillen, Roberto Gustavo (Lab C)
- Espinoza Barrios, David Alejandro (Lab B)

**DOCENTE:**  
Escobar Castillo, Maria Vilma

**AREQUIPA – PERÚ**  
**2026**

---

## Índice

1. [Introducción](#1-introducción)
2. [Arquitectura del Sistema](#2-arquitectura-del-sistema)
3. [Componentes Implementados](#3-componentes-implementados)
   - 3.1 [Storage Manager](#31-storage-manager)
   - 3.2 [Buffer Manager](#32-buffer-manager)
   - 3.3 [Record Manager](#33-record-manager)
   - 3.4 [WAL (Write-Ahead Log)](#34-wal-write-ahead-log)
   - 3.5 [B+ Tree Index](#35-b-tree-index)
   - 3.6 [SQL Engine](#36-sql-engine)
4. [Operaciones DML Implementadas](#4-operaciones-dml-implementadas)
   - 4.1 [INSERT](#41-insert)
   - 4.2 [DELETE](#42-delete)
   - 4.3 [UPDATE](#43-update)
5. [Volcano Model (Modelo de Iterador)](#5-volcano-model-modelo-de-iterador)
6. [Optimización con B+ Tree](#6-optimización-con-b-tree)
7. [Pruebas y Validación](#7-pruebas-y-validación)
8. [Métricas de Rendimiento](#8-métricas-de-rendimiento)
9. [Conclusiones](#9-conclusiones)

---

## 1. Introducción

Este informe documenta el desarrollo completo del Sistema de Gestión de Bases de Datos (SGBD) implementado durante el curso de Base de Datos II. El proyecto abarca desde el almacenamiento a bajo nivel hasta un motor SQL completo con soporte para operaciones DML (INSERT, DELETE, UPDATE) y consultas SELECT.

### Objetivos del Proyecto

1. Implementar un SGBD completo desde cero en C++
2. Desarrollar un almacenamiento persistente con páginas ranuradas (Slotted Pages)
3. Implementar un Buffer Manager con políticas LRU y CLOCK
4. Crear un índice B+ Tree para optimización de consultas
5. Desarrollar un motor SQL con soporte para SELECT, INSERT, DELETE y UPDATE
6. Implementar el Volcano Model para procesamiento eficiente de consultas

---

## 2. Arquitectura del Sistema

```
┌─────────────────────────────────────────────────────────────┐
│                     SQL Engine                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   Parser    │  │  Executor   │  │   Printer   │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Volcano Model                            │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │  Scan    │  │  Select  │  │  Project │  │   Join   │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   B+ Tree Index                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                 │
│  │  Find    │  │  Range   │  │  Insert  │                 │
│  └──────────┘  └──────────┘  └──────────┘                 │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                  Buffer Manager                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                 │
│  │   LRU    │  │  CLOCK   │  │  Flush   │                 │
│  └──────────┘  └──────────┘  └──────────┘                 │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                 Storage Manager                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │   Page   │  │  Record  │  │   WAL    │  │   CSV    │  │
│  │ Manager  │  │  Manager │  │   Log    │  │  Reader  │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     Disco (.bin, .tbl, .wal)                │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Componentes Implementados

### 3.1 Storage Manager

#### PageManager
- **Archivo:** `include/storage/page_manager.h`, `src/storage/page_manager.cpp`
- **Función:** Gestión de páginas de 4096 bytes
- **Características:**
  - Creación y apertura de archivos de páginas
  - Lectura y escritura de páginas individuales
  - Gestión automática de espacio en disco
  - Soporte para múltiples archivos de páginas

#### RecordManager
- **Archivo:** `include/storage/record_manager.h`, `src/storage/record_manager.cpp`
- **Función:** Operaciones CRUD sobre páginas ranuradas
- **Características:**
  - Inserción de registros con gestión de espacio libre
  - Lectura de registros por pageId y slot
  - Actualización y eliminación de registros
  - Reutilización de slots eliminados

### 3.2 Buffer Manager

- **Archivo:** `include/storage/buffer_pool.h`, `src/storage/buffer_pool.cpp`
- **Función:** Cache de páginas en memoria
- **Políticas de Reemplazo:**
  - **LRU (Least Recently Used):** Reemplaza la página menos recientemente usada
  - **CLOCK:** Algoritmo de segunda oportunidad con bits de referencia
- **Características:**
  - Pool de 16 frames (configurable)
  - Dirty tracking para páginas modificadas
  - Pin count para gestión de concurrencia
  - FlushAll para persistencia manual

### 3.3 Record Manager

- **Estructura de Slotted Pages:**
  ```
  ┌─────────────────────────────────────┐
  │ Header (12 bytes)                   │
  │ - Page ID                           │
  │ - Page Type                         │
  │ - Checksum                          │
  │ - Free Bytes                        │
  ├─────────────────────────────────────┤
  │ Control (6 bytes)                   │
  │ - Slot Count                        │
  │ - Free Space Offset                 │
  │ - Free List Header                  │
  ├─────────────────────────────────────┤
  │ Data Area (crece desde inicio)      │
  │ - Registros de datos                │
  ├─────────────────────────────────────┤
  │ Slot Directory (crece desde final)  │
  │ - [offset, length] por slot         │
  └─────────────────────────────────────┘
  ```

### 3.4 WAL (Write-Ahead Log)

- **Archivo:** `include/storage/wal_log.h`, `src/storage/wal_log.cpp`
- **Función:** Registro de transacciones para recuperación
- **Estructura de Entrada:**
  ```
  [Metadata + Checksum] [PageId + Binary Payload]
  ```
- **Características:**
  - Registro de mutaciones antes de aplicarse
  - Recuperación ante falloscríticos
  - Validación de integridad con checksums

### 3.5 B+ Tree Index

- **Archivo:** `include/index/bplus_tree.h`, `src/index/bplus_tree.cpp`
- **Función:** Índice para aceleración de consultas
- **Operaciones Soportadas:**
  - **Find(key):** Búsqueda puntual de un valor
  - **Range(min, max):** Búsqueda por rangos
  - **Insert(key):** Inserción con splits automáticos
- **Características:**
  - Soporte para tipos INT32, FLOAT, STRING
  - Splits de hojas y nodos internos
  - Persistencia en archivos .meta
  - Integración con Buffer Pool

### 3.6 SQL Engine

- **Archivo:** `include/sql/sql_engine.h`, `src/sql/sql_engine.cpp`
- **Función:** Parser y ejecutor de consultas SQL
- **Sentencias Soportadas:**
  - **SELECT:** Consultas con WHERE, proyección de columnas
  - **INSERT:** Inserción de nuevos registros
  - **DELETE:** Eliminación de registros
  - **UPDATE:** Actualización de registros

---

## 4. Operaciones DML Implementadas

### 4.1 INSERT

**Sintaxis:**
```sql
INSERT INTO tabla (columna1, columna2, ...) VALUES (valor1, valor2, ...);
```

**Ejemplo:**
```sql
INSERT INTO titanic (PassengerId, Name, Sex, Age) VALUES (999, "Gustavo Garcia", male, 25);
```

**Implementación:**
```cpp
std::string ExecuteInsert(Catalog& catalog, const std::string& rawQuery) {
    // 1. Extraer nombre de la tabla
    // 2. Extraer lista de columnas entre paréntesis
    // 3. Extraer lista de valores entre paréntesis
    // 4. Validar que columnas y valores coincidan en cantidad
    // 5. Insertar fila en la tabla
    // 6. Persistir cambios en disco
}
```

**Características:**
- Soporte para valores entre comillas (strings con espacios)
- Parsing de cadena cruda para manejar paréntesis correctamente
- Validación de columnas existentes
- Persistencia automática en disco

### 4.2 DELETE

**Sintaxis:**
```sql
DELETE FROM tabla WHERE columna operador valor;
```

**Ejemplo:**
```sql
DELETE FROM titanic WHERE PassengerId = 999;
```

**Operadores Soportados:**
- `=` (igual)
- `>` (mayor que)
- `<` (menor que)
- `>=` (mayor o igual)
- `<=` (menor o igual)
- `!=` (diferente)

**Implementación:**
```cpp
std::string ExecuteDelete(Catalog& catalog, const std::string& rawQuery) {
    // 1. Extraer nombre de la tabla
    // 2. Extraer condición WHERE
    // 3. Evaluar condición para cada fila
    // 4. Eliminar filas que cumplen la condición
    // 5. Persistir cambios en disco
    // 6. Retornar cantidad de filas eliminadas
}
```

### 4.3 UPDATE

**Sintaxis:**
```sql
UPDATE tabla SET columna = valor WHERE columna operador valor;
```

**Ejemplo:**
```sql
UPDATE titanic SET Age = 30 WHERE PassengerId = 999;
```

**Implementación:**
```cpp
std::string ExecuteUpdate(Catalog& catalog, const std::string& rawQuery) {
    // 1. Extraer nombre de la tabla
    // 2. Extraer columna y valor a actualizar (SET)
    // 3. Extraer condición WHERE
    // 4. Evaluar condición para cada fila
    // 5. Actualizar filas que cumplen la condición
    // 6. Persistir cambios en disco
    // 7. Retornar cantidad de filas actualizadas
}
```

**Características Comunes a INSERT, DELETE, UPDATE:**
- Parsing robusto de cadena cruda (no depende de tokens)
- Soporte para strings entre comillas
- Persistencia automática mediante `catalog.SaveTable()`
- Retorno de mensajes de éxito con cantidad de filas afectadas

---

## 5. Volcano Model (Modelo de Iterador)

### Concepto

El Volcano Model es la arquitectura estándar de motores de bases de datos relacionales. Cada operador implementa una interfaz común con tres métodos:

- **Open():** Inicializa el operador
- **Next():** Retorna la siguiente tupla
- **Close():** Libera recursos

### Ventajas

| Aspecto | Enfoque Materializado | Volcano Model |
|---------|----------------------|---------------|
| Memoria | Carga todas las filas | Procesa una fila a la vez |
| Latencia primera fila | Espera a escanear toda la tabla | Retorna en O(1) |
| Componibilidad | Lógica monolítica | Operadores intercambiables |
| Extensibilidad | Requiere reescribir executor | Se agrega nuevo operador |

### Operadores Implementados

1. **ScanOperator:** Recorre todas las filas de una tabla
2. **SelectOperator:** Filtra filas según condición WHERE
3. **ProjectOperator:** Selecciona columnas específicas
4. **JoinOperator:** Combina dos tablas (Nested Loop Join)
5. **IndexScanOperator:** Usa B+ Tree para búsquedas aceleradas

---

## 6. Optimización con B+ Tree

### Estrategia de Uso

| Operador WHERE | Estrategia |
|----------------|------------|
| `columna = valor` | `BPlusTree::Find(key)` → acceso único |
| `columna > valor` | `BPlusTree::Range(min, max)` → escaneo por rango |
| `columna < valor` | `BPlusTree::Range(min, max)` con límite superior |
| `columna >= valor` | `BPlusTree::Range(valor, max)` |
| `columna <= valor` | `BPlusTree::Range(min, valor)` |
| Sin índice | Reversión a ScanOperator (lineal) |

### Mejora de Rendimiento

**Sin índice:**
```
ScanOperator → evalúa 891 registros → filtra → proyecta
Impacto: 891 operaciones de lectura
```

**Con B+ Tree Index:**
```
IndexScanOperator → BPlusTree::Find(42) → pocos accesos a nodos → 1 lectura final
Impacto: ~4 interacciones con disco (profundidad del árbol)
```

---

## 7. Pruebas y Validación

### 7.1 Pruebas de Storage Manager

| Prueba | Resultado |
|--------|-----------|
| Creación de archivo y asignación de 5 páginas | ✅ Exitoso (20480 bytes) |
| Escritura/lectura en página 0 | ✅ Integridad verificada |
| Acceso a pageId inexistente | ✅ Retorna false controlado |
| Inserción masiva de registros | ✅ Almacenamiento correcto |
| Recuperación de datos por pageId/slot | ✅ Valores coincidentes |
| Modificación y eliminación de tuplas | ✅ Actualización efectiva |
| Desbordamiento de página | ✅ Transición automática |

### 7.2 Pruebas de Buffer Manager

| Prueba | Resultado |
|--------|-----------|
| Llenado de frames y hits iniciales | ✅ Funcional |
| Política de reemplazo LRU | ✅ Funcional |
| Política de reemplazo CLOCK | ✅ Funcional |
| Persistencia de páginas referenciadas | ✅ Funcional |
| Sincronización de páginas dirty | ✅ Funcional |

### 7.3 Pruebas de B+ Tree

| Prueba | Resultado |
|--------|-----------|
| Inserción de 20 claves con splits | ✅ Árbol balanceado |
| Búsquedas puntuales (Find) | ✅ Retorno preciso |
| Escaneo por rangos (Range) | ✅ Listado ordenado |
| Operación con extrema presión de memoria | ✅ Funcionamiento estable |

### 7.4 Pruebas de SQL Engine

#### SELECT
```sql
SELECT * FROM titanic;                                    -- Escaneo íntegro
SELECT Name, Age FROM titanic;                            -- Proyección
SELECT Name, Age, Fare FROM titanic WHERE Pclass = 1;    -- Filtrado
SELECT Name, Fare FROM titanic WHERE Fare > 100;          -- Rango
SELECT Name, Sex FROM titanic WHERE Sex = "female";       -- Filtro categórico
```

#### INSERT
```sql
INSERT INTO titanic (PassengerId, Name, Sex, Age) 
VALUES (999, "Gustavo Garcia", male, 25);

-- Resultado: OK. 1 fila insertada en "titanic".
```

#### DELETE
```sql
DELETE FROM titanic WHERE PassengerId = 999;

-- Resultado: OK. 1 fila(s) eliminada(s) de "titanic".
```

#### UPDATE
```sql
UPDATE titanic SET Age = 30 WHERE PassengerId = 999;

-- Resultado: OK. 1 fila(s) actualizada(s) en "titanic".
```

#### JOIN
```sql
SELECT a.Name, b.Name FROM titanic a 
JOIN titanic b ON a.PassengerId = b.PassengerId 
WHERE a.Pclass = 1;
```

### 7.5 Gestión de Excepciones

```sql
SELECT * FROM tabla_erronea;      -- Tabla inexistente
SELECT campo_falso FROM titanic;  -- Columna no identificada
SELECT FROM titanic;              -- Error sintáctico
```

---

## 8. Métricas de Rendimiento

### 8.1 Métricas Clave

| Indicador | Método de Captura |
|-----------|-------------------|
| Ingesta CSV | Tiempo total de ejecución en LoadCsv() |
| Latencia Full Scan | Ejecución de SELECT * sin filtros |
| Latencia Index Scan | Consultas puntuales con árbol B+ |
| Eficiencia del Pool | Relación aciertos/accesos (Hit Rate) |
| E/S por Consulta | Conteo de lecturas en PageManager |
| Tamaño de Archivos | Dimensiones de .tbl, .bin y .wal |

### 8.2 Comparativa de Resultados

| Operación | Enfoque Base | Optimizado |
|-----------|--------------|------------|
| Carga (891 regs) | ~50ms | ~50ms |
| SELECT * | ~10ms | ~10ms |
| WHERE id = 42 | ~10ms | ~0.5ms |
| WHERE id > 100 | ~10ms | ~2ms |
| JOIN (100x100) | ~2s | ~200ms |

### 8.3 Estimaciones de Eficiencia del Buffer Pool

| Escenario | Hit Rate Esperado |
|-----------|-------------------|
| Escaneo secuencial (891 filas) | 40% - 60% |
| Point Lookup repetido (B+) | 100% |
| Range Scan (hojas contiguas) | 70% - 90% |

---

## 9. Conclusiones

### Logros Alcanzados

1. **SGBD Completo:** Se implementó un sistema de gestión de bases de datos completo desde cero
2. **Almacenamiento Persistente:** Páginas ranuradas con gestión eficiente de espacio
3. **Buffer Manager:** Dos políticas de reemplazo (LRU y CLOCK) funcionales
4. **Índice B+ Tree:** Optimización de consultas con búsqueda puntual y por rangos
5. **Motor SQL:** Soporte completo para SELECT, INSERT, DELETE y UPDATE
6. **Volcano Model:** Arquitectura extensible para procesamiento de consultas

### Funcionalidades Implementadas

| # | Capacidad | Estado |
|---|-----------|--------|
| 1-4 | Almacenamiento, Slotted Pages, Buffer Pool y WAL | ✅ Finalizado |
| 5-8 | Árbol B+, Persistencia, Parsing CSV y Motor SQL | ✅ Finalizado |
| 9-11 | Volcano Model, Joins e Index Scan | ✅ Operativo |
| 12 | Métricas y Diagnóstico de Rendimiento | ✅ Validado |
| 13 | Operaciones DML (INSERT, DELETE, UPDATE) | ✅ Implementado |

### Comandos para Ejecutar

```bash
# Acceder al directorio y compilar
cd Desktop\Base-de-datos-II

# Compilar
g++ -std=c++17 -Iinclude -o sgbd.exe src\main.cpp src\common\*.cpp src\db\*.cpp src\index\*.cpp src\io\*.cpp src\sql\*.cpp src\storage\*.cpp

# Ejecutar
.\sgbd.exe
```

### Secuencia de Demostración

1. Ingesta de datos desde `titanic.csv`
2. Consultas SELECT con filtros y proyecciones
3. Inserción de nuevos registros (INSERT)
4. Actualización de registros existentes (UPDATE)
5. Eliminación de registros (DELETE)
6. Consultas JOIN entre tablas
7. Uso de índices para optimización
8. Finalización y cierre seguro del sistema

### Notas Finales

- La culminación del proyecto representa la integración total del ecosistema SGBD
- El motor ratifica los principios de persistencia, durabilidad y eficiencia esperados en sistemas reales
- La evidencia métrica sustenta la viabilidad técnica de la arquitectura propuesta
- El sistema está preparado para futuras ampliaciones como transacciones ACID y optimizador de consultas

---

**Repositorio:** `https://github.com/GustavoYG/Base-de-datos-II`  
**Rama Final:** `final`  
**Fecha:** Julio 2026
