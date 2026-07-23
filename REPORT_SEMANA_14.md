# REPORT SEMANA 14

## Resumen y objetivo:

Esta semana corresponde a la Implementación del Modelo de Iterador (Volcano Model) para el procesamiento de consultas SQL. El objetivo es descomponer las consultas en operadores básicos encadenados: **Scan**, **Select** y **Project**, siguiendo la arquitectura de iteradores donde cada operador produce tuplas una a la vez bajo demanda (pull-based), en lugar de materializar resultados intermedios completos.

---

## Referencia conceptual:

El **Volcano Model** (también llamado Iterator Model) es la arquitectura estándar de motores de bases de datos relacionales. Cada operador de consulta implementa una interfaz común con tres métodos:

- **Open()**: Inicializa el operador (abre archivos, prepara estado interno).
- **Next()**: Retorna la siguiente tupla (o fila), o un indicador de fin.
- **Close()**: Libera recursos.

La ventaja clave es que el procesamiento es **lazy (perezoso)**: cada operador solo produce una tupla cuando su padre la solicita. Esto minimiza el uso de memoria porque nunca se materializa el resultado completo de una consulta.

### Flujo del Volcano Model para `SELECT Name, Age FROM titanic WHERE Age > 30`:

```
Project(Name, Age)
    |
    |-- Next() llama a:
Select(Age > 30)
    |
    |-- Next() llama a:
Scan(titanic)
    |
    |-- Next() retorna cada fila del disco/memoria
```

- **Scan**: Recorre todas las filas de la tabla (fuente de datos).
- **Select**: Filtra filas según una condición WHERE, reenvía solo las que cumplen.
- **Project**: Selecciona únicamente las columnas solicitadas en SELECT.

---

## Estado actual del repositorio:

En la rama `chamba`, el motor SQL (`src/sql/sql_engine.cpp`) implementa el procesamiento de consultas de forma **materializada**: carga todas las filas en memoria, aplica el filtro WHERE sobre un `vector<vector<string>>`, proyecta columnas, y muestra un preview limitado a 2 filas.

### Componentes actuales del motor SQL:

| Componente | Archivo | Función |
|---|---|---|
| Tokenizer | `sql_engine.cpp:16-30` | Divide la consulta en tokens respetando comillas |
| Parser | `sql_engine.cpp:153-220` | Extrae SELECT, columnas, FROM, tabla, WHERE |
| Executor | `sql_engine.cpp:254-278` | Escanea filas, evalúa WHERE, proyecta columnas |
| Printer | `sql_engine.cpp:100-149` | Muestra resultados en formato ASCII table |

### Flujo actual (sin Volcano Model):

```
1. Parsear consulta SQL
2. Resolver tabla desde el Catalog (in-memory LoadedTable)
3. For each row in table.rows:
     a. Evaluar condición WHERE (si existe)
     b. Si cumple, extraer columnas proyectadas
     c. Agregar a vector resultado
4. Mostrar primeras 2 filas como preview
```

**Problema**: Este enfoque materializa todas las filas que cumplen la condición antes de mostrar任何 resultado. Para tablas grandes, esto es ineficiente en memoria.

---

## Implementación del Modelo de Iterador:

### Diseño de la jerarquía de operadores:

```
                    Operator (interfaz base)
                   /          |          \
           ScanOperator  SelectOperator  ProjectOperator
```

### Interfaz base (`include/sql/operators.h`):

```cpp
#pragma once
#include <vector>
#include <string>

class Operator {
public:
    virtual ~Operator() = default;
    virtual void Open() = 0;          // Inicializar
    virtual bool Next(std::vector<std::string>& row) = 0;  // Siguiente fila
    virtual void Close() = 0;         // Liberar recursos
    virtual std::vector<std::string> GetSchema() const = 0; // Columnas
};
```

### ScanOperator:

Recorre todas las filas de una tabla almacenada en disco a través del `RecordManager` con el Buffer Pool.

```cpp
class ScanOperator : public Operator {
    RecordManager& rm;
    Schema& schema;
    int currentPageId;
    int currentSlot;
    int totalPages;
    bool isOpen;
public:
    ScanOperator(RecordManager& rm, Schema& schema);
    void Open() override;          //.currentPageId = 0, currentSlot = 0
    bool Next(vector<string>& row) override;  // Lee siguiente registro
    void Close() override;
    vector<string> GetSchema() const override; // Retorna nombres de columnas
};
```

**Algoritmo de Next()**:
1. Intentar leer el registro en `(currentPageId, currentSlot)`.
2. Si el slot es válido, decodificar los bytes a strings usando el Schema y el Serializer.
3. Incrementar `currentSlot`.
4. Si `currentSlot` excede los slots de la página, avanzar a `currentPageId + 1` y reiniciar `currentSlot = 0`.
5. Si no hay más páginas, retornar `false`.

### SelectOperator:

Filtra tuplas según una condición. Actúa como un wrapper sobre un hijo (downstream operator).

```cpp
class SelectOperator : public Operator {
    Operator* child;               // Operador fuente (puede ser Scan u otro)
    int columnIndex;               // Columna a evaluar
    CmpOp op;                      // Operador de comparación
    string value;                  // Valor de comparación
public:
    SelectOperator(Operator* child, int colIdx, CmpOp op, const string& val);
    void Open() override;          // child->Open()
    bool Next(vector<string>& row) override;
    void Close() override;         // child->Close()
};
```

**Algoritmo de Next()**:
```
while (child->Next(row)):
    if EvalCondition(row[columnIndex], op, value):
        return true
return false  // sin más filas que cumplan
```

### ProjectOperator:

Selecciona un subconjunto de columnas de las tuplas de su hijo.

```cpp
class ProjectOperator : public Operator {
    Operator* child;
    vector<int> columnIndices;     // Índices de columnas a proyectar
    vector<string> outputSchema;   // Nombres de columnas de salida
public:
    ProjectOperator(Operator* child, const vector<int>& cols,
                    const vector<string>& names);
    void Open() override;
    bool Next(vector<string>& row) override;
    void Close() override;
};
```

**Algoritmo de Next()**:
```
if child->Next(fullRow):
    row = []
    for each idx in columnIndices:
        row.push_back(fullRow[idx])
    return true
return false
```

---

## Construcción del plan de ejecución:

Para una consulta `SELECT Name, Age FROM titanic WHERE Age > 30`, el motor construye:

```cpp
// 1. Resolver tabla
LoadedTable* t = catalog.Get("titanic");
Schema schema = t->GetSchema();

// 2. Crear ScanOperator
auto scan = new ScanOperator(recordManager, schema);

// 3. Crear SelectOperator (WHERE Age > 30)
int ageIdx = schema.GetColumnIndex("Age");
auto select = new SelectOperator(scan, ageIdx, CmpOp::GT, "30");

// 4. Crear ProjectOperator (SELECT Name, Age)
vector<int> projIdx = {schema.GetColumnIndex("Name"), schema.GetColumnIndex("Age")};
vector<string> projNames = {"Name", "Age"};
auto project = new ProjectOperator(select, projIdx, projNames);

// 5. Ejecutar el plan
project->Open();
vector<string> row;
int count = 0;
while (project->Next(row) && count < PREVIEW_LIMIT) {
    PrintRow(row);
    count++;
}
project->Close();
```

---

## Ventajas del Volcano Model sobre el enfoque actual:

| Aspecto | Enfoque actual (materializado) | Volcano Model (iterator) |
|---|---|---|
| Memoria | Carga todas las filas que cumplen WHERE en un `vector` | Procesa una fila a la vez |
| Latencia primera fila | Espera a escanear toda la tabla | Retorna la primera fila en O(1) |
| Componibilidad | Lógica monolítica en `ExecuteAndPrintQuery` | Operadores intercambiables y reutilizables |
| Extensibilidad | Agregar JOIN/ORDER BY requiere reescribir el executor | Se agrega un nuevo operador sin modificar los existentes |
| Eficiencia con LIMIT | Escanea toda la tabla aunque solo muestre 2 filas | Puede detenerse después de N filas |

---

## Integración con componentes existentes:

```
Catalog (LoadedTable, in-memory)
    |
    v
SQL Parser (sql_engine.cpp)
    |
    v
Plan Builder -> Crea cadena de operadores
    |
    v
ProjectOperator
    └── SelectOperator
         └── ScanOperator
              └── RecordManager (BufferPool -> PageManager -> Disco)
```

### Flujo de datos por capa:

```
Disco (.bin)
  --> PageManager::ReadPage()     // Lee página de 4096 bytes
    --> BufferPool::PinPage()     // Cachea en memoria (LRU/CLOCK)
      --> RecordManager::ReadRecord()  // Decodifica slot directory + bytes
        --> ScanOperator::Next()  // Retorna vector<string> (fila decodificada)
          --> SelectOperator::Next()  // Filtra según condición
            --> ProjectOperator::Next()  // Selecciona columnas
              --> Resultado al usuario
```

---

## Evidencia de implementación:

### Archivos clave a crear/modificar:

| Archivo | Contenido |
|---|---|
| `include/sql/operators.h` | Definición de la interfaz `Operator` y las clases `ScanOperator`, `SelectOperator`, `ProjectOperator` |
| `src/sql/operators.cpp` | Implementación de los tres operadores |
| `src/sql/sql_engine.cpp` | Modificar `ExecuteAndPrintQuery` para construir el plan de operadores en lugar del escaneo lineal |

### Salida esperada de demostración:

```
SQL> SELECT Name, Age FROM titanic WHERE Age > 30;

+---------------------+-----+
| Name                | Age |
+---------------------+-----+
| Cumings, Mrs. ...   |  38 |
| Futrelle, Mrs. ...  |  35 |
+---------------------+-----+
[Mostrando 2 de 178 filas que cumplen la condicion]
```

---

## Próximos pasos:

1. Implementar la interfaz `Operator` y las tres clases de operadores.
2. Integrar el plan builder en `sql_engine.cpp`.
3. Verificar que los resultados son idénticos al enfoque materializado actual.
4. Preparar la demostración comparativa de rendimiento.

---

## Notas finales:

- El Volcano Model es la arquitectura base sobre la cual se construirán JOINs (Semana 15) y optimizaciones con índices.
- La implementación actual en la rama `chamba` demuestra que el SQL parser y el executor funcionan correctamente; el refactor hacia iteradores preserva esa funcionalidad mientras agrega eficiencia y extensibilidad.

Fecha: semana 14
