#include "io/interactive_cli.h"

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#include "sql/sql_completer.h"
#include "db/catalog.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#define ISATTY _isatty(_fileno(stdin))
#else
#include <unistd.h>
#include <termios.h>
#define ISATTY isatty(fileno(stdin))
#endif

namespace cli {

namespace {

// Enable VT100 / ANSI escape processing on Windows console
static void EnableVirtualTerminal() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

// Historial simple de consultas ejecutas en la sesión
static std::vector<std::string> gHistory;

} // namespace

std::string ReadLineWithAutoComplete(const std::string& prompt, const Catalog* catalog) {
    // Si no estamos en una terminal interactiva (ej. pipeline), usar std::getline
    if (!ISATTY) {
        std::string line;
        if (!std::getline(std::cin, line)) return "";
        return line;
    }

    EnableVirtualTerminal();

    std::string line;
    size_t cursor = 0;
    size_t selectedIndex = 0;
    int historyIndex = -1; // -1 significa escribiendo nueva consulta
    std::string tempHistoryLine;

    int lastRenderedPopupLines = 0;

    auto Redraw = [&](const std::vector<CompletionItem>& completions) {
        // Limpiar popup previo si existia
        for (int i = 0; i < lastRenderedPopupLines; ++i) {
            std::cout << "\n\x1b[K"; // nueva linea y limpiar
        }
        if (lastRenderedPopupLines > 0) {
            std::cout << "\x1b[" << lastRenderedPopupLines << "A"; // subir N lineas
        }

        // Ir al inicio del prompt
        std::cout << "\r\x1b[K" << prompt << line;

        // Mostrar inline ghost text si hay sugerencia activa
        std::string ghostText;
        if (!completions.empty() && selectedIndex < completions.size()) {
            const auto& item = completions[selectedIndex];
            if (item.replaceStart <= cursor) {
                std::string prefix = line.substr(item.replaceStart, cursor - item.replaceStart);
                if (prefix.size() < item.text.size()) {
                    ghostText = item.text.substr(prefix.size());
                }
            }
        }

        if (!ghostText.empty() && cursor == line.size()) {
            std::cout << "\x1b[90m" << ghostText << "\x1b[0m"; // Gris tenue
        }

        // Posicionar cursor real
        size_t totalCol = prompt.size() + cursor;
        std::cout << "\r\x1b[" << totalCol << "C";

        // Renderizar menu emergente debajo (hasta 5 elementos)
        int popupLines = 0;
        if (!completions.empty()) {
            const size_t maxShow = 5;
            size_t count = std::min(completions.size(), maxShow);

            std::cout << "\n";
            for (size_t i = 0; i < count; ++i) {
                const auto& item = completions[i];
                std::cout << "\x1b[K  ";

                if (i == selectedIndex) {
                    std::cout << "\x1b[7m"; // Invertir colores para seleccion
                }

                // Colorear segun categoria
                if (item.category == CompletionCategory::KEYWORD) {
                    std::cout << "\x1b[36m" << item.display << "\x1b[0m"; // Cyan
                } else if (item.category == CompletionCategory::TABLE) {
                    std::cout << "\x1b[32m" << item.display << "\x1b[0m"; // Verde
                } else if (item.category == CompletionCategory::COLUMN) {
                    std::cout << "\x1b[33m" << item.display << "\x1b[0m"; // Amarillo
                } else {
                    std::cout << item.display;
                }

                std::cout << " " << "\x1b[90m" << item.detail << "\x1b[0m";

                if (i == selectedIndex) {
                    std::cout << "\x1b[0m";
                }
                std::cout << "\n";
                popupLines++;
            }

            // Subir el cursor de vuelta al prompt
            std::cout << "\x1b[" << (popupLines + 1) << "A";
            std::cout << "\r\x1b[" << totalCol << "C";
        }
        std::cout.flush();

        lastRenderedPopupLines = popupLines;
    };

    std::vector<CompletionItem> completions = SqlCompleter::GetCompletions(line, cursor, catalog);
    Redraw(completions);

    while (true) {
#ifdef _WIN32
        int ch = _getch();
        if (ch == 0 || ch == 224) {
            // Tecla extendida en Windows (flechas, etc.)
            int ext = _getch();
            if (ext == 72) { // Flecha Arriba
                if (!completions.empty()) {
                    if (selectedIndex > 0) selectedIndex--;
                    else selectedIndex = completions.size() - 1;
                } else if (!gHistory.empty()) {
                    if (historyIndex == -1) {
                        tempHistoryLine = line;
                        historyIndex = (int)gHistory.size() - 1;
                    } else if (historyIndex > 0) {
                        historyIndex--;
                    }
                    if (historyIndex >= 0 && historyIndex < (int)gHistory.size()) {
                        line = gHistory[historyIndex];
                        cursor = line.size();
                    }
                }
            } else if (ext == 80) { // Flecha Abajo
                if (!completions.empty()) {
                    if (selectedIndex + 1 < completions.size()) selectedIndex++;
                    else selectedIndex = 0;
                } else if (historyIndex != -1) {
                    if (historyIndex + 1 < (int)gHistory.size()) {
                        historyIndex++;
                        line = gHistory[historyIndex];
                        cursor = line.size();
                    } else {
                        historyIndex = -1;
                        line = tempHistoryLine;
                        cursor = line.size();
                    }
                }
            } else if (ext == 75) { // Flecha Izquierda
                if (cursor > 0) cursor--;
            } else if (ext == 77) { // Flecha Derecha
                if (cursor < line.size()) {
                    cursor++;
                } else if (!completions.empty() && selectedIndex < completions.size()) {
                    // Completar con flecha derecha al final de linea
                    const auto& item = completions[selectedIndex];
                    line.replace(item.replaceStart, item.replaceLength, item.text);
                    cursor = item.replaceStart + item.text.size();
                    selectedIndex = 0;
                }
            }
        } else if (ch == 13 || ch == 10) { // ENTER
            // Limpiar popup antes de ejecutar
            for (int i = 0; i < lastRenderedPopupLines; ++i) {
                std::cout << "\n\x1b[K";
            }
            if (lastRenderedPopupLines > 0) {
                std::cout << "\x1b[" << lastRenderedPopupLines << "A";
            }
            std::cout << "\r\x1b[K" << prompt << line << "\n";
            std::cout.flush();

            if (!line.empty() && (gHistory.empty() || gHistory.back() != line)) {
                gHistory.push_back(line);
            }
            return line;
        } else if (ch == 9) { // TAB - Aplicar autocompletado
            if (!completions.empty() && selectedIndex < completions.size()) {
                const auto& item = completions[selectedIndex];
                line.replace(item.replaceStart, item.replaceLength, item.text);
                cursor = item.replaceStart + item.text.size();
                selectedIndex = 0;
            }
        } else if (ch == 8) { // BACKSPACE
            if (cursor > 0) {
                line.erase(cursor - 1, 1);
                cursor--;
                selectedIndex = 0;
            }
        } else if (ch == 27) { // ESCAPE
            completions.clear();
            selectedIndex = 0;
        } else if (ch >= 32 && ch <= 126) { // Caracter imprimible
            line.insert(cursor, 1, (char)ch);
            cursor++;
            selectedIndex = 0;
        }
#else
        // Fallback POSIX si no es Windows
        int ch = std::cin.get();
        if (ch == '\n' || ch == '\r') {
            std::cout << "\n";
            return line;
        } else if (ch == 9) { // TAB
            if (!completions.empty()) {
                const auto& item = completions[selectedIndex];
                line.replace(item.replaceStart, item.replaceLength, item.text);
                cursor = item.replaceStart + item.text.size();
            }
        } else if (ch == 127 || ch == 8) {
            if (!line.empty()) line.pop_back();
            cursor = line.size();
        } else if (ch >= 32 && ch <= 126) {
            line += (char)ch;
            cursor = line.size();
        }
#endif

        completions = SqlCompleter::GetCompletions(line, cursor, catalog);
        if (selectedIndex >= completions.size()) {
            selectedIndex = 0;
        }
        Redraw(completions);
    }
}

} // namespace cli
