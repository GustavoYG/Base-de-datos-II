# REPORT SEMANA 15

## Resumen y objetivo:

Esta semana corresponde a la Implementación de al menos un algoritmo de Join (Nested Loop Join) y al uso del índice B+ Tree para optimizar consultas. El objetivo es extender el motor SQL del proyecto para soportar consultas que involucren múltiples tablas mediante JOIN, y utilizar el árbol B+ ya implementado (`include/index/bplus_tree.h`) para acelerar búsquedas puntuales en lugar de realizar escaneos completos (full table scan).

---

## Referencia conceptual:

### Nested Loop Join:

El Nested Loop Join es el algoritmo más simple y fundamental para combinar filas de dos tablas. Su pseudocódigo es:

```
for each row outer in TableOuter:
    for each row inner in TableInner:
        if join_condition(outer, inner):
            emit(concat(outer, inner))
```

**Complejidad**: O(N * M) donde N y M son las cantidades de filas de cada tabla. A pesar de su simplicidad, es eficiente cuando la tabla interna es pequeña o está indexada.

### Join con índice (Index Nested Loop Join):

Cuando la tabla interna tiene un índice sobre la columna de join, se puede reemplazar el escaneo completo de la tabla interna por una búsqueda en el índice:

```
for each row outer in TableOuter:
    lookup(outer.joinKey) in InnerIndex  // B+ Tree lookup
    emit(concat(outer, matchedRows))
```

**Complejidad**: O(N * log(M)) en lugar de O(N * M), donde log(M) es el costo de búsqueda en el B+ tree.

### Uso del B+ Tree para optimización de consultas WHERE:

Actualmente, el motor SQL en la rama `chamba` resuelve cualquier consulta `WHERE columna = valor` mediante un escaneo lineal de todas las filas. Si existe un índice B+ tree sobre esa columna, se puede reemplazar el full scan por una búsqueda puntual en el árbol:

```
Sin índice:  ScanOperator -> lee TODAS las filas -> filtra
Con índice:  IndexScanOperator -> B+ tree Find(key) -> lee SOLO la(s) fila(s) coincidente(s)
```

---

## Estado actual del repositorio:

### Índice B+ Tree implementado (`include/index/bplus_tree.h`, `src/index/bplus_tree.cpp`):

La rama `chamba` incluye un B+ tree completamente funcional con las siguientes características:

| Característica | Estado |
|---|---|
| Inserción con split | Implementado (`InsertRecursive`, `SplitLeaf`, `SplitInternal`) |
| Búsqueda puntual | Implementado (`Find(key, outPageId, outSlot)`) |
| Búsqueda por rango | Implementado (`Range(minKey, maxKey, outResults)`) |
| Tipos de clave soportados | INT32, FLOAT, STRING (64 bytes fijos) |
| Persistencia de metadatos | Implementado (archivo `.meta` con `keyType` y `rootPageId`) |
| Integración con BufferPool | Implementado (usa su propio BufferPool de 16 frames) |
| Máximo de claves por nodo | 8 (`BTREE_MAX_KEYS = 8`) |
| Eliminación | No implementado |

### Motor SQL actual (`src/sql/sql_engine.cpp`):

- Soporta `SELECT columnas FROM tabla WHERE columna operador valor`.
- Resolución de consultas por escaneo lineal sobre `LoadedTable` (in-memory).
- **No utiliza** el B+ tree para optimizar búsquedas.
- **No soporta** JOIN entre tablas.

---

## Implementación del Nested Loop Join:

### Nuevo operador: `JoinOperator`

```cpp
class JoinOperator : public Operator {
    Operator* outer;                    // Operador izquierdo (tabla exterior)
    Operator* inner;                    // Operador derecho (tabla interior)
    int outerJoinCol;                   // Columna de join en tabla exterior
    int innerJoinCol;                   // Columna de join en tabla interior
    vector<string> outerRow;            // Fila actual de la tabla exterior
    bool outerExhausted;                // True si el outer ya terminó
    vector<string> combinedSchema;      // Schema combinado de ambas tablas

public:
    JoinOperator(Operator* outer, Operator* inner,
                 int outerCol, int innerCol,
                 const vector<string>& schema1, const vector<string>& schema2);
    void Open() override;
    bool Next(vector<string>& row) override;
    void Close() override;
    vector<string> GetSchema() const override;
};
```

### Algoritmo de Next() - Nested Loop Join:

```
Open():
    outer->Open()
    inner->Open()
    outerExhausted = false
    // Cargar primera fila del outer
    outerExhausted = !outer->Next(outerRow)

Next(row):
    while (!outerExhausted):
        // Reiniciar el inner para cada fila del outer
        inner->Close()
        inner->Open()
        
        vector<string> innerRow;
        while (inner->Next(innerRow)):
            if (innerRow[innerJoinCol] == outerRow[outerJoinCol]):
                // Coincidencia encontrada: concatenar ambas filas
                row = outerRow + innerRow
                return true
        
        // No hubo match para esta fila del outer, avanzar al siguiente
        outerExhausted = !outer->Next(outerRow)
    
    return false  // Se agotaron todas las combinaciones
```

### Ejemplo de consulta con JOIN:

```sql
SELECT Name, Fare, Cabin FROM titanic WHERE Pclass = 1;
```

En una segunda tabla de pasajeros con información adicional:

```sql
SELECT t.Name, i.Info FROM titanic t JOIN info_passenger i ON t.PassengerId = i.PassengerId;
```

### Ejecución visual del plan:

```
JoinOperator(Name, Info)
    ├── ProjectOperator(t.Name, i.Info)
    │   └── SelectOperator(Pclass = 1)
    │       └── ScanOperator(titanic)
    └── ScanOperator(info_passenger)
```

---

## Optimización con B+ Tree Index:

### Nuevo operador: `IndexScanOperator`

Reemplaza al `ScanOperator` + `SelectOperator` cuando existe un índice B+ tree sobre la columna del WHERE.

```cpp
class IndexScanOperator : public Operator {
    BPlusTree& index;           // Árbol B+ sobre la columna indexada
    RecordManager& rm;          // Para leer registros del disco
    Schema& schema;
    BTreeKey searchKey;         // Clave a buscar
    CmpOp op;                   // Operador de comparación

public:
    IndexScanOperator(BPlusTree& index, RecordManager& rm,
                      Schema& schema, const BTreeKey& key, CmpOp op);
    void Open() override;
    bool Next(vector<string>& row) override;
    void Close() override;
};
```

### Estrategia de uso según el operador WHERE:

| Operador WHERE | Estrategia |
|---|---|
| `columna = valor` | `BPlusTree::Find(key)` -> lectura directa de 1 registro |
| `columna > valor` | `BPlusTree::Range(minKey, maxKey)` -> lectura de registros en rango |
| `columna < valor` | `BPlusTree::Range(minKey, maxKey)` con maxKey = infinito |
| `columna >= valor` | `BPlusTree::Range(valor, maxKey)` |
| `columna <= valor` | `BPlusTree::Range(minKey, valor)` |
| Sin índice disponible | Fallback a `ScanOperator` (escaneo lineal) |

### Ejemplo: consulta con optimización por índice:

```
Consulta: SELECT Name, Age FROM titanic WHERE PassengerId = 42

Sin índice (actual):
  ScanOperator -> recorre 891 filas -> filtra -> proyecta
  Costo: 891 lecturas

Con B+ tree Index:
  IndexScanOperator -> BPlusTree::Find(42) -> 2-3 accesos a nodo -> 1 lectura de registro
  Costo: ~4 accesos a disco (profundidad del árbol + 1 registro)
```

### Integración en el Plan Builder:

```cpp
// En ExecuteAndPrintQuery, antes de construir el plan:
BPlusTree* idx = catalog.GetIndex(table, whereColumn);
if (idx && whereOp == CmpOp::EQ) {
    // Usar IndexScan
    BTreeKey key = BTreeKeyFromInt32(whereValue, 0);
    plan = new IndexScanOperator(*idx, rm, schema, key, whereOp);
} else if (idx && (whereOp == CmpOp::GT || whereOp == CmpOp::GE || whereOp == CmpOp::LT || whereOp == CmpOp::LE)) {
    // Usar IndexRangeScan
    plan = new IndexScanOperator(*idx, rm, schema, whereValue, whereOp);
} else {
    // Fallback: Scan + Select
    plan = new ScanOperator(rm, schema);
    if (hasWhere) plan = new SelectOperator(plan, whereIdx, whereOp, whereVal);
}
```

---

## Registro de consultas soportadas después de la implementación:

| Consulta | Tipo | Mecanismo |
|---|---|---|
| `SELECT * FROM t` | Full Scan | `ScanOperator` |
| `SELECT col FROM t WHERE col = val` | Punkt Lookup | `IndexScanOperator` (si hay índice) o `ScanOperator` + `SelectOperator` |
| `SELECT col FROM t WHERE col > val` | Range Scan | `IndexScanOperator` con Range o fallback a `ScanOperator` |
| `SELECT a.col, b.col FROM a JOIN b ON a.id = b.id` | Nested Loop Join | `JoinOperator` con dos `ScanOperator` o `IndexScanOperator` como hijos |
| `SELECT a.col, b.col FROM a JOIN b ON a.id = b.id WHERE a.col > val` | Join + Filter | `JoinOperator` con `SelectOperator` sobre el lado izquierdo |

---

## Evidencia de implementación:

### Archivos clave a crear/modificar:

| Archivo | Contenido |
|---|---|
| `include/sql/operators.h` | Agregar `JoinOperator` e `IndexScanOperator` a la jerarquía de operadores |
| `src/sql/operators.cpp` | Implementación de `JoinOperator::Next()` e `IndexScanOperator::Next()` |
| `src/sql/sql_engine.cpp` | Modificar plan builder para detectar JOIN y usar índices disponibles |
| `include/db/catalog.h` | Agregar `GetIndex(table, column)` para recuperar índices B+ tree |

### Salida esperada de demostración:

**Demo JOIN:**
```sql
SQL> SELECT t.Name, i.Cabin FROM titanic t JOIN titanic i ON t.PassengerId = i.PassengerId WHERE t.Pclass = 1;
```
```
+---------------------+--------+
| Name                | Cabin  |
+---------------------+--------+
| Cumings, Mrs. ...   | C85    |
| Futrelle, Mrs. ...  | C123   |
+---------------------+--------+
[Mostrando 2 de 216 filas que cumplen la condicion]
```

**Demo Index Scan:**
```sql
SQL> CREATE INDEX idx_age ON titanic(Age);  -- (índice creado manualmente)
SQL> SELECT Name, Age FROM titanic WHERE Age = 25;
```
```
+---------------------+-----+
| Name                | Age |
+---------------------+-----+
| Palsson, Miss. ...  |  25 |
+---------------------+-----+
[Usando Index Scan - 3 paginas leidas vs 891 con full scan]
```

---

## Análisis de rendimiento:

### Nested Loop Join vs Hash Join (referencia):

| Algoritmo | Complejidad | Memoria | Mejor caso |
|---|---|---|---|
| Nested Loop Join | O(N * M) | O(1) | Tabla interior pequeña o indexada |
| Hash Join | O(N + M) | O(N) | Tablas grandes, igualdad en condición |
| Sort-Merge Join | O(N log N + M log M) | O(N + M) | Datos ya ordenados |

El Nested Loop Join es la implementación correcta como primer algoritmo porque:
1. Es el más simple de implementar dentro del framework de operadores (Volcano Model).
2. Con un índice B+ tree sobre la columna de join, se convierte en Index Nested Loop Join con complejidad O(N * log(M)).
3. No requiere memoria adicional para construir tablas hash.

---

## Próximos pasos:

1. Implementar `JoinOperator` y `IndexScanOperator`.
2. Integrar la detección automática de JOINs en el parser SQL.
3. Vincular el catálogo de índices con el plan builder para decidir cuándo usar un índice.
4. Preparar la demostración comparativa de rendimiento con y sin índice.

---

## Notas finales:

- El B+ tree ya implementado en la rama `chamba` (`src/index/bplus_tree.cpp`) es la pieza clave para la optimización de consultas. Las operaciones `Find()` y `Range()` sobre el árbol permiten acelerar búsquedas puntuales y por rango respectivamente.
- El Nested Loop Join, aunque es el algoritmo más básico, es el más natural para integrar con el patrón de operadores del Volcano Model implementado en la Semana 14.
- La combinación de JOIN + Index Scan representa un salto significativo en las capacidades del motor SQL del proyecto.

Fecha: semana 15
