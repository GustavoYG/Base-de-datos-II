#pragma once

#include <vector>
#include <string>
#include <memory>

// Representa una tupla (fila) como un vector de valores representados en texto
struct Tuple {
    std::vector<std::string> values;
};

// Interfaz base del modelo Volcano (Iterator Model)
class Operator {
public:
    virtual ~Operator() = default;

    // Inicializa el operador (abre hijos, reinicia punteros)
    virtual void Open() = 0;

    // Obtiene la siguiente tupla de la ejecucion. Retorna true si obtuvo una tupla, false si finalizo (EOF)
    virtual bool Next(Tuple& outTuple) = 0;

    // Cierra el operador y libera recursos
    virtual void Close() = 0;

    // Retorna la lista de nombres de columnas que produce este operador
    virtual const std::vector<std::string>& GetOutputColumns() const = 0;
};
