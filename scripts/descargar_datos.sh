#!/usr/bin/env bash
# Descarga y descomprime NASA-HTTP y ClarkNet desde el Internet Traffic Archive.
# Página de las trazas: https://ita.ee.lbl.gov/html/traces.html
# Si el FTP no responde, descarga los .gz a mano desde esa página y déjalos en data/.
set -euo pipefail

cd "$(dirname "$0")/../data"
BASE="ftp://ita.ee.lbl.gov/traces"

ARCHIVOS=(
  NASA_access_log_Jul95.gz
  NASA_access_log_Aug95.gz
  clarknet_access_log_Aug28.gz
  clarknet_access_log_Sep4.gz
)

for a in "${ARCHIVOS[@]}"; do
  if [[ -f "${a%.gz}" ]]; then
    echo "ya existe ${a%.gz}"
    continue
  fi
  [[ -f "$a" ]] || wget -c "$BASE/$a"
  gunzip -k "$a"
done

# WorldCup98 (formato binario, ~1.300 millones de peticiones):
#   https://ita.ee.lbl.gov/html/contrib/WorldCup.html
# Trae herramientas (recreate) para convertirlo a Common Log Format.
echo "Listo. Archivos en $(pwd)"
