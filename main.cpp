#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <iomanip>
#include <algorithm>
#include <windows.h>
using namespace std;

class DataB {
private:
    vector<vector<string>> data;

public:
    DataB(const string& filename, char delimiter = ',') {
        ifstream file(filename);
        if (file.is_open()) {
            string line;
            getline(file, line); // Leer la primera línea (encabezados)
            data.push_back(split(line, delimiter)); // Agregar encabezados al vector
            while (getline(file, line)) {
                data.push_back(split(line, delimiter));
            }
            file.close();
        }
    }

    vector<vector<string>> getData() const {
        return data;
    }

private:
    vector<string> split(const string& str, char delimiter) {
        vector<string> tokens;
        istringstream iss(str);
        string token;
        while (getline(iss, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }
};

class Esquemas : public DataB {
private:
    vector<vector<string>> dataTypes;

public:
    Esquemas(const string& filename, char delimiter = ',') : DataB(filename, delimiter) {
        const auto& data = getData();
        for (size_t i = 1; i < data.size(); i++) { // Comenzar desde la segunda fila (ignorar encabezados)
            const auto& row = data[i];
            vector<string> rowTypes;
            for (const auto& field : row) {
                rowTypes.push_back(getDataType(field));
            }
            dataTypes.push_back(rowTypes);
        }
    }

    vector<vector<string>> getDataTypes() const {
        return dataTypes;
    }

    void printData() const {
        const auto& data = getData();
        const auto& headers = data[0]; // Obtener los encabezados
        for (const auto& header : headers) {
            cout << header << "#";
        }
        cout << endl;

        for (size_t i = 1; i < data.size(); i++) { // Comenzar desde la segunda fila (ignorar encabezados)
            const auto& row = data[i];
            const auto& rowTypes = dataTypes[i - 1];
            for (size_t j = 0; j < row.size(); j++) {
                cout << row[j] << "#(" << rowTypes[j] << ")";
                if (j != row.size() - 1) {
                    cout << "#";
                }
            }
            cout << endl;
        }
    }
    // Función para calcular la capacidad total en bytes
    unsigned long long calculateTotalCapacity() {
        unsigned long long totalCapacity = 0;
        for (const auto& rowTypes : dataTypes) {
            for (const auto& fieldType : rowTypes) {
                // Supongamos algunos tamaños típicos para los tipos de datos
                totalCapacity += calculateFieldTypeSize(fieldType);
            }
        }
        return totalCapacity;
    }

    // Función para calcular el tamaño de un tipo de datos dado su nombre
    unsigned long long calculateFieldTypeSize(const string& fieldType) {
        // Supongamos algunos tamaños típicos para los tipos de datos
        if (fieldType == "int") {
            return sizeof(int);
        } else if (fieldType == "double") {
            return sizeof(double);
        } else {
            // Asumimos que el tipo de datos es string
            return fieldType.size(); // Tamaño de la cadena de caracteres
        }
    }

    // Función para calcular la capacidad utilizada en bytes
    unsigned long long calculateUsedCapacity() {
        return calculateTotalCapacity(); // Suponiendo que todos los datos están cargados en memoria
    }

    // Función para calcular el espacio libre en bytes
    unsigned long long calculateFreeSpace() {
        // Supongamos que el disco tiene un tamaño máximo de 1TB (1 terabyte)
        unsigned long long maxDiskSize = 1ULL * 1024 * 1024 * 1024 * 1024; // 1TB en bytes
        return maxDiskSize - calculateUsedCapacity();
    }

    // Función para mostrar la información del disco
    void showDiskInfo() {
        cout << "Capacidad total del disco duro en bytes: " << calculateTotalCapacity() << endl;
        cout << "Capacidad utilizada en bytes: " << calculateUsedCapacity() << endl;
        cout << "Espacio libre en bytes: " << calculateFreeSpace() << endl;
    }


    void saveSelectedData(const string& outputFilename) {
        const auto& data = getData();
        const auto& headers = data[0]; // Obtener los encabezados

        // Encontrar los índices de las columnas "Nombres", "Edad" y "Sexo"
        vector<int> selectedColumnIndices;
        for (size_t i = 0; i < headers.size(); i++) {
            if (headers[i] == "Nombres" || headers[i] == "Edad" || headers[i] == "Sexo") {
                selectedColumnIndices.push_back(i);
            }
        }

        // Guardar los datos seleccionados en un nuevo archivo
        ofstream outputFile(outputFilename);
        if (outputFile.is_open()) {
            for (size_t i = 1; i < data.size(); i++) { // Comenzar desde la segunda fila (ignorar encabezados)
                const auto& row = data[i];
                for (int columnIndex : selectedColumnIndices) {
                    outputFile << row[columnIndex] << "\t";
                }
                outputFile << endl;
            }
            outputFile.close();
            cout << "Los datos seleccionados se han guardado en el archivo: " << outputFilename << endl;
        } else {
            cout << "Error al abrir el archivo de salida." << endl;
        }
    }
    void saveColumnData(const string& columnName, const string& outputFilename) {
    const auto& data = getData();
    const auto& headers = data[0]; // Obtener los encabezados

    // Encontrar el índice de la columna
    int columnIndex = -1;
    for (size_t i = 0; i < headers.size(); i++) {
        if (headers[i] == columnName) {
            columnIndex = i;
            break;
        }
    }

    // Verificar si se encontró la columna
    if (columnIndex == -1) {
        cout << "La columna '" << columnName << "' no se encontró en el archivo." << endl;
        return;
    }

    // Encontrar el índice de la columna de ID (asumiendo que el nombre de la columna puede ser "ID", "id" o "Id")
    int idIndex = -1;
    for (size_t i = 0; i < headers.size(); i++) {
        string header = headers[i];
        transform(header.begin(), header.end(), header.begin(), ::tolower); // Convertir a minúsculas
        if (header == "Id" || header == "ID" || header == "id" ) {
            idIndex = i;
            break;
        }
    }

    // Guardar los datos de la columna en un nuevo archivo
    ofstream outputFile(outputFilename);
    if (outputFile.is_open()) {
        // Imprimir encabezados en el archivo de salida
        outputFile << "ID\t" << columnName << endl;

        // Imprimir datos en el archivo de salida
        for (size_t i = 0; i < data.size(); i++) {
            // Escapar caracteres especiales antes de escribir en el archivo
            string escapedName = data[i][columnIndex];
            for (char& c : escapedName) {
                if (c == '\n' || c == '\r' || c == '\t') {
                    c = ' '; // Reemplazar caracteres especiales con espacios
                }
            }
            if (idIndex != -1) {
                outputFile << data[i][idIndex] << "\t"; // Imprimir el ID
            } else {
                outputFile << "ID-NO-ENCONTRADO\t"; // Si no se encuentra la columna de ID
            }
            outputFile << escapedName << endl;
        }
        outputFile.close();
        cout << "Los datos de la columna '" << columnName << "' se han guardado en el archivo: " << outputFilename << endl;
    } else {
        cout << "Error al abrir el archivo de salida." << endl;
    }
}
void filterColumnData(const string& inputFilename, const string& outputFilename, char comparisonOperator, int threshold) {
    ifstream inputFile(inputFilename);
    ofstream outputFile(outputFilename);

    if (inputFile.is_open() && outputFile.is_open()) {
        string line;
        while (getline(inputFile, line)) {
            // Parsear la línea para obtener el valor a comparar
            // Suponemos que el ID está en la primera columna y el valor a comparar en la segunda columna
            string id, valueStr;
            int value;
            istringstream iss(line);
            getline(iss, id, '\t');
            getline(iss, valueStr, '\t');
            value = stoi(valueStr); // Convertir el valor a entero

            // Realizar la comparación según el operador proporcionado por el usuario
            bool includeRow = false;
            switch (comparisonOperator) {
                case '<':
                    includeRow = (value < threshold);
                    break;
                case '>':
                    includeRow = (value > threshold);
                    break;
                case '=':
                    includeRow = (value == threshold);
                    break;
                case '!':
                    includeRow = (value != threshold);
                    break;
                default:
                    cout << "Operador de comparación no válido." << endl;
                    break;
            }

            // Si la fila cumple con la condición, copiarla al archivo de salida
            if (includeRow) {
                outputFile << line << endl;
            }
        }

        inputFile.close();
        outputFile.close();
        cout << "Los datos filtrados se han guardado correctamente en el archivo: " << outputFilename << endl;
    } else {
        cout << "Error al abrir los archivos de entrada o salida." << endl;
    }
}
private:
    string getDataType(const string& field) {
        if (field.empty()) {
            return "null";
        }
        bool isNumeric = true;
        bool isInteger = true;
        for (char c : field) {
            if (!isdigit(c) && c != '-' && c != '.') {
                isNumeric = false;
                break;
            }
            if (!isdigit(c) && c != '-') {
                isInteger = false;
            }
        }
        if (isInteger) {
            return "int";
        }
        if (isNumeric) {
            return "float";
        }
        return "string";
    }
};

int main() {
    Esquemas schema("Titanic.csv");
    schema.printData();
    schema.showDiskInfo();

    string columnName;
    cout << "\nIngrese el nombre de la columna que desea guardar: ";
    getline(cin, columnName);

    schema.saveColumnData(columnName, columnName+".txt");
    schema.filterColumnData(columnName+".txt", "filtro.txt", '<', 5000);
    return 0;
}
