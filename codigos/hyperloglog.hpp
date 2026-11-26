// hyperloglog.hpp — HyperLogLog (Flajolet et al., 2007).
//
// m = 2^p registros de 1 byte. Los p bits altos del hash eligen el registro;
// en el resto se cuenta la posición del primer 1 (rho). Error estándar
// aproximado 1.04/sqrt(m): p=10 -> ~3.3 %, p=12 -> ~1.6 %.
// Incluye la corrección de rango pequeño (linear counting). Con hashes de
// 64 bits no hace falta la corrección de rango grande.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

class HyperLogLog {
public:
    explicit HyperLogLog(int p = 10) : p_(p), m_(1u << p), reg_(m_, 0) {
        if (p < 4 || p > 18) throw std::invalid_argument("HyperLogLog: p debe estar en [4, 18]");
    }

    // `h` es un hash de 64 bits del elemento (por ejemplo murmur64(url)).
    void add(uint64_t h) {
        const uint32_t idx = static_cast<uint32_t>(h >> (64 - p_));
        // Bits restantes, con un 1 centinela para que rho <= 64 - p + 1.
        const uint64_t resto = (h << p_) | (1ULL << (p_ - 1));
        const uint8_t rho = static_cast<uint8_t>(__builtin_clzll(resto) + 1);
        if (rho > reg_[idx]) reg_[idx] = rho;
    }

    double estimate() const {
        double suma = 0.0;
        uint32_t ceros = 0;
        for (uint8_t r : reg_) {
            suma += std::ldexp(1.0, -static_cast<int>(r));
            if (r == 0) ++ceros;
        }
        const double m = static_cast<double>(m_);
        const double e = alfa() * m * m / suma;
        if (e <= 2.5 * m && ceros > 0) return m * std::log(m / ceros);  // linear counting
        return e;
    }

    // Unión: máximo registro a registro. Requiere el mismo p.
    void merge(const HyperLogLog& otro) {
        if (p_ != otro.p_) throw std::invalid_argument("HyperLogLog::merge: p distinto");
        for (uint32_t i = 0; i < m_; ++i) reg_[i] = std::max(reg_[i], otro.reg_[i]);
    }

    void reset() { std::fill(reg_.begin(), reg_.end(), 0); }

    int p() const { return p_; }
    size_t memoria_bytes() const { return m_; }  // 1 byte por registro

private:
    double alfa() const {
        switch (m_) {
            case 16: return 0.673;
            case 32: return 0.697;
            case 64: return 0.709;
            default: return 0.7213 / (1.0 + 1.079 / m_);
        }
    }

    int p_;
    uint32_t m_;
    std::vector<uint8_t> reg_;
};
