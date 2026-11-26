#!/usr/bin/env python3
"""Gráficas y tabla a partir del CSV que escribe ./bin/detector --csv.

Genera en la carpeta de salida:
  memoria_vs_precision.png  F1, precision y recall vs. memoria (un panel por métrica).
                            La línea gris punteada marca la memoria del conteo exacto.
  tiempo_vs_memoria.png     tiempo de procesamiento vs. memoria.
  tiempo_vs_tamano.png      tiempo vs. registros (solo si el CSV tiene logs de distinto tamaño).
  tabla.md                  todos los resultados en una tabla Markdown (para el informe).

Ejemplo:
  python3 scripts/graficar.py resultados/nasa.csv --vs exacto --salida resultados/nasa
"""
import argparse
import csv
import os
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Colores en orden fijo (paleta validada para daltonismo) y un marcador distinto
# por serie, para que la identidad no dependa solo del color.
ESTILO = {
    "variante_a":      ("Variante A (CM + HLL)",     "#2a78d6", "o"),
    "variante_b_pb6":  ("Variante B, HLL p=6",       "#eb6834", "s"),
    "variante_b_pb8":  ("Variante B, HLL p=8",       "#1baf7a", "^"),
    "variante_b_pb10": ("Variante B, HLL p=10",      "#eda100", "D"),
    "hll_por_ip":      ("HLL por IP (sin filtro)",   "#e87ba4", "P"),
}
GRIS = "#52514e"
TEXTO = "#0b0b0b"


def estilo(metodo):
    return ESTILO.get(metodo, (metodo, GRIS, "x"))


def leer(ruta, vs, log=None):
    filas = []
    with open(ruta) as f:
        for r in csv.DictReader(f):
            if r["vs"] != vs or (log and r["log"] != log):
                continue
            for c in ("memoria_bytes", "tiempo_ms", "precision", "recall", "f1"):
                r[c] = float(r[c])
            r["registros"] = int(r["registros"])
            filas.append(r)
    return filas


def por_metodo(filas, x, y):
    series = defaultdict(list)
    for r in filas:
        series[r["metodo"]].append((r[x], r[y]))
    orden = [m for m in ESTILO if m in series] + [m for m in series if m not in ESTILO]
    return {m: sorted(series[m]) for m in orden}


def preparar_eje(eje):
    eje.set_xscale("log")
    eje.grid(alpha=0.25, linewidth=0.6)
    for lado in ("top", "right"):
        eje.spines[lado].set_visible(False)
    eje.tick_params(colors=GRIS, labelsize=9)


def linea_exacto(eje, mem_exacto, etiqueta=True):
    if mem_exacto is None:
        return
    eje.axvline(mem_exacto / 1024, color=GRIS, linestyle="--", linewidth=1.2, zorder=1)
    if etiqueta:
        eje.annotate(f"Exacto\n{mem_exacto / 1024:,.0f} KiB", xy=(mem_exacto / 1024, 0.03),
                     xytext=(-4, 0), textcoords="offset points", ha="right", va="bottom",
                     fontsize=8, color=GRIS)


def grafico_memoria(filas, mem_exacto, salida, titulo):
    fig, ejes = plt.subplots(1, 3, figsize=(15, 4.6), sharey=True)
    for eje, metrica in zip(ejes, ("f1", "precision", "recall")):
        for m, pts in por_metodo(filas, "memoria_bytes", metrica).items():
            nombre, color, marca = estilo(m)
            eje.plot([p[0] / 1024 for p in pts], [p[1] for p in pts], color=color, marker=marca,
                     markersize=7, linewidth=2, label=nombre, zorder=3)
        linea_exacto(eje, mem_exacto, etiqueta=(metrica == "f1"))
        preparar_eje(eje)
        eje.set_xlabel("Memoria (KiB, escala log)", color=TEXTO)
        eje.set_title(metrica.capitalize() if metrica != "f1" else "F1", color=TEXTO)
    ejes[0].set_ylabel("Valor", color=TEXTO)
    ejes[0].set_ylim(0, 1.05)
    manijas, etiquetas = ejes[0].get_legend_handles_labels()
    fig.legend(manijas, etiquetas, loc="lower center", ncol=len(etiquetas), frameon=False, fontsize=9)
    fig.suptitle(titulo, color=TEXTO)
    fig.tight_layout(rect=(0, 0.08, 1, 1))
    ruta = os.path.join(salida, "memoria_vs_precision.png")
    fig.savefig(ruta, dpi=150)
    plt.close(fig)
    print("->", ruta)


def grafico_tiempo_memoria(filas, exacto, salida, titulo):
    fig, eje = plt.subplots(figsize=(7.5, 4.6))
    for m, pts in por_metodo(filas, "memoria_bytes", "tiempo_ms").items():
        nombre, color, marca = estilo(m)
        eje.plot([p[0] / 1024 for p in pts], [p[1] / 1000 for p in pts], color=color, marker=marca,
                 markersize=7, linewidth=2, label=nombre, zorder=3)
    if exacto:
        eje.plot([exacto["memoria_bytes"] / 1024], [exacto["tiempo_ms"] / 1000], color=GRIS, marker="*",
                 markersize=12, linestyle="none", label="Exacto", zorder=4)
    preparar_eje(eje)
    eje.set_xlabel("Memoria (KiB, escala log)", color=TEXTO)
    eje.set_ylabel("Tiempo de procesamiento (s)", color=TEXTO)
    eje.set_ylim(bottom=0)
    eje.set_title(titulo, color=TEXTO)
    eje.legend(fontsize=8, frameon=False)
    fig.tight_layout()
    ruta = os.path.join(salida, "tiempo_vs_memoria.png")
    fig.savefig(ruta, dpi=150)
    plt.close(fig)
    print("->", ruta)


def grafico_tiempo_tamano(filas, salida):
    if len({r["registros"] for r in filas}) < 2:
        return  # un solo log: no hay eje de tamaño que mostrar
    fig, eje = plt.subplots(figsize=(7.5, 4.6))
    for m, pts in por_metodo(filas, "registros", "tiempo_ms").items():
        nombre, color, marca = estilo(m) if m != "exacto" else ("Exacto", GRIS, "*")
        eje.plot([p[0] for p in pts], [p[1] / 1000 for p in pts], color=color, marker=marca,
                 markersize=7, linewidth=2, label=nombre)
    eje.grid(alpha=0.25, linewidth=0.6)
    eje.set_xlabel("Registros procesados", color=TEXTO)
    eje.set_ylabel("Tiempo (s)", color=TEXTO)
    eje.set_title("Tiempo vs. tamaño de datos", color=TEXTO)
    eje.legend(fontsize=8, frameon=False)
    fig.tight_layout()
    ruta = os.path.join(salida, "tiempo_vs_tamano.png")
    fig.savefig(ruta, dpi=150)
    plt.close(fig)
    print("->", ruta)


def tabla(filas, salida, vs):
    orden = {m: i for i, m in enumerate(["exacto", "hll_por_ip", *ESTILO])}
    filas = sorted(filas, key=lambda r: (orden.get(r["metodo"], 99), r["memoria_bytes"]))
    lineas = [f"Referencia: **{vs}**\n",
              "| Método | Memoria (KiB) | Precision | Recall | F1 | Tiempo (s) | TP | FP | FN |",
              "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for r in filas:
        nombre = "Exacto" if r["metodo"] == "exacto" else estilo(r["metodo"])[0]
        lineas.append(f"| {nombre} | {r['memoria_bytes'] / 1024:,.1f} | {r['precision']:.3f} | "
                      f"{r['recall']:.3f} | {r['f1']:.3f} | {r['tiempo_ms'] / 1000:.2f} | "
                      f"{r['tp']} | {r['fp']} | {r['fn']} |")
    ruta = os.path.join(salida, "tabla.md")
    with open(ruta, "w") as f:
        f.write("\n".join(lineas) + "\n")
    print("->", ruta)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--vs", default="exacto", choices=["exacto", "inyectado"])
    ap.add_argument("--log", help="filtrar por un log (columna 'log' del CSV)")
    ap.add_argument("--titulo", default="", help="nombre del dataset para los títulos")
    ap.add_argument("--salida", default="resultados")
    args = ap.parse_args()

    os.makedirs(args.salida, exist_ok=True)
    filas = leer(args.csv, args.vs, args.log)
    if not filas:
        raise SystemExit("No hay filas que coincidan con los filtros.")
    exactos = [r for r in filas if r["metodo"] == "exacto"]
    exacto = exactos[-1] if exactos else None
    mem_exacto = exacto["memoria_bytes"] if exacto else None
    sketches = [r for r in filas if r["metodo"] != "exacto"]
    sufijo = f" — {args.titulo}" if args.titulo else ""

    grafico_memoria(sketches, mem_exacto, args.salida,
                    f"Precisión vs. memoria (referencia: {args.vs}){sufijo}")
    grafico_tiempo_memoria(sketches, exacto, args.salida, f"Tiempo vs. memoria{sufijo}")
    grafico_tiempo_tamano(filas, args.salida)
    tabla(filas, args.salida, args.vs)


if __name__ == "__main__":
    main()
