#pragma once

#include <string>

class Catalog;

namespace cli {

// Lee una linea desde la consola interactiva con auto-completado tipo IDE en tiempo real.
// Retorna la linea de texto cuando el usuario presiona ENTER.
// Si no hay consola TTY disponible, realiza fallback automatico a std::getline.
std::string ReadLineWithAutoComplete(const std::string& prompt, const Catalog* catalog);

} // namespace cli
