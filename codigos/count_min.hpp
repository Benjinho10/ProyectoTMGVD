// count_min.hpp — Count-Min Sketch (Cormode y Muthukrishnan, 2005).
//
// Tabla de d filas x w columnas de contadores de 32 bits. Cada fila usa un
// hash distinto (misma función con semilla distinta). La estimación es el
// mínimo de los d contadores: nunca subestima, puede sobreestimar por
// colisiones. Error <= (e/w)·N con probabilidad >= 1 - e^{-d}.
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "hash.hpp"

class CountMin {
public:
    CountMin(size_t d, size_t w, uint64_t semilla = 1)
        : d_(d), w_(w), tabla_(d * w, 0), semillas_(d) {
        if (d == 0 || w == 0) throw std::invalid_argument("CountMin: d y w deben ser > 0");
        for (size_t i = 0; i < d_; ++i)
            semillas_[i] = mezclar64(semilla + (i + 1) * 0x9e3779b97f4a7c15ULL);
    }

    // Construye un CM que ocupa a lo más `bytes` bytes, con d filas.
    static CountMin desde_memoria(size_t bytes, size_t d, uint64_t semilla = 1) {
        size_t w = std::max<size_t>(1, bytes / (d * sizeof(uint32_t)));
        return CountMin(d, w, semilla);
    }

    // `h` es el hash de la clave (por ejemplo murmur64(ip)).
    void update(uint64_t h, uint32_t c = 1) {
        for (size_t i = 0; i < d_; ++i) {
            uint32_t& x = tabla_[i * w_ + columna(i, h)];
            const uint32_t max = std::numeric_limits<uint32_t>::max();
            x = (x > max - c) ? max : x + c;  // saturación, no desborda
        }
    }

    uint32_t estimate(uint64_t h) const {
        uint32_t minimo = std::numeric_limits<uint32_t>::max();
        for (size_t i = 0; i < d_; ++i)
            minimo = std::min(minimo, tabla_[i * w_ + columna(i, h)]);
        return minimo;
    }

    // Suma celda a celda. Solo válido si ambos tienen mismas d, w y semillas.
    void merge(const CountMin& otro) {
        if (d_ != otro.d_ || w_ != otro.w_ || semillas_ != otro.semillas_)
            throw std::invalid_argument("CountMin::merge: dimensiones o semillas distintas");
        for (size_t i = 0; i < tabla_.size(); ++i) {
            uint64_t s = static_cast<uint64_t>(tabla_[i]) + otro.tabla_[i];
            tabla_[i] = static_cast<uint32_t>(std::min<uint64_t>(s, std::numeric_limits<uint32_t>::max()));
        }
    }

    void reset() { std::fill(tabla_.begin(), tabla_.end(), 0); }

    size_t d() const { return d_; }
    size_t w() const { return w_; }
    size_t memoria_bytes() const { return tabla_.size() * sizeof(uint32_t); }

private:
    size_t columna(size_t fila, uint64_t h) const {
        return mezclar64(h ^ semillas_[fila]) % w_;
    }

    size_t d_, w_;
    std::vector<uint32_t> tabla_;
    std::vector<uint64_t> semillas_;
};
