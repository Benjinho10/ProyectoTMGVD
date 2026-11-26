#!/usr/bin/env bash
# Corre los métodos con distintos presupuestos de memoria y agrega los
# resultados a un CSV, para luego graficar con scripts/graficar.py.
#
# Uso: scripts/barrido_memoria.sh LOG [GT] [K] [VENTANA] [CSV]
#
# - exacto y HLL por IP no dependen de --mem: se corren una vez.
# - Variante A: 6 presupuestos de memoria (4 KB a 4 MB).
# - Variante B: los mismos presupuestos, con HLL por celda de pb = 6, 8 y 10
#   (más registros por celda = menos error del HLL, pero menos columnas).
set -euo pipefail

LOG=${1:?"falta el log"}
GT=${2:-}
K=${3:-100}
VENTANA=${4:-3600}
CSV=${5:-resultados/resultados.csv}

cd "$(dirname "$0")/.."
make -s bin/detector
mkdir -p "$(dirname "$CSV")"

GT_ARG=()
[[ -n "$GT" ]] && GT_ARG=(--gt "$GT")
COMUN=(--log "$LOG" --k "$K" --ventana "$VENTANA" "${GT_ARG[@]}" --csv "$CSV")

echo "[1/3] exacto y HLL por IP"
./bin/detector "${COMUN[@]}" --metodo exacto,hll > /dev/null

MEMS=(4096 16384 65536 262144 1048576 4194304)
echo "[2/3] Variante A"
for MEM in "${MEMS[@]}"; do
  ./bin/detector "${COMUN[@]}" --metodo a --mem "$MEM" > /dev/null
done

echo "[3/3] Variante B"
for PB in 6 8 10; do
  for MEM in "${MEMS[@]}"; do
    ./bin/detector "${COMUN[@]}" --metodo b --mem "$MEM" --pb "$PB" > /dev/null
  done
done
echo "Resultados en $CSV"
