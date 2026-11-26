// Pruebas básicas: parser, Count-Min, HyperLogLog y detección end-to-end.
// Compilar y correr con:  make test
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "../codigos/count_min.hpp"
#include "../codigos/exact.hpp"
#include "../codigos/hash.hpp"
#include "../codigos/hll_por_ip.hpp"
#include "../codigos/hyperloglog.hpp"
#include "../codigos/parser.hpp"
#include "../codigos/variante_a.hpp"
#include "../codigos/variante_b.hpp"

static int fallas = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("  FALLA %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            ++fallas;                                                        \
        }                                                                    \
    } while (0)

static bool contiene(const std::vector<std::string>& v, const std::string& x) {
    for (const auto& s : v) if (s == x) return true;
    return false;
}

void test_parser() {
    std::printf("parser\n");
    Registro r;
    CHECK(parsear_linea("199.72.81.55 - - [01/Jul/1995:00:00:01 -0400] \"GET /history/apollo/ HTTP/1.0\" 200 6245", r));
    CHECK(r.ip == "199.72.81.55");
    CHECK(r.url == "/history/apollo/");
    CHECK(r.ts == 804571201);  // 1995-07-01 04:00:01 UTC

    CHECK(parsear_linea("host.com - - [01/Jul/1995:00:00:09 -0400] \"GET /shuttle/\" 200 -\r", r));
    CHECK(r.url == "/shuttle/");
    CHECK(parsear_linea("x - - [01/Jul/1995:00:00:09 -0400] \"/solo/url\" 200 -", r));
    CHECK(r.url == "/solo/url");

    CHECK(!parsear_linea("", r));
    CHECK(!parsear_linea("basura sin formato", r));
    CHECK(!parsear_linea("h - - [01/Xyz/1995:00:00:01 -0400] \"GET / HTTP/1.0\" 200 1", r));
    CHECK(!parsear_linea("h - - [01/Jul/1995:00:00:01 -0400] \"GET / HTTP/1.0 200 1", r));
    CHECK(!parsear_linea("h - - [01/Jul/1995:00:00:01 -0400] \"\" 200 1", r));
}

void test_count_min() {
    std::printf("count-min\n");
    CountMin cm(4, 1000);
    std::vector<uint32_t> real(5000, 0);
    std::mt19937 rng(1);
    for (int i = 0; i < 100000; ++i) {
        uint32_t x = rng() % 5000;
        real[x]++;
        cm.update(murmur64(std::to_string(x)));
    }
    bool nunca_subestima = true;
    for (uint32_t x = 0; x < 5000; ++x)
        if (cm.estimate(murmur64(std::to_string(x))) < real[x]) nunca_subestima = false;
    CHECK(nunca_subestima);
    CHECK(cm.memoria_bytes() == 4 * 1000 * sizeof(uint32_t));
}

void test_hll() {
    std::printf("hyperloglog\n");
    for (int n : {10, 1000, 100000}) {
        HyperLogLog h(12);
        for (int i = 0; i < n; ++i) h.add(murmur64("elem" + std::to_string(i)));
        for (int i = 0; i < n; ++i) h.add(murmur64("elem" + std::to_string(i)));  // repetidos
        double err = std::fabs(h.estimate() - n) / n;
        std::printf("  n=%d estimado=%.1f error=%.2f%%\n", n, h.estimate(), 100 * err);
        CHECK(err < 0.05);
    }
    HyperLogLog a(10), b(10);
    for (int i = 0; i < 5000; ++i) a.add(murmur64("x" + std::to_string(i)));
    for (int i = 2500; i < 7500; ++i) b.add(murmur64("x" + std::to_string(i)));
    a.merge(b);
    CHECK(std::fabs(a.estimate() - 7500) / 7500 < 0.1);
}

// 2000 usuarios normales (pocas URLs), 1 recargador (muchas peticiones, 1 URL)
// y 1 scraper (600 URLs distintas). Con k = 200 solo el scraper debe salir.
void test_end_to_end() {
    std::printf("end-to-end\n");
    std::vector<Registro> regs;
    std::mt19937 rng(7);
    for (int u = 0; u < 2000; ++u)
        for (int i = 0; i < 20; ++i)
            regs.push_back({"user" + std::to_string(u), "/pag/" + std::to_string(rng() % 50), 0});
    for (int i = 0; i < 5000; ++i) regs.push_back({"recargador", "/index.html", 0});
    for (int i = 0; i < 600; ++i) regs.push_back({"scraper", "/cat/" + std::to_string(i), 0});
    std::shuffle(regs.begin(), regs.end(), rng);

    const uint64_t k = 200;
    Exacto ex(k);
    HllPorIp hp(k, 10);
    VarianteA va(k, 64 * 1024, 4, 10);
    VarianteB vb(k, 512 * 1024, 4, 6);
    std::vector<Detector*> dets = {&ex, &hp, &va, &vb};
    for (Detector* d : dets) {
        d->inicio_ventana();
        if (d->dos_pasadas()) for (auto& r : regs) d->pasada1(r);
        for (auto& r : regs) d->pasada2(r);
        auto pred = d->reportar();
        std::printf("  %-11s reporta %zu IP(s), memoria %zu B  %s\n", d->nombre().c_str(), pred.size(),
                    d->memoria_bytes(), d->detalle().c_str());
        CHECK(contiene(pred, "scraper"));
        CHECK(!contiene(pred, "recargador"));
        CHECK(pred.size() == 1);
    }
}


void test_worldcup() {
    std::printf("worldcup binario\n");
    // Dos registros big endian de 20 bytes: (ts, cliente, objeto, tamaño, 4 bytes extra).
    auto be = [](std::string& s, uint32_t v) {
        for (int i = 3; i >= 0; --i) s.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
    };
    std::string bin;
    be(bin, 893971817); be(bin, 1); be(bin, 7); be(bin, 1000); bin.append(4, '\0');
    be(bin, 893971818); be(bin, 70000); be(bin, 8); be(bin, 0); bin.append(4, '\0');
    bin.append(3, 'x');  // bytes sueltos al final: se cuentan como inválidos
    const char* ruta = "bin/test_wc.bin";
    std::ofstream(ruta, std::ios::binary) << bin;

    LectorWorldCup lector(ruta);
    Registro r;
    CHECK(lector.siguiente(r));
    CHECK(r.ts == 893971817 && r.ip == "1" && r.url == "/obj/7" && lector.tamano() == 1000);
    CHECK(lector.siguiente(r));
    CHECK(r.ip == "70000" && r.url == "/obj/8");
    CHECK(!lector.siguiente(r));
    CHECK(lector.lineas_invalidas() == 1);

    // formatear_timestamp y parsear_timestamp son inversas.
    int64_t ts;
    CHECK(parsear_timestamp(formatear_timestamp(893971817), ts) && ts == 893971817);
    CHECK(formatear_timestamp(804571201) == "01/Jul/1995:04:00:01 +0000");
}

int main() {
    test_parser();
    test_worldcup();
    test_count_min();
    test_hll();
    test_end_to_end();
    if (fallas) {
        std::printf("\n%d prueba(s) fallaron\n", fallas);
        return EXIT_FAILURE;
    }
    std::printf("\nTodas las pruebas pasaron\n");
    return EXIT_SUCCESS;
}
