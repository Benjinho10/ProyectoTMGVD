// parser.hpp — lectura de logs. Dos formatos:
//
//  - Common Log Format (NASA-HTTP, ClarkNet), una línea por petición:
//      host - - [01/Jul/1995:00:00:01 -0400] "GET /history/apollo/ HTTP/1.0" 200 6245
//    Las líneas mal formadas se saltan y se cuentan, sin detener la lectura.
//
//  - Binario de WorldCup98: registros de 20 bytes en big endian
//      timestamp, clientID, objectID, size (uint32) + method, status, type, server (uint8)
//    clientID hace de IP y objectID de URL (se guarda como "/obj/<id>").
//
// Ambos lectores aceptan "-" como ruta para leer de la entrada estándar, por
// ejemplo:  zcat wc_day46_*.gz | ./bin/detector --formato wc --log -
#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct Registro {
    std::string ip;   // primer campo (en NASA-HTTP suele ser un nombre de host)
    std::string url;  // segundo token de la petición entre comillas
    int64_t ts = 0;   // segundos desde epoch, en UTC (se aplica la zona horaria)
};

// Convierte "01/Jul/1995:00:00:01 -0400" a segundos epoch UTC.
bool parsear_timestamp(std::string_view s, int64_t& ts);

// Devuelve false si la línea no tiene el formato esperado.
bool parsear_linea(std::string_view linea, Registro& r);

// Formatea un timestamp epoch como "01/Jul/1995:04:00:01 +0000" (UTC).
std::string formatear_timestamp(int64_t ts);

// Interfaz común de los lectores.
class Fuente {
public:
    virtual ~Fuente() = default;
    virtual bool abierto() const = 0;
    // Lee el siguiente registro válido. Devuelve false al llegar al final.
    virtual bool siguiente(Registro& r) = 0;
    uint64_t lineas_leidas() const { return leidas_; }
    uint64_t lineas_invalidas() const { return invalidas_; }

protected:
    uint64_t leidas_ = 0;
    uint64_t invalidas_ = 0;
};

// Abre `ruta` como archivo, o la entrada estándar si ruta == "-".
class Entrada {
public:
    explicit Entrada(const std::string& ruta);
    std::istream* stream() const { return in_; }
    bool abierto() const { return in_ != nullptr && in_->good(); }

private:
    std::unique_ptr<std::ifstream> archivo_;
    std::istream* in_ = nullptr;
};

class LectorLog : public Fuente {
public:
    explicit LectorLog(const std::string& ruta) : entrada_(ruta) {}
    bool abierto() const override { return entrada_.abierto(); }
    bool siguiente(Registro& r) override;

private:
    Entrada entrada_;
    std::string linea_;
};

class LectorWorldCup : public Fuente {
public:
    static constexpr size_t TAM_REGISTRO = 20;

    explicit LectorWorldCup(const std::string& ruta) : entrada_(ruta), buffer_(TAM_REGISTRO * 65536) {}
    bool abierto() const override { return entrada_.abierto(); }
    bool siguiente(Registro& r) override;

    // Campos crudos del último registro leído (para wc2clf).
    uint32_t objeto() const { return objeto_; }
    uint32_t tamano() const { return tamano_; }

private:
    bool rellenar();

    Entrada entrada_;
    std::vector<char> buffer_;
    size_t pos_ = 0, fin_ = 0;
    uint32_t objeto_ = 0, tamano_ = 0;
};

// Crea el lector según el formato: "clf" o "wc". Devuelve nullptr si el formato no existe.
std::unique_ptr<Fuente> crear_fuente(const std::string& formato, const std::string& ruta);
