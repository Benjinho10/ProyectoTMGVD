// hll_por_ip.hpp — baseline: un HyperLogLog por cada IP, sin filtro previo.
#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>

#include "detector.hpp"
#include "hash.hpp"
#include "hyperloglog.hpp"

class HllPorIp : public Detector {
public:
    HllPorIp(uint64_t k, int p) : k_(k), p_(p) {}

    std::string nombre() const override { return "hll_por_ip"; }

    void inicio_ventana() override {
        max_ips_ = std::max(max_ips_, hll_.size());
        hll_.clear();
    }

    void pasada2(const Registro& r) override {
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

    // Registros de cada HLL + bytes de la clave (IP).
    size_t memoria_bytes() const override {
        size_t total = 0;
        for (const auto& [ip, h] : hll_) total += h.memoria_bytes() + ip.size();
        return total;
    }

    std::string detalle() const override {
        return "max_hlls=" + std::to_string(std::max(max_ips_, hll_.size()));
    }

private:
    uint64_t k_;
    int p_;
    size_t max_ips_ = 0;
    std::unordered_map<std::string, HyperLogLog> hll_;
};
