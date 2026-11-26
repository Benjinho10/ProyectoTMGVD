# Detección de bots y scrapers con Count-Min + HyperLogLog

Proyecto de *Tópicos en el Manejo de Grandes Volúmenes de Datos*.
Una IP es **scraper** si pide más de **k URLs distintas** en una ventana de tiempo.

## Estructura

```
codigos/
  hash.hpp          MurmurHash64A + mezclador (fmix64) para derivar hashes con semilla
  parser.hpp/.cpp   lee Common Log Format y entrega (ip, url, timestamp); salta líneas malas
  count_min.hpp     Count-Min Sketch: update, estimate, merge, memoria
  hyperloglog.hpp   HyperLogLog: add, estimate (con linear counting), merge
  detector.hpp      interfaz común de los métodos (una o dos pasadas por ventana)
  exact.hpp         baseline exacto: map<ip, set<url>>  (verdad de referencia)
  hll_por_ip.hpp    baseline: un HLL por IP, sin filtro
  variante_a.hpp    filtro Count-Min + HLL solo para IPs con > k peticiones (dos pasadas)
  variante_b.hpp    tabla Count-Min con HLL + IP candidata por celda (una pasada, tipo SpreadSketch)
  metrics.hpp       precision, recall, F1
  main.cpp          CLI: corta en ventanas, corre los métodos, imprime y guarda CSV
  wc2clf.cpp        convierte el binario de WorldCup98 a Common Log Format
tests/
  test_sketches.cpp pruebas del parser, CM, HLL y detección end-to-end
scripts/
  descargar_datos.sh   baja NASA-HTTP y ClarkNet a data/
  descargar_worldcup.sh baja WorldCup98 (todo o un rango de días), revisando el espacio libre
  correr_worldcup.sh   corre el detector sobre los .gz de WorldCup98 sin descomprimirlos
  inyectar_bots.py     inyecta scrapers / recargadores / lentos en un log real + ground truth
  generar_sintetico.py log 100 % sintético (Zipf) + ground truth, para pruebas y escalabilidad
  avance.sh            corre todos los experimentos de la presentación de avance
  barrido_memoria.sh   corre A (6 memorias) y B (6 memorias x pb = 6, 8, 10) -> CSV
  graficar.py          memoria vs. precision/recall/F1, tiempo vs. memoria y tabla.md
  clf.py               utilidades compartidas para leer/escribir CLF
data/        logs (no se versionan)
resultados/  CSV y gráficas
```

## Compilar y probar

```bash
make          # bin/detector y bin/wc2clf  (g++ -O2 -std=c++17)
make test     # bin/tests
```

## Uso rápido (sin descargar nada)

```bash
python3 scripts/generar_sintetico.py --salida data/sint.log --gt data/sint_gt.txt --peticiones 300000
./bin/detector --log data/sint.log --gt data/sint_gt.txt --k 100 --ventana 3600
```

## Descargar las trazas

Todas vienen del Internet Traffic Archive: <https://ita.ee.lbl.gov/html/traces.html>.
Los archivos están en <https://ita.ee.lbl.gov/traces/>. Usa los links **https**: los `ftp://`
que aparecen en las páginas antiguas ya no abren en los navegadores actuales.

| Traza | Peticiones | Descarga | En disco |
|---|---|---|---|
| NASA-HTTP (jul. y ago. 1995) | ~3,5 millones | ~42 MB (2 `.gz`) | ~370 MB en texto |
| ClarkNet (2 semanas, 1995) | ~3,3 millones | ~43 MB (2 `.gz`) | texto, similar a NASA |
| WorldCup98 (92 días, 1998) | ~1.352 millones | hasta ~12 GB (249 `.gz`) | se deja comprimido |

### Paso 0: preparar

```bash
cd ~/Escritorio/udec/VolumesDatos/Proyecto
chmod +x scripts/*.sh scripts/*.py
make && make test
```

Se necesita `wget` (`sudo apt install wget` si no está).

### Paso 1: NASA-HTTP y ClarkNet

```bash
scripts/descargar_datos.sh
```

Baja los 4 `.gz` a `data/` y los descomprime (deja también el `.gz`). Si ya existen, los salta.
Resultado:

```
data/NASA_access_log_Jul95       data/clarknet_access_log_Aug28
data/NASA_access_log_Aug95       data/clarknet_access_log_Sep4
```

**A mano**, si el script falla: abre <https://ita.ee.lbl.gov/traces/>, descarga esos 4
archivos `.gz`, muévelos a `data/` y ejecuta `gunzip -k data/*.gz`.

Para comprobar que quedó bien:

```bash
./bin/detector --log data/NASA_access_log_Jul95 --k 100 --ventana 3600
```

### Paso 2: WorldCup98

Son registros binarios de 20 bytes (unos 27 GB sin comprimir y más de 100 GB si se pasan a
texto). **No se descomprimen**: el detector lee los `.gz` directamente con `--formato wc`.

```bash
scripts/descargar_worldcup.sh            # los 92 días
scripts/descargar_worldcup.sh 46 52      # solo los días 46 a 52 (para empezar)
scripts/descargar_worldcup.sh 46 52 -y   # sin pedir confirmación
```

El script:

1. lee la lista de archivos desde <https://ita.ee.lbl.gov/traces/WorldCup/>;
2. consulta el tamaño de cada uno y muestra el total, lo que falta por bajar y el espacio libre;
3. se detiene si no quedarían al menos 5 GB libres;
4. pide confirmación y descarga a `data/worldcup/`;
5. revisa con `gzip -t` que ningún archivo esté dañado.

Si la descarga se corta (o la cortas con Ctrl+C), vuelve a ejecutar el mismo comando: retoma
donde quedó y salta los archivos completos. Los días 1 a 4 vienen vacíos; los días con
partidos (46 en adelante) son los que tienen más tráfico.

Información de la traza: <https://ita.ee.lbl.gov/html/contrib/WorldCup.html>.

## Experimentos

### Presentación de avance (todo de una vez)

```bash
scripts/avance.sh
```

Inyecta bots en NASA-HTTP (julio) y ClarkNet (28 ago - 3 sep), corre el barrido de memoria
con k = 100 y ventanas de 1 hora, y deja en `resultados/nasa/` y `resultados/clarknet/`:
`memoria_vs_precision.png`, `tiempo_vs_memoria.png` y `tabla.md` (y lo mismo contra los
bots inyectados en `vs_inyectado/`). Tarda un par de minutos.

### NASA-HTTP / ClarkNet con bots inyectados

```bash
python3 scripts/inyectar_bots.py --log data/NASA_access_log_Jul95 \
    --salida data/nasa_jul_iny.log --gt data/nasa_jul_gt.txt --scrapers 50 --urls-min 150
scripts/barrido_memoria.sh data/nasa_jul_iny.log data/nasa_jul_gt.txt 100 3600 resultados/nasa.csv
python3 scripts/graficar.py resultados/nasa.csv --vs exacto --salida resultados/
```

### WorldCup98

```bash
scripts/correr_worldcup.sh 46 46 --k 1000 --metodo a,b --csv resultados/wc.csv
```

En WorldCup98 `clientID` hace de IP y `objectID` de URL (`/obj/<id>`). Las IPs ya vienen
anonimizadas como enteros. El detector guarda en RAM las peticiones de una ventana y el
exacto siempre corre: si se queda corto de memoria en los días de partidos, usa `--ventana 600`.

Para inyectar bots en un trozo, primero conviértelo a texto:

```bash
zcat data/worldcup/wc_day46_1.gz | ./bin/wc2clf > data/wc_day46_1.log
python3 scripts/inyectar_bots.py --log data/wc_day46_1.log --salida data/wc46_iny.log --gt data/wc46_gt.txt
```

### Opciones de `bin/detector`

| Opción | Por defecto | Significado |
|---|---|---|
| `--log` | (obligatorio) | archivo de log, o `-` para leer de la entrada estándar |
| `--formato` | `clf` | `clf` (texto) o `wc` (binario WorldCup98) |
| `--metodo` | `todos` | `exacto`, `hll`, `a`, `b` separados por comas |
| `--k` | 100 | umbral de URLs distintas |
| `--ventana` | 3600 | segundos; `0` = todo el log como una ventana |
| `--mem` | 262144 | bytes del Count-Min (A) o de la tabla completa (B) |
| `--d` | 4 | filas del Count-Min / tabla |
| `--p` | 10 | precisión de los HLL por IP (m = 2^p registros) |
| `--pb` | 6 | precisión del HLL de cada celda en la variante B |
| `--gt` | — | ground truth de la inyección |
| `--csv` | — | agrega una fila por método y referencia |

## Cómo se evalúa

- **vs exacto**: por cada ventana se compara contra el baseline exacto (pares ventana-IP). Mide el error que introducen los sketches respecto de la definición.
- **vs inyectado**: a nivel de IP, contra los scrapers inyectados con más de k URLs. Las IPs reales del log que también cumplen la definición cuentan como falsos positivos, por eso conviene mirar ambas.
- **Memoria**: bytes de las estructuras (contadores, registros, claves guardadas), máximo entre ventanas. No es el uso total del proceso. La del exacto es una aproximación (strings + ~32 B por nodo).
- **Tiempo**: solo el procesamiento de cada método (sin la lectura del archivo), con `std::chrono`.

## Decisiones tomadas (revisables)

- **Hash**: MurmurHash64A; las d filas del Count-Min usan `fmix64(h ^ semilla_i)`.
- **HLL por IP**: se hashea la URL dentro del HLL de cada IP. En la variante B se hashea el par (IP, URL) con `combinar(h_ip, h_url)`, porque cada celda mezcla varias IPs.
- **Filtro de la variante A**: pasa una IP si el CM estima **más de k** peticiones. Como el CM nunca subestima, el filtro no genera falsos negativos.
- **Variante B**: la candidata de cada celda se reemplaza cuando el "nivel" del par (posición del primer 1 en los bits bajos del hash) es mayor o igual al guardado, como en SpreadSketch. La estimación de una candidata es el mínimo de los HLL de sus d celdas.
- **Ventanas**: fijas y alineadas a epoch (`id = floor(ts / ventana)`), con las estructuras reiniciadas en cada ventana. Los timestamps se pasan a UTC según su zona.
- **Dos pasadas**: los registros de una ventana se guardan en memoria para recorrerlos dos veces. Ese buffer no se cuenta como memoria del método.

## Pendiente (según la planificación)

- [ ] Correr con NASA-HTTP y ClarkNet y elegir k y ventana razonables.
- [ ] Count-Min con *conservative update* (opcional, mejora el filtro).
- [ ] Comparar con el código público de SpreadSketch.
- [ ] WorldCup98: experimentos de escala (lector binario y scripts ya listos).
