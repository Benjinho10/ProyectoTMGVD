// detector.hpp — interfaz común de los métodos de detección.
//
// main.cpp corta el log en ventanas de tiempo. Para cada ventana llama:
//   inicio_ventana();
//   [pasada1(r) para cada registro]   <- solo si dos_pasadas() == true
//   pasada2(r) para cada registro
//   reportar()                         <- IPs marcadas como scrapers
#pragma once

#include <string>
#include <vector>

#include "parser.hpp"

class Detector {
public:
    virtual ~Detector() = default;

    virtual std::string nombre() const = 0;
    virtual bool dos_pasadas() const { return false; }

    virtual void inicio_ventana() = 0;
    virtual void pasada1(const Registro&) {}
    virtual void pasada2(const Registro& r) = 0;

    // IPs con más de k URLs distintas (estimadas) en la ventana actual.
    virtual std::vector<std::string> reportar() const = 0;

    // Bytes de las estructuras de datos (no el uso total del proceso).
    virtual size_t memoria_bytes() const = 0;

    // Información extra para el CSV (por ejemplo, cuántas IPs pasaron el filtro).
    virtual std::string detalle() const { return ""; }
};
