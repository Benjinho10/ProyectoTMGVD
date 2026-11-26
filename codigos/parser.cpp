#include "parser.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace {

bool leer_entero(std::string_view s, int& v) {
    if (s.empty()) return false;
    v = 0;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        v = v * 10 + (c - '0');
    }
    return true;
}

int numero_mes(std::string_view m) {
    static const char* nombres[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; ++i)
        if (m == nombres[i]) return i + 1;
    return 0;
}

// Días desde 1970-01-01 (algoritmo de Howard Hinnant, days_from_civil).
int64_t dias_desde_civil(int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

}  // namespace

bool parsear_timestamp(std::string_view s, int64_t& ts) {
    // dd/Mon/yyyy:HH:MM:SS [+-]zzzz   (la zona es opcional)
    if (s.size() < 20) return false;
    if (s[2] != '/' || s[6] != '/' || s[11] != ':' || s[14] != ':' || s[17] != ':') return false;

    int dia, anio, hora, min, seg;
    if (!leer_entero(s.substr(0, 2), dia) || !leer_entero(s.substr(7, 4), anio) ||
        !leer_entero(s.substr(12, 2), hora) || !leer_entero(s.substr(15, 2), min) ||
        !leer_entero(s.substr(18, 2), seg))
        return false;
    const int mes = numero_mes(s.substr(3, 3));
    if (mes == 0 || dia < 1 || dia > 31 || hora > 23 || min > 59 || seg > 60) return false;

    int64_t t = dias_desde_civil(anio, mes, dia) * 86400 + hora * 3600 + min * 60 + seg;

    if (s.size() >= 26 && s[20] == ' ' && (s[21] == '+' || s[21] == '-')) {
        int hh, mm;
        if (!leer_entero(s.substr(22, 2), hh) || !leer_entero(s.substr(24, 2), mm)) return false;
        const int offset = hh * 3600 + mm * 60;
        t += (s[21] == '+') ? -offset : offset;  // hora local -> UTC
    }
    ts = t;
    return true;
}

bool parsear_linea(std::string_view l, Registro& r) {
    while (!l.empty() && (l.back() == '\r' || l.back() == '\n')) l.remove_suffix(1);

    const size_t sp = l.find(' ');
    if (sp == std::string_view::npos || sp == 0) return false;

    const size_t a = l.find('[', sp);
    if (a == std::string_view::npos) return false;
    const size_t b = l.find(']', a);
    if (b == std::string_view::npos) return false;

    int64_t ts;
    if (!parsear_timestamp(l.substr(a + 1, b - a - 1), ts)) return false;

    const size_t q1 = l.find('"', b);
    if (q1 == std::string_view::npos) return false;
    const size_t q2 = l.find('"', q1 + 1);
    if (q2 == std::string_view::npos) return false;
    std::string_view peticion = l.substr(q1 + 1, q2 - q1 - 1);

    // Tokens de la petición: METODO URL PROTOCOLO. Algunas líneas antiguas
    // traen solo la URL ("GET /x" o "/x"), por eso se toleran 1 o 2 tokens.
    std::string_view tokens[3];
    int n = 0;
    size_t i = 0;
    while (i < peticion.size() && n < 3) {
        while (i < peticion.size() && peticion[i] == ' ') ++i;
        if (i >= peticion.size()) break;
        size_t j = peticion.find(' ', i);
        if (j == std::string_view::npos) j = peticion.size();
        tokens[n++] = peticion.substr(i, j - i);
        i = j;
    }
    if (n == 0) return false;
    std::string_view url = (n >= 2) ? tokens[1] : tokens[0];
    if (url.empty()) return false;

    r.ip.assign(l.data(), sp);
    r.url.assign(url.data(), url.size());
    r.ts = ts;
    return true;
}

std::string formatear_timestamp(int64_t ts) {
    static const char* nombres[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int64_t dias = ts / 86400, seg = ts % 86400;
    if (seg < 0) { seg += 86400; --dias; }
    // civil_from_days (Howard Hinnant)
    dias += 719468;
    const int64_t era = (dias >= 0 ? dias : dias - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(dias - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;
    const int64_t y = static_cast<int64_t>(yoe) + era * 400 + (m <= 2);

    char out[40];
    std::snprintf(out, sizeof out, "%02u/%s/%04lld:%02lld:%02lld:%02lld +0000", d, nombres[m - 1],
                  static_cast<long long>(y), static_cast<long long>(seg / 3600),
                  static_cast<long long>(seg / 60 % 60), static_cast<long long>(seg % 60));
    return out;
}

Entrada::Entrada(const std::string& ruta) {
    if (ruta == "-") {
        std::ios::sync_with_stdio(false);
        in_ = &std::cin;
    } else {
        archivo_ = std::make_unique<std::ifstream>(ruta, std::ios::binary);
        if (archivo_->is_open()) in_ = archivo_.get();
    }
}

bool LectorLog::siguiente(Registro& r) {
    std::istream* in = entrada_.stream();
    if (!in) return false;
    while (std::getline(*in, linea_)) {
        ++leidas_;
        if (parsear_linea(linea_, r)) return true;
        ++invalidas_;
    }
    return false;
}

namespace {
inline uint32_t leer_be32(const char* p) {
    const auto* u = reinterpret_cast<const unsigned char*>(p);
    return (uint32_t(u[0]) << 24) | (uint32_t(u[1]) << 16) | (uint32_t(u[2]) << 8) | uint32_t(u[3]);
}
}  // namespace

bool LectorWorldCup::rellenar() {
    std::istream* in = entrada_.stream();
    if (!in) return false;
    // Mueve al inicio los bytes sobrantes (un registro cortado entre lecturas).
    const size_t sobra = fin_ - pos_;
    if (sobra) std::memmove(buffer_.data(), buffer_.data() + pos_, sobra);
    pos_ = 0;
    fin_ = sobra;
    in->read(buffer_.data() + fin_, static_cast<std::streamsize>(buffer_.size() - fin_));
    fin_ += static_cast<size_t>(in->gcount());
    return fin_ - pos_ >= TAM_REGISTRO;
}

bool LectorWorldCup::siguiente(Registro& r) {
    if (fin_ - pos_ < TAM_REGISTRO && !rellenar()) {
        if (fin_ > pos_) { ++invalidas_; pos_ = fin_; }  // bytes sueltos al final
        return false;
    }
    const char* p = buffer_.data() + pos_;
    pos_ += TAM_REGISTRO;
    ++leidas_;
    r.ts = leer_be32(p);
    r.ip = std::to_string(leer_be32(p + 4));
    objeto_ = leer_be32(p + 8);
    tamano_ = leer_be32(p + 12);
    r.url = "/obj/" + std::to_string(objeto_);
    return true;
}

std::unique_ptr<Fuente> crear_fuente(const std::string& formato, const std::string& ruta) {
    if (formato == "clf") return std::make_unique<LectorLog>(ruta);
    if (formato == "wc") return std::make_unique<LectorWorldCup>(ruta);
    return nullptr;
}
