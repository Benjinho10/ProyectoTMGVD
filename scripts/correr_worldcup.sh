#!/usr/bin/env bash
# Corre el detector sobre WorldCup98 leyendo los .gz directo (sin descomprimir a disco).
#
# Uso: scripts/correr_worldcup.sh DIA_INI DIA_FIN [opciones del detector...]
#   scripts/correr_worldcup.sh 46 46 --k 1000 --metodo a,b
#   scripts/correr_worldcup.sh 5 92 --k 1000 --metodo a,b --csv resultados/wc.csv
#
# Los archivos se pasan en orden (día y parte) para que los timestamps queden
# ordenados. Ojo con la RAM: el detector guarda en memoria los registros de una
# ventana, y el baseline exacto siempre corre. En los días de partidos una hora
# puede tener millones de peticiones; si se queda corto, usa --ventana 600.
set -euo pipefail

INI=${1:?"falta DIA_INI"}
FIN=${2:?"falta DIA_FIN"}
shift 2

cd "$(dirname "$0")/.."
make -s bin/detector

ARCHIVOS=()
while read -r f; do
  d=$(basename "$f"); d=${d#wc_day}; d=${d%%_*}
  if (( d >= INI && d <= FIN )); then ARCHIVOS+=("$f"); fi
done < <(ls data/worldcup/wc_day*.gz 2>/dev/null | sort -V)

(( ${#ARCHIVOS[@]} > 0 )) || { echo "No hay archivos de los días $INI a $FIN en data/worldcup/"; exit 1; }
echo "Procesando ${#ARCHIVOS[@]} archivo(s)..." >&2

zcat "${ARCHIVOS[@]}" | ./bin/detector --formato wc --log - "$@"
