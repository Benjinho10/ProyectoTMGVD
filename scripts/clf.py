"""Utilidades compartidas para leer y escribir Common Log Format."""
import re
from datetime import datetime, timezone

MESES = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
_IDX_MES = {m: i + 1 for i, m in enumerate(MESES)}
_RE_TS = re.compile(r"\[(\d{2})/(\w{3})/(\d{4}):(\d{2}):(\d{2}):(\d{2})(?: ([+-])(\d{2})(\d{2}))?\]")
_RE_REQ = re.compile(r'"([^"]*)"')


def ts_de_linea(linea):
    """Timestamp epoch UTC de una línea CLF, o None si no se puede leer."""
    m = _RE_TS.search(linea)
    if not m:
        return None
    d, mes, a, hh, mm, ss, signo, oh, om = m.groups()
    if mes not in _IDX_MES:
        return None
    try:
        t = datetime(int(a), _IDX_MES[mes], int(d), int(hh), int(mm), int(ss),
                     tzinfo=timezone.utc).timestamp()
    except ValueError:
        return None
    if signo:
        off = int(oh) * 3600 + int(om) * 60
        t += -off if signo == "+" else off
    return int(t)


def url_de_linea(linea):
    m = _RE_REQ.search(linea)
    if not m:
        return None
    toks = m.group(1).split()
    if not toks:
        return None
    return toks[1] if len(toks) >= 2 else toks[0]


def formatear(ip, url, ts, status=200, bytes_=1024):
    """Línea CLF con el timestamp en UTC (+0000)."""
    t = datetime.fromtimestamp(ts, tz=timezone.utc)
    fecha = f"{t.day:02d}/{MESES[t.month - 1]}/{t.year}:{t.hour:02d}:{t.minute:02d}:{t.second:02d} +0000"
    return f'{ip} - - [{fecha}] "GET {url} HTTP/1.0" {status} {bytes_}\n'
