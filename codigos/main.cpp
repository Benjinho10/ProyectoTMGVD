// main.cpp — ejecuta los métodos de detección sobre un log y mide
// precision / recall / F1, memoria y tiempo.
//
// Uso:
//   ./bin/detector --log data/NASA_access_log_Jul95 --k 100 --ventana 3600
//                  --metodo todos --mem 262144 --d 4 --p 10 --pb 6
//                  [--gt data/gt.txt] [--csv resultados/resultados.csv]
//   zcat data/worldcup/wc_day46_*.gz | ./bin/detector --formato wc --log - --k 1000
#include <chrono>
#include <climits>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "detector.hpp"
#include "exact.hpp"
#include "hll_por_ip.hpp"
#include "metrics.hpp"
#include "parser.hpp"
#include "variante_a.hpp"
#include "variante_b.hpp"

namespace {

struct Config {
    std::string log;
    std::string formato = "clf";   // clf (texto) o wc (binario WorldCup98)
    std::string metodo = "todos";  // exacto,hll,a,b o "todos" (lista separada por comas)
    std::string gt;                // archivo de ground truth de la inyección (opcional)
    std::string csv;               // archivo CSV donde agregar resultados (opcional)
    uint64_t k = 100;              // umbral de URLs distintas
    int64_t ventana = 3600;        // segundos; 0 = todo el log en una ventana
    size_t mem = 256 * 1024;       // bytes para CM (variante A) o tabla (variante B)
    size_t d = 4;                  // filas de Count-Min
    int p = 10;                    // precisión de los HLL por IP
    int pb = 6;                    // precisión de los HLL por celda (variante B)
};

void uso(const char* prog) {
    std::cerr
        << "Uso: " << prog << " --log ARCHIVO|- [opciones]   (- = entrada estándar)\n"
        << "  --formato F      clf (texto, por defecto) o wc (binario WorldCup98)\n"
        << "  --metodo LISTA   exacto,hll,a,b o todos (por defecto: todos)\n"
        << "  --k N            umbral de URLs distintas (por defecto: 100)\n"
        << "  --ventana S      tamaño de ventana en segundos, 0 = sin ventanas (3600)\n"
        << "  --mem BYTES      memoria del Count-Min (A) o de la tabla (B) (262144)\n"
        << "  --d N            filas del Count-Min / tabla (4)\n"
        << "  --p N            precisión de los HLL por IP, 4..18 (10)\n"
        << "  --pb N           precisión de los HLL por celda en la variante B (6)\n"
        << "  --gt ARCHIVO     ground truth de la inyección sintética\n"
        << "  --csv ARCHIVO    agrega una fila por método a este CSV\n";
}

bool parsear_args(int argc, char** argv, Config& c) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto valor = [&](const char* nombre) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error(std::string("falta valor para ") + nombre);
            return argv[++i];
        };
        if (a == "--log") c.log = valor("--log");
        else if (a == "--formato") c.formato = valor("--formato");
        else if (a == "--metodo") c.metodo = valor("--metodo");
        else if (a == "--gt") c.gt = valor("--gt");
        else if (a == "--csv") c.csv = valor("--csv");
        else if (a == "--k") c.k = std::stoull(valor("--k"));
        else if (a == "--ventana") c.ventana = std::stoll(valor("--ventana"));
        else if (a == "--mem") c.mem = std::stoull(valor("--mem"));
        else if (a == "--d") c.d = std::stoull(valor("--d"));
        else if (a == "--p") c.p = std::stoi(valor("--p"));
        else if (a == "--pb") c.pb = std::stoi(valor("--pb"));
        else if (a == "-h" || a == "--help") return false;
        else throw std::runtime_error("argumento desconocido: " + a);
    }
    return !c.log.empty();
}

std::unordered_set<std::string> separar(const std::string& s) {
    std::unordered_set<std::string> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) if (!tok.empty()) out.insert(tok);
    return out;
}

// Formato del ground truth (una línea por IP inyectada):
//   ip tipo urls_distintas inicio_ventana
// Se consideran scrapers las de tipo "scraper" con urls_distintas > k.
std::unordered_set<std::string> leer_gt(const std::string& ruta, uint64_t k) {
    std::unordered_set<std::string> out;
    std::ifstream f(ruta);
    if (!f) throw std::runtime_error("no se pudo abrir el ground truth: " + ruta);
    std::string linea;
    while (std::getline(f, linea)) {
        if (linea.empty() || linea[0] == '#') continue;
        std::istringstream is(linea);
        std::string ip, tipo;
        uint64_t n = 0;
        if (!(is >> ip >> tipo >> n)) continue;
        if (tipo == "scraper" && n > k) out.insert(ip);
    }
    return out;
}

int64_t div_piso(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

struct Resultado {
    double tiempo_ms = 0;
    size_t memoria_max = 0;
    std::string detalle;
    Confusion vs_exacto;                       // por (ventana, IP)
    std::unordered_set<std::string> marcadas;  // unión de todas las ventanas
};

}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    try {
        if (!parsear_args(argc, argv, cfg)) {
            uso(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        uso(argv[0]);
        return 1;
    }

    // --- Métodos a ejecutar. El exacto siempre corre porque es la referencia.
    auto pedidos = separar(cfg.metodo);
    const bool todos = pedidos.count("todos") > 0;
    auto pedido = [&](const std::string& m) { return todos || pedidos.count(m) > 0; };

    std::vector<std::unique_ptr<Detector>> dets;
    dets.push_back(std::make_unique<Exacto>(cfg.k));
    const bool mostrar_exacto = pedido("exacto");
    try {
        if (pedido("hll")) dets.push_back(std::make_unique<HllPorIp>(cfg.k, cfg.p));
        if (pedido("a")) dets.push_back(std::make_unique<VarianteA>(cfg.k, cfg.mem, cfg.d, cfg.p));
        if (pedido("b")) dets.push_back(std::make_unique<VarianteB>(cfg.k, cfg.mem, cfg.d, cfg.pb));
    } catch (const std::exception& e) {
        std::cerr << "Error en los parámetros: " << e.what() << "\n";
        return 1;
    }

    std::unordered_set<std::string> gt;
    const bool hay_gt = !cfg.gt.empty();
    if (hay_gt) {
        try { gt = leer_gt(cfg.gt, cfg.k); }
        catch (const std::exception& e) { std::cerr << "Error: " << e.what() << "\n"; return 1; }
    }

    std::unique_ptr<Fuente> fuente = crear_fuente(cfg.formato, cfg.log);
    if (!fuente) {
        std::cerr << "Error: formato desconocido '" << cfg.formato << "' (usa clf o wc)\n";
        return 1;
    }
    Fuente& lector = *fuente;
    if (!lector.abierto()) {
        std::cerr << "Error: no se pudo abrir " << cfg.log << "\n";
        return 1;
    }

    std::vector<Resultado> res(dets.size());
    std::vector<Registro> buffer;  // registros de la ventana actual
    uint64_t n_registros = 0, n_ventanas = 0, fuera_de_orden = 0;

    auto procesar_ventana = [&]() {
        if (buffer.empty()) return;
        ++n_ventanas;
        std::unordered_set<std::string> verdad;
        for (size_t j = 0; j < dets.size(); ++j) {
            Detector& det = *dets[j];
            auto t0 = std::chrono::steady_clock::now();
            det.inicio_ventana();
            if (det.dos_pasadas())
                for (const auto& r : buffer) det.pasada1(r);
            for (const auto& r : buffer) det.pasada2(r);
            std::vector<std::string> pred = det.reportar();
            auto t1 = std::chrono::steady_clock::now();

            res[j].tiempo_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
            res[j].memoria_max = std::max(res[j].memoria_max, det.memoria_bytes());
            res[j].detalle = det.detalle();
            if (j == 0) verdad.insert(pred.begin(), pred.end());  // exacto
            res[j].vs_exacto.agregar(pred, verdad);
            res[j].marcadas.insert(pred.begin(), pred.end());
        }
        buffer.clear();
    };

    int64_t ventana_actual = LLONG_MIN;
    Registro r;
    while (lector.siguiente(r)) {
        ++n_registros;
        const int64_t id = cfg.ventana > 0 ? div_piso(r.ts, cfg.ventana) : 0;
        if (id > ventana_actual) {
            procesar_ventana();
            ventana_actual = id;
        } else if (id < ventana_actual) {
            ++fuera_de_orden;  // se asigna a la ventana actual
        }
        buffer.push_back(std::move(r));
    }
    procesar_ventana();

    // --- Resumen por pantalla
    std::printf("Log: %s\n", cfg.log.c_str());
    std::printf("Líneas: %llu  válidas: %llu  inválidas: %llu  fuera de orden: %llu  ventanas: %llu\n",
                (unsigned long long)lector.lineas_leidas(), (unsigned long long)n_registros,
                (unsigned long long)lector.lineas_invalidas(), (unsigned long long)fuera_de_orden,
                (unsigned long long)n_ventanas);
    std::printf("k=%llu  ventana=%llds  mem=%zu  d=%zu  p=%d  pb=%d\n\n", (unsigned long long)cfg.k,
                (long long)cfg.ventana, cfg.mem, cfg.d, cfg.p, cfg.pb);

    std::printf("%-12s %-10s %8s %8s %8s %6s %6s %6s %14s %10s\n", "metodo", "vs", "prec", "recall",
                "f1", "tp", "fp", "fn", "memoria(B)", "tiempo(ms)");

    std::ofstream csv;
    if (!cfg.csv.empty()) {
        std::ifstream prueba(cfg.csv);
        const bool nuevo = !prueba.good() || prueba.peek() == std::ifstream::traits_type::eof();
        prueba.close();
        csv.open(cfg.csv, std::ios::app);
        if (!csv) {
            std::cerr << "Error: no se pudo escribir " << cfg.csv << "\n";
            return 1;
        }
        if (nuevo)
            csv << "log,metodo,vs,k,ventana,mem_param,d,p,pb,registros,memoria_bytes,tiempo_ms,"
                   "tp,fp,fn,precision,recall,f1,detalle\n";
    }

    auto imprimir = [&](const std::string& metodo, const std::string& vs, const Confusion& c,
                        const Resultado& rs) {
        std::printf("%-12s %-10s %8.4f %8.4f %8.4f %6llu %6llu %6llu %14zu %10.1f\n", metodo.c_str(),
                    vs.c_str(), c.precision(), c.recall(), c.f1(), (unsigned long long)c.tp,
                    (unsigned long long)c.fp, (unsigned long long)c.fn, rs.memoria_max, rs.tiempo_ms);
        if (csv.is_open())
            csv << cfg.log << ',' << metodo << ',' << vs << ',' << cfg.k << ',' << cfg.ventana << ','
                << cfg.mem << ',' << cfg.d << ',' << cfg.p << ',' << cfg.pb << ',' << n_registros << ','
                << rs.memoria_max << ',' << rs.tiempo_ms << ',' << c.tp << ',' << c.fp << ',' << c.fn
                << ',' << c.precision() << ',' << c.recall() << ',' << c.f1() << ',' << rs.detalle
                << '\n';
    };

    for (size_t j = 0; j < dets.size(); ++j) {
        if (j == 0 && !mostrar_exacto) continue;
        const std::string m = dets[j]->nombre();
        imprimir(m, "exacto", res[j].vs_exacto, res[j]);
        if (hay_gt) {
            Confusion c;
            c.agregar(res[j].marcadas, gt);
            imprimir(m, "inyectado", c, res[j]);
        }
    }
    if (hay_gt)
        std::printf("\n(vs inyectado: a nivel de IP; %zu scrapers inyectados con > k URLs. Los scrapers "
                    "reales del log cuentan como FP.)\n",
                    gt.size());
    return 0;
}
