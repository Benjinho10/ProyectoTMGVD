#!/usr/bin/env python3
"""Genera un log CLF totalmente sintético, con ground truth.

Sirve para probar el pipeline sin descargar nada y para los experimentos de
escalabilidad (tiempo vs. tamaño de datos). Los usuarios normales eligen URLs
con una distribución Zipf (pocas páginas muy populares); los bots se inyectan
igual que en inyectar_bots.py.

Ejemplo:
  python3 scripts/generar_sintetico.py --salida data/sint_1M.log \
      --gt data/sint_1M_gt.txt --peticiones 1000000 --usuarios 50000
"""
import argparse
import bisect
import itertools
import random

from clf import formatear


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--salida", required=True)
    ap.add_argument("--gt", required=True)
    ap.add_argument("--peticiones", type=int, default=200000, help="peticiones de usuarios normales")
    ap.add_argument("--usuarios", type=int, default=20000)
    ap.add_argument("--urls", type=int, default=20000, help="páginas del sitio")
    ap.add_argument("--zipf", type=float, default=1.1)
    ap.add_argument("--horas", type=int, default=24)
    ap.add_argument("--ventana", type=int, default=3600)
    ap.add_argument("--scrapers", type=int, default=30)
    ap.add_argument("--urls-min", type=int, default=150)
    ap.add_argument("--urls-max", type=int, default=1000)
    ap.add_argument("--recargadores", type=int, default=10)
    ap.add_argument("--peticiones-recargador", type=int, default=3000)
    ap.add_argument("--inicio", type=int, default=804556800, help="epoch inicial (01/Jul/1995)")
    ap.add_argument("--semilla", type=int, default=42)
    args = ap.parse_args()

    rng = random.Random(args.semilla)
    duracion = args.horas * 3600
    inicio = args.inicio - args.inicio % args.ventana
    urls = [f"/sitio/seccion{i % 50}/pagina{i}.html" for i in range(args.urls)]

    # Zipf: probabilidad proporcional a 1 / rango^s (CDF para muestrear).
    pesos = [1.0 / (r + 1) ** args.zipf for r in range(args.urls)]
    cdf = list(itertools.accumulate(pesos))
    total = cdf[-1]

    eventos = []
    for _ in range(args.peticiones):
        ip = f"user{rng.randrange(args.usuarios)}.ejemplo.cl"
        u = urls[bisect.bisect_left(cdf, rng.random() * total)]
        ts = inicio + rng.randrange(duracion)
        eventos.append((ts, ip, u))

    gt = []
    n_vent = duracion // args.ventana
    for i in range(args.scrapers):
        ip = f"10.66.{i // 250}.{i % 250 + 1}"
        w = rng.randrange(n_vent)
        n = rng.randint(args.urls_min, min(args.urls_max, args.urls))
        for u in rng.sample(urls, n):
            eventos.append((inicio + w * args.ventana + rng.randrange(args.ventana), ip, u))
        gt.append((ip, "scraper", n, inicio + w * args.ventana))
    for i in range(args.recargadores):
        ip = f"10.77.{i // 250}.{i % 250 + 1}"
        w = rng.randrange(n_vent)
        pocas = rng.sample(urls[:100], 3)
        for _ in range(args.peticiones_recargador):
            eventos.append((inicio + w * args.ventana + rng.randrange(args.ventana), ip, rng.choice(pocas)))
        gt.append((ip, "recargador", 3, inicio + w * args.ventana))

    eventos.sort(key=lambda e: e[0])
    with open(args.salida, "w") as f:
        for ts, ip, u in eventos:
            f.write(formatear(ip, u, ts))
    with open(args.gt, "w") as f:
        f.write("# ip tipo urls_distintas inicio_ventana\n")
        for fila in gt:
            f.write(" ".join(map(str, fila)) + "\n")
    print(f"{len(eventos)} líneas -> {args.salida}; ground truth -> {args.gt}")


if __name__ == "__main__":
    main()
