#include "io/csv_loader.h"

#include <fstream>
#include <iostream>
#include <filesystem>

#include "io/csv_reader.h"
#include "io/type_infer.h"
#include "io/schema_io.h"
#include "io/serializer.h"
#include "storage/record_manager.h"

namespace fs = std::filesystem;
namespace csvloader {

long long CountDataRows(const std::string& csvPath) {
    std::ifstream in(csvPath.c_str());
    if (!in) return 0;
    long long n = 0;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (first) { first = false; continue; } // encabezado
        ++n;
    }
    return n;
}

CsvLoadResult LoadCsvToStorage(const std::string& csvPath,
                               const std::string& tableName,
                               const std::string& tablesDir,
                               int sampleRows) {
    CsvLoadResult res;
    res.tableName = tableName;

    std::error_code ec;
    fs::create_directories(tablesDir, ec);

    // Sobrescribir: borra datos y esquema previos de la misma tabla.
    std::string binPath = tablesDir + "/" + tableName + ".bin";
    std::string walPath = binPath + ".wal";
    std::string schPath = schemaio::SchemaPath(tablesDir, tableName);
    fs::remove(binPath, ec);
    fs::remove(walPath, ec);
    fs::remove(schPath, ec);

    // 1) Encabezado + muestra para inferencia de tipos.
    std::ifstream in(csvPath.c_str());
    if (!in) { res.ok = false; return res; }

    std::string line;
    if (!std::getline(in, line) || line.empty()) { res.ok = false; return res; }
    std::vector<std::string> header = io::CsvReader_ReadAll_one(line);

    std::vector<std::vector<std::string>> sample;
    for (int i = 0; i < sampleRows && std::getline(in, line); ++i) {
        if (line.empty()) { --i; continue; }
        sample.push_back(io::CsvReader_ReadAll_one(line));
    }
    // NOTA: la muestra se lee justo despues del encabezado (sin rebobinar),
    // para no incluir la fila de nombres en la inferencia de tipos.

    // 2) Inferencia de tipos por columna.
    std::vector<ColumnType> types = typeinfer::InferColumnTypes(header, sample);

    // 3) Medir ancho maximo real de columnas de texto (streaming, 1 pasada).
    std::vector<int> widths = typeinfer::MeasureMaxWidths(csvPath, header.size(), true, types);

    // 4) Construir el esquema (ancho fijo).
    Schema schema;
    for (size_t i = 0; i < header.size(); ++i) {
        int len = 0;
        if (types[i] == ColumnType::STRING) {
            len = widths[i];
            if (len < 1) len = DEFAULT_VARCHAR_LEN; // minima cota por defecto
        }
        schema.AddColumn(header[i], types[i], len);
    }
    schema.Finalize();

    // Persistir el esquema (evita re-inferenciar al reiniciar).
    schemaio::SaveSchema(schPath, schema);
    res.schema = schema;

    // 5) Cargar filas al heap file en modo streaming (sin acumular en RAM).
    // Rebobinar al inicio del archivo para volver a leer todas las filas
    // (la carga salta la primera linea, que es el encabezado).
    in.clear();
    in.seekg(0, std::ios::beg);
    RecordManager rm(binPath, schema, ReplacementPolicy::LRU);
    long long count = 0;
    bool firstData = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (firstData) { firstData = false; continue; } // ya consumimos encabezado
        std::vector<std::string> cells = io::CsvReader_ReadAll_one(line);
        std::vector<unsigned char> bytes = RowToBytes(schema, cells);
        int pid = 0, slot = 0;
        if (!rm.InsertRecord(bytes, pid, slot)) {
            std::cerr << "csv_loader: fallo al insertar fila " << count << "\n";
            res.ok = false;
            return res;
        }
        ++count;
    }

    res.rowCount = count;
    res.ok = true;
    return res;
}

} // namespace csvloader
