// metrics.hpp — precision, recall y F1 a partir de conjuntos de IPs.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

struct Confusion {
    uint64_t tp = 0, fp = 0, fn = 0;

    void agregar(const std::vector<std::string>& predichas,
                 const std::unordered_set<std::string>& reales) {
        std::unordered_set<std::string> pred(predichas.begin(), predichas.end());
        agregar(pred, reales);
    }

    void agregar(const std::unordered_set<std::string>& pred,
                 const std::unordered_set<std::string>& reales) {
        for (const auto& ip : pred) (reales.count(ip) ? tp : fp)++;
        for (const auto& ip : reales)
            if (!pred.count(ip)) fn++;
    }

    // Convención: si el denominador es 0 (no hubo predicciones / no hay
    // positivos reales) la métrica vale 1, porque no hubo errores.
    double precision() const { return (tp + fp) ? double(tp) / double(tp + fp) : 1.0; }
    double recall() const { return (tp + fn) ? double(tp) / double(tp + fn) : 1.0; }
    double f1() const {
        const double p = precision(), r = recall();
        return (p + r > 0) ? 2 * p * r / (p + r) : 0.0;
    }
};
