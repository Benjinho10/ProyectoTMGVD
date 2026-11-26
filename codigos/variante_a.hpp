// variante_a.hpp — Variante A: filtro Count-Min + HLL (dos pasadas).
//
// Pasada 1: Count-Min cuenta peticiones por IP.
// Pasada 2: solo las IPs con estimación > k reciben un HLL de URLs distintas.
//           (Una IP con <= k peticiones no puede tener > k URLs distintas, y
//           como el CM nunca subestima, el filtro no produce falsos negativos).
// Reporte:  IPs cuyo HLL estima > k URLs distintas.
#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>

#include "count_min.hpp"
#include "detector.hpp"
#include "hash.hpp"
#include "hyperloglog.hpp"

class VarianteA : public Detector {
public:
    // mem_cm: bytes para la tabla Count-Min; d: filas; p: precisión de cada HLL.
    VarianteA(uint64_t k, size_t mem_cm, size_t d, int p)
        : k_(k), p_(p), cm_(CountMin::desde_memoria(mem_cm, d)) {}

    std::string nombre() const override { return "variante_a"; }
    bool dos_pasadas() const override { return true; }

    void inicio_ventana() override {
        max_filtradas_ = std::max(max_filtradas_, hll_.size());
        cm_.reset();
        hll_.clear();
    }

    void pasada1(const Registro& r) override { cm_.update(murmur64(r.ip)); }

    void pasada2(const Registro& r) override {
        if (cm_.estimate(murmur64(r.ip)) <= k_) return;  // descartada por el filtro
        auto it = hll_.find(r.ip);
        if (it == hll_.end()) it = hll_.emplace(r.ip, HyperLogLog(p_)).first;
        it->second.add(murmur64(r.url, SEMILLA_URL));
    }

    std::vector<std::string> reportar() const override {
        std::vector<std::string> out;
        for (const auto& [ip, h] : hll_)
            if (h.estimate() > static_cast<double>(k_)) out.push_back(ip);
        return out;
    }

    size_t memoria_bytes() const override {
        size_t total = cm_.memoria_bytes();
        for (const auto& [ip, h] : hll_) total += h.memoria_bytes() + ip.size();
        return total;
    }

    std::string detalle() const override {
        // Máximo de IPs que pasaron el filtro en una misma ventana.
        return "w=" + std::to_string(cm_.w()) +
               ";max_ips_filtradas=" + std::to_string(std::max(max_filtradas_, hll_.size()));
    }

private:
    uint64_t k_;
    int p_;
    CountMin cm_;
    std::unordered_map<std::string, HyperLogLog> hll_;
    size_t max_filtradas_ = 0;
};
