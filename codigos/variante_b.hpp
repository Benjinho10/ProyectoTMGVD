// variante_b.hpp — Variante B: tabla Count-Min con un HLL y una IP candidata
// por celda (estructura tipo SpreadSketch, una sola pasada).
//
// Cada celda guarda:
//   - un HLL pequeño con los pares (IP, URL) distintos que cayeron en ella,
//   - la IP candidata y su "nivel".
// Al procesar (ip, url): en cada fila i se va a la celda h_i(ip), se agrega
// hash(ip, url) al HLL y se calcula nivel = posición del primer 1 en otros
// bits de ese hash (geométrico). Si nivel >= nivel de la celda, la IP pasa a
// ser la candidata (regla de SpreadSketch: la IP con más pares distintos
// tiene más probabilidad de producir el nivel más alto).
// Consulta: para cada candidata, estimación = mínimo sobre las d filas del
// HLL de su celda (igual que Count-Min, nunca subestima salvo error del HLL).
#pragma once

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "detector.hpp"
#include "hash.hpp"
#include "hyperloglog.hpp"

class VarianteB : public Detector {
public:
    // mem: bytes totales para la tabla; d: filas; p: precisión del HLL de cada celda.
    VarianteB(uint64_t k, size_t mem, size_t d, int p) : k_(k), d_(d), p_(p) {
        w_ = std::max<size_t>(1, mem / (d_ * bytes_por_celda(p_)));
        tabla_.assign(d_ * w_, Celda{HyperLogLog(p_), std::string(), 0, false});
        semillas_.resize(d_);
        for (size_t i = 0; i < d_; ++i) semillas_[i] = mezclar64(0xB0B0 + (i + 1) * 0x9e3779b97f4a7c15ULL);
    }

    // Registros del HLL + 1 byte de nivel + ~16 bytes estimados para la clave.
    static size_t bytes_por_celda(int p) { return (size_t{1} << p) + 1 + 16; }

    std::string nombre() const override { return "variante_b_pb" + std::to_string(p_); }

    // Solo se reinician las celdas que se usaron en la ventana anterior: con
    // tablas grandes y ventanas con poco tráfico, recorrer toda la tabla en
    // cada ventana domina el tiempo.
    void inicio_ventana() override {
        for (uint32_t j : tocadas_) {
            Celda& c = tabla_[j];
            c.hll.reset();
            c.candidata.clear();
            c.nivel = 0;
            c.usada = false;
        }
        tocadas_.clear();
    }

    void pasada2(const Registro& r) override {
        const uint64_t h_ip = murmur64(r.ip);
        const uint64_t h_par = combinar(h_ip, murmur64(r.url, SEMILLA_URL));
        // Nivel con los bits bajos (el HLL usa los altos): ctz + 1.
        const uint8_t nivel = static_cast<uint8_t>(__builtin_ctzll(h_par | (1ULL << 63)) + 1);

        for (size_t i = 0; i < d_; ++i) {
            const size_t j = i * w_ + columna(i, h_ip);
            Celda& c = tabla_[j];
            if (!c.usada) {
                c.usada = true;
                tocadas_.push_back(static_cast<uint32_t>(j));
            }
            c.hll.add(h_par);
            if (nivel >= c.nivel) {
                if (c.candidata != r.ip) c.candidata = r.ip;
                c.nivel = nivel;
            }
        }
    }

    std::vector<std::string> reportar() const override {
        // Estimación de cada celda usada, calculada una sola vez. Las celdas
        // de una candidata siempre están usadas (ella misma pasó por ahí).
        std::unordered_map<uint32_t, double> est;
        est.reserve(tocadas_.size());
        for (uint32_t j : tocadas_) est[j] = tabla_[j].hll.estimate();

        std::unordered_set<std::string> vistas;
        std::vector<std::string> out;
        for (uint32_t jc : tocadas_) {
            const Celda& c = tabla_[jc];
            if (c.candidata.empty() || !vistas.insert(c.candidata).second) continue;
            const uint64_t h = murmur64(c.candidata);
            double minimo = std::numeric_limits<double>::max();
            for (size_t i = 0; i < d_; ++i) {
                auto it = est.find(static_cast<uint32_t>(i * w_ + columna(i, h)));
                minimo = std::min(minimo, it == est.end() ? 0.0 : it->second);
            }
            if (minimo > static_cast<double>(k_)) out.push_back(c.candidata);
        }
        return out;
    }

    // Memoria de la tabla: registros + nivel de todas las celdas (reservadas
    // aunque no se usen) + bytes de las candidatas guardadas.
    size_t memoria_bytes() const override {
        size_t total = tabla_.size() * (tabla_.front().hll.memoria_bytes() + 1);
        for (uint32_t j : tocadas_) total += tabla_[j].candidata.size();
        return total;
    }

    std::string detalle() const override {
        return "w=" + std::to_string(w_) + ";p=" + std::to_string(p_);
    }

private:
    struct Celda {
        HyperLogLog hll;
        std::string candidata;
        uint8_t nivel;
        bool usada;  // marca para reiniciar solo lo necesario (no cuenta como memoria del sketch)
    };

    size_t columna(size_t fila, uint64_t h) const { return mezclar64(h ^ semillas_[fila]) % w_; }

    uint64_t k_;
    size_t d_, w_;
    int p_;
    std::vector<Celda> tabla_;
    std::vector<uint64_t> semillas_;
    std::vector<uint32_t> tocadas_;  // celdas usadas en la ventana actual
};
