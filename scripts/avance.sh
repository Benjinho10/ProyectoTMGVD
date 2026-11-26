#!/usr/bin/env bash
# Ejecuta todos los experimentos de la presentación de avance:
#   NASA-HTTP (julio) y ClarkNet (28 ago - 3 sep), con bots inyectados,
#   k = 100, ventanas de 1 hora, barrido de memoria y gráficas.
#
# Uso: scripts/avance.sh
# Requiere haber corrido antes scripts/descargar_datos.sh.
# Resultados en resultados/nasa/ y resultados/clarknet/ (gráficas + tabla.md).
set -euo pipefail
cd "$(dirname "$0")/.."

K=100
VENTANA=3600
URLS_MIN=150   # los scrapers inyectados piden entre 150 y 1000 URLs distintas (> k)

make -s
make -s test | tail -1

correr() {   # correr NOMBRE LOG_ORIGINAL
  local nombre=$1 log=$2
  echo
  echo "=== $nombre ==="
  [[ -f "$log" ]] || { echo "Falta $log: ejecuta scripts/descargar_datos.sh"; exit 1; }

  echo "Inyectando bots..."
  python3 scripts/inyectar_bots.py --log "$log" --salida "data/${nombre}_iny.log" \
      --gt "data/${nombre}_gt.txt" --urls-min "$URLS_MIN" --ventana "$VENTANA"

  rm -f "resultados/${nombre}.csv"   # el barrido agrega filas: se parte de cero
  scripts/barrido_memoria.sh "data/${nombre}_iny.log" "data/${nombre}_gt.txt" \
      "$K" "$VENTANA" "resultados/${nombre}.csv"

  python3 scripts/graficar.py "resultados/${nombre}.csv" --vs exacto \
      --titulo "$nombre" --salida "resultados/${nombre}"
  python3 scripts/graficar.py "resultados/${nombre}.csv" --vs inyectado \
      --titulo "$nombre" --salida "resultados/${nombre}/vs_inyectado"
}

correr nasa     data/NASA_access_log_Jul95
correr clarknet data/clarknet_access_log_Aug28

echo
echo "Listo. Gráficas y tablas en resultados/nasa/ y resultados/clarknet/"
