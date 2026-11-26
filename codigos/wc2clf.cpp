// wc2clf.cpp — convierte el binario de WorldCup98 a Common Log Format.
//
// Sirve para pasar un trozo de WorldCup98 por inyectar_bots.py (que trabaja
// con texto). Para correr el detector NO hace falta: lee el binario directo
// con --formato wc. Ojo: en texto el log ocupa ~4 veces más que en binario.
//
// Uso:  zcat data/worldcup/wc_day46_1.gz | ./bin/wc2clf > data/wc_day46_1.log
//       ./bin/wc2clf ARCHIVO_BINARIO > salida.log
#include <cstdio>
#include <iostream>
#include <string>

#include "parser.hpp"

int main(int argc, char** argv) {
    const std::string ruta = argc > 1 ? argv[1] : "-";
    LectorWorldCup lector(ruta);
    if (!lector.abierto()) {
        std::cerr << "Error: no se pudo abrir " << ruta << "\n";
        return 1;
    }
    Registro r;
    std::string linea;
    while (lector.siguiente(r)) {
        linea.clear();
        linea += r.ip;
        linea += " - - [";
        linea += formatear_timestamp(r.ts);
        linea += "] \"GET ";
        linea += r.url;
        linea += " HTTP/1.0\" 200 ";
        linea += std::to_string(lector.tamano());
        linea += '\n';
        std::fwrite(linea.data(), 1, linea.size(), stdout);
    }
    std::cerr << lector.lineas_leidas() << " registros convertidos\n";
    return 0;
}
