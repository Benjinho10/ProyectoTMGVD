// exact.hpp — baseline exacto: conjunto de URLs distintas por IP.
// Es la verdad de referencia (ground truth según la definición de scraper).
#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "detector.hpp"

class Exacto : public Detector {
public:
    explicit Exacto(uint64_t k) : k_(k) {}

    std::string nombre() const override { return "exacto"; }

    void inicio_ventana() override {
        max_ips_ = std::max(max_ips_, urls_por_ip_.size());
        urls_por_ip_.clear();
    }

    void pasada2(const Registro& r) override { urls_por_ip_[r.ip].insert(r.url); }

    std::vector<std::string> reportar() const override {
        std::vector<std::string> out;
        for (const auto& [ip, urls] : urls_por_ip_)
            if (urls.size() > k_) out.push_back(ip);
        return out;
    }

    // Aproximación: contenido de los strings + overhead típico por nodo de
    // unordered_set/map (~32 bytes en libstdc++ de 64 bits). No es exacta,
    // pero sirve para comparar órdenes de magnitud con los sketches.
    size_t memoria_bytes() const override {
        constexpr size_t NODO = 32;
        size_t total = 0;
        for (const auto& [ip, urls] : urls_por_ip_) {
            total += NODO + sizeof(std::string) + ip.size() + sizeof(urls);
            for (const auto& u : urls) total += NODO + sizeof(std::string) + u.size();
        }
        return total;
    }

    std::string detalle() const override {
        return "max_ips=" + std::to_string(std::max(max_ips_, urls_por_ip_.size()));
    }

private:
    uint64_t k_;
    size_t max_ips_ = 0;
    std::unordered_map<std::string, std::unordered_set<std::string>> urls_por_ip_;
};
