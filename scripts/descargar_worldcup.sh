#!/usr/bin/env bash
# Descarga los logs de WorldCup98 (binarios .gz) a data/worldcup/.
#
# Uso:
#   scripts/descargar_worldcup.sh              # todos los días (5 a 92)
#   scripts/descargar_worldcup.sh 46 52        # solo los días 46 a 52
#   scripts/descargar_worldcup.sh 46 52 -y     # sin pedir confirmación
#
# Antes de bajar nada calcula el tamaño total, lo compara con el espacio
# libre y pide confirmación. Se puede cortar con Ctrl+C y volver a correr:
# retoma donde quedó (wget -c) y salta los archivos ya completos.
#
# Los .gz NO se descomprimen: el detector los lee directo con
#   zcat data/worldcup/wc_day46_*.gz | ./bin/detector --formato wc --log -
# (ver scripts/correr_worldcup.sh).
set -euo pipefail

BASE="${WC_BASE:-https://ita.ee.lbl.gov/traces/WorldCup}"
INI=${1:-5}
FIN=${2:-92}
SI=${3:-}
MARGEN=$((5 * 1024 * 1024 * 1024))   # deja al menos 5 GB libres

command -v wget >/dev/null || { echo "Falta wget: sudo apt install wget"; exit 1; }

DEST="$(cd "$(dirname "$0")/.." && pwd)/data/worldcup"
mkdir -p "$DEST"
cd "$DEST"

echo "Obteniendo la lista de archivos..."
wget -q -O indice.html "$BASE/"
grep -o 'wc_day[0-9]*_[0-9]*\.gz' indice.html | sort -u -V > todos.txt
rm -f indice.html

: > lista.txt
while read -r f; do
  d=${f#wc_day}; d=${d%%_*}
  if (( d >= INI && d <= FIN )); then echo "$f" >> lista.txt; fi
done < todos.txt
N=$(wc -l < lista.txt)
(( N > 0 )) || { echo "No hay archivos para los días $INI a $FIN"; exit 1; }

tamano_remoto() {
  wget --spider --server-response "$BASE/$1" 2>&1 \
    | awk 'tolower($1)=="content-length:" {print $2}' | tail -1 | tr -d '\r'
}

echo "Consultando el tamaño de $N archivos (días $INI a $FIN)..."
TOTAL=0; FALTA=0; i=0
: > tamanos.txt
while read -r f; do
  i=$((i + 1))
  s=$(tamano_remoto "$f"); s=${s:-0}
  echo "$f $s" >> tamanos.txt
  TOTAL=$((TOTAL + s))
  local_s=0; [[ -f "$f" ]] && local_s=$(stat -c %s "$f")
  (( s > local_s )) && FALTA=$((FALTA + s - local_s))
  printf "\r  %d/%d" "$i" "$N"
done < lista.txt
echo

gb() { awk -v b="$1" 'BEGIN {printf "%.2f GB", b / 1024^3}'; }
LIBRE=$(df --output=avail -B1 . | tail -1)

echo "Tamaño total (comprimido): $(gb "$TOTAL")"
echo "Falta por bajar:           $(gb "$FALTA")"
echo "Espacio libre en disco:    $(gb "$LIBRE")"

if (( FALTA + MARGEN > LIBRE )); then
  echo "No alcanza el espacio (se dejan 5 GB de margen). Baja menos días, por ejemplo:"
  echo "  scripts/descargar_worldcup.sh 46 60"
  exit 1
fi

if [[ "$SI" != "-y" ]]; then
  read -r -p "¿Descargar? [s/N] " r
  [[ "$r" == "s" || "$r" == "S" ]] || { echo "Cancelado."; exit 0; }
fi

i=0
while read -r f s; do
  i=$((i + 1))
  if [[ -f "$f" ]] && (( $(stat -c %s "$f") == s )); then continue; fi
  echo "[$i/$N] $f"
  wget -c -q --show-progress "$BASE/$f"
done < tamanos.txt

echo "Verificando los .gz..."
malos=0
while read -r f; do
  gzip -t "$f" 2>/dev/null || { echo "  dañado: $f (bórralo y vuelve a correr el script)"; malos=$((malos + 1)); }
done < lista.txt
rm -f todos.txt lista.txt tamanos.txt

if (( malos == 0 )); then
  echo "Listo: $N archivos en $DEST"
else
  echo "$malos archivo(s) dañado(s)."
  exit 1
fi
