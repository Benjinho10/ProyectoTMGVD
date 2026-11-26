// hash.hpp — funciones hash de 64 bits usadas por todos los sketches.
//
// - murmur64: MurmurHash64A (Austin Appleby, dominio público) sobre strings.
// - mezclar64: finalizador de MurmurHash3 (fmix64), sirve para derivar
//   hashes "independientes" a partir de uno solo usando semillas distintas.
#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>

// Semillas fijas para que los resultados sean reproducibles.
constexpr uint64_t SEMILLA_IP  = 0x9747b28c1f2d3e4fULL;
constexpr uint64_t SEMILLA_URL = 0x5bd1e9955bd1e995ULL;

inline uint64_t mezclar64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

inline uint64_t murmur64(std::string_view s, uint64_t semilla = SEMILLA_IP) {
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;
    const size_t len = s.size();
    const unsigned char* datos = reinterpret_cast<const unsigned char*>(s.data());

    uint64_t h = semilla ^ (static_cast<uint64_t>(len) * m);

    const size_t bloques = len / 8;
    for (size_t i = 0; i < bloques; ++i) {
        uint64_t k;
        std::memcpy(&k, datos + 8 * i, 8);
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }

    const unsigned char* cola = datos + 8 * bloques;
    switch (len & 7) {
        case 7: h ^= static_cast<uint64_t>(cola[6]) << 48; [[fallthrough]];
        case 6: h ^= static_cast<uint64_t>(cola[5]) << 40; [[fallthrough]];
        case 5: h ^= static_cast<uint64_t>(cola[4]) << 32; [[fallthrough]];
        case 4: h ^= static_cast<uint64_t>(cola[3]) << 24; [[fallthrough]];
        case 3: h ^= static_cast<uint64_t>(cola[2]) << 16; [[fallthrough]];
        case 2: h ^= static_cast<uint64_t>(cola[1]) << 8;  [[fallthrough]];
        case 1: h ^= static_cast<uint64_t>(cola[0]);
                h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

// Combina dos hashes en uno (para el par (IP, URL) sin concatenar strings).
inline uint64_t combinar(uint64_t a, uint64_t b) {
    return mezclar64(a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2)));
}
