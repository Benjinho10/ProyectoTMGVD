#!/usr/bin/env python3
"""Inyecta bots de comportamiento conocido en un log real (ground truth).

Tipos de bots inyectados:
  scraper     muchas URLs distintas dentro de UNA ventana (positivo si > k).
  recargador  muchas peticiones a 1-3 URLs (negativo: engaña a un contador
              de frecuencia, pero no a uno de URLs distintas).
  lento       muchas URLs distintas repartidas en varias ventanas, con pocas
              por ventana (negativo según la definición por ventana).

Las ventanas se alinean igual que en main.cpp: id = floor(ts / ventana).

Salida:
  - log mezclado, ordenado por tiempo (las líneas inyectadas van en UTC);
  - archivo gt con una línea por IP:  ip tipo urls_distintas inicio_ventana
    (para "lento" se anota el máximo de URLs distintas en una ventana).

Ejemplo:
  python3 scripts/inyectar_bots.py --log data/NASA_access_log_Jul95 \
      --salida data/nasa_jul_iny.log --gt data/nasa_jul_gt.txt \
      --scrapers 50 --urls-min 150 --urls-max 1000 --ventana 3600
"""
import argparse
import heapq
import random
import sys

from clf import formatear, ts_de_linea, url_de_linea


def leer_info(ruta, max_pool, rng):
    """Primera pasada: rango de tiempo y una muestra (reservoir) de URLs reales."""
    t_min, t_max, pool, vistas, n = None, None, [], set(), 0
    with open(ruta, encoding="latin-1", errors="replace") as f:
        for linea in f:
            ts = ts_de_linea(linea)
            if ts is None:
                continue
            t_min = ts if t_min is None else min(t_min, ts)
            t_max = ts if t_max is None else max(t_max, ts)
            url = url_de_linea(linea)
            if url is None or url in vistas:
                continue
            vistas.add(url)
            n += 1
            if len(pool) < max_pool:
                pool.append(url)
            else:
                j = rng.randrange(n)
                if j < max_pool:
                    pool[j] = url
            if len(vistas) > 5 * max_pool:  # acota la memoria del set
                vistas.clear()
    return t_min, t_max, pool


def elegir_urls(pool, n, rng, prefijo):
    if n <= len(pool):
        return rng.sample(pool, n)
    extra = [f"/{prefijo}/pagina_{i}.html" for i in range(n - len(pool))]
    return pool + extra


def generar(args, t_min, t_max, pool, rng):
    """Devuelve (lista de (ts, linea), lista de filas gt)."""
    v = args.ventana
    primera, ultima = t_min // v, t_max // v
    if ultima - primera < 1:
        sys.exit("El log abarca menos de dos ventanas; usa una ventana más pequeña.")
    eventos, gt = [], []

    for i in range(args.scrapers):
        ip = f"10.66.{i // 250}.{i % 250 + 1}"
        w = rng.randint(primera, ultima)
        n = rng.randint(args.urls_min, args.urls_max)
        urls = elegir_urls(pool, n, rng, "scrap")
        # Algunas URLs se repiten (un scraper real también re-pide páginas).
        pedidos = urls + rng.choices(urls, k=int(n * args.repeticion))
        for u in pedidos:
            ts = w * v + rng.randrange(v)
            eventos.append((ts, formatear(ip, u, ts)))
        gt.append((ip, "scraper", n, w * v))

    for i in range(args.recargadores):
        ip = f"10.77.{i // 250}.{i % 250 + 1}"
        w = rng.randint(primera, ultima)
        urls = rng.sample(pool, min(3, len(pool))) if pool else ["/index.html"]
        for _ in range(args.peticiones_recargador):
            ts = w * v + rng.randrange(v)
            eventos.append((ts, formatear(ip, rng.choice(urls), ts)))
        gt.append((ip, "recargador", len(urls), w * v))

    for i in range(args.lentos):
        ip = f"10.88.{i // 250}.{i % 250 + 1}"
        n_vent = min(args.ventanas_lento, ultima - primera + 1)
        inicio = rng.randint(primera, ultima - n_vent + 1)
        por_ventana = max(1, args.urls_min // 2)
        urls = elegir_urls(pool, por_ventana * n_vent, rng, "lento")
        for j in range(n_vent):
            for u in urls[j * por_ventana:(j + 1) * por_ventana]:
                ts = (inicio + j) * v + rng.randrange(v)
                eventos.append((ts, formatear(ip, u, ts)))
        gt.append((ip, "lento", por_ventana, inicio * v))

    eventos.sort(key=lambda e: e[0])
    return eventos, gt


def lineas_originales(ruta):
    """(ts, línea) del log original. Líneas sin timestamp heredan el anterior."""
    ultimo = 0
    with open(ruta, encoding="latin-1", errors="replace") as f:
        for linea in f:
            ts = ts_de_linea(linea)
            if ts is None:
                ts = ultimo
            ultimo = ts
            yield ts, linea


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--log", required=True)
    ap.add_argument("--salida", required=True)
    ap.add_argument("--gt", required=True)
    ap.add_argument("--ventana", type=int, default=3600, help="segundos (igual que en main.cpp)")
    ap.add_argument("--scrapers", type=int, default=50)
    ap.add_argument("--urls-min", type=int, default=150)
    ap.add_argument("--urls-max", type=int, default=1000)
    ap.add_argument("--repeticion", type=float, default=0.2,
                    help="fracción extra de peticiones repetidas por scraper")
    ap.add_argument("--recargadores", type=int, default=20)
    ap.add_argument("--peticiones-recargador", type=int, default=3000)
    ap.add_argument("--lentos", type=int, default=10)
    ap.add_argument("--ventanas-lento", type=int, default=12)
    ap.add_argument("--pool", type=int, default=200000, help="máximo de URLs reales a muestrear")
    ap.add_argument("--semilla", type=int, default=42)
    args = ap.parse_args()

    rng = random.Random(args.semilla)
    t_min, t_max, pool = leer_info(args.log, args.pool, rng)
    if t_min is None:
        sys.exit("No se encontraron líneas válidas en el log.")
    eventos, gt = generar(args, t_min, t_max, pool, rng)

    with open(args.salida, "w", encoding="latin-1", errors="replace") as out:
        for _, linea in heapq.merge(lineas_originales(args.log), eventos, key=lambda e: e[0]):
            out.write(linea)

    with open(args.gt, "w") as f:
        f.write("# ip tipo urls_distintas inicio_ventana\n")
        for fila in gt:
            f.write(" ".join(map(str, fila)) + "\n")

    print(f"{len(eventos)} líneas inyectadas ({args.scrapers} scrapers, {args.recargadores} recargadores, "
          f"{args.lentos} lentos) -> {args.salida}")
    print(f"ground truth -> {args.gt}")


if __name__ == "__main__":
    main()
