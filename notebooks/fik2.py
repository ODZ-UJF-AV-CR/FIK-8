import numpy as np
import pandas as pd
import sys
from matplotlib import pyplot as plt

start_time = None

def try_float(v):
    try:
        return float(v)
    except ValueError:
        return float("NaN")

def try_int(v):
    try:
        return int(v)
    except ValueError:
        return -1

def gprmc_date(date):
    if date.strip() == "":
        return None
    
    date = int(date)
    
    return pd.Timestamp(
            year=2000+int(date % 100),
            month=int(date//1e2) % 100,
            day=int(date // 1e4),
            tz='UTC'
    )

def gprmc_time(time):
    if time.strip() == "":
        return None

    time = float(time)
    return pd.Timedelta(
        value = int(1000*(time%100)) \
            + 1000*60*(int(time//1e2) % 100) \
            + 1000*3600*int(time//1e4),
        unit = 'millisecond'
    )

def degmin(s):
    try:
        s = float(s)
    except ValueError:
        return float("NaN")
    
    return (s%100)/60 + int(s)//100


def sum_every_n(s, N):
    assert isinstance(s, pd.Series)
    a = np.array(s.array)
    a = np.sum(a[:len(a)//N*N].reshape((-1, N)), axis=1)
     
    return pd.Series(data=a, index=s.index.array[N//2:len(a)*N:N])


def parse_series(lines, header, fields=[]):
    last_msg = dict()
    series = []
    
    def get_fields(v, r, t):
        try:
            if isinstance(r, int):
                return t(v[r])
            else:
                return np.array(list(map(t, v[r[0]:r[1]])))
        except IndexError:
            return None
        
    
    for l in lines:
        h = l.strip().split(",", 2)[0]
        if h != header:
            last_msg[h] = l
            continue
        v = l.strip().split(",")
        
        series.append([
            get_fields(v, f[1], f[2]) if (isinstance(f[1], int) or isinstance(f[1], tuple)) \
                else get_fields(last_msg.get(f[1], "").split(","), f[2], f[3]) 
            for f in fields
        ])

    return pd.DataFrame(data=series,
                        columns=[f[0] for f in fields])


def read_gnss_lines(lines):
    headers = headers_present(lines)

    gnss_time_msg = '$GPRMC' if '$GPRMC' in headers else '$GNRMC'
    gnss_pos_msg = '$GPGGA' if '$GPGGA' in headers else '$GNGGA'

    t = parse_series(lines, '$TIME', [
        ('Time', 1, try_float),
        ('GNSSTime', gnss_time_msg, 1, gprmc_time),
        ('GNSSDate', gnss_time_msg, 9, gprmc_date)
    ]).dropna()

    if len(t) > 0 and '$TIME' in headers:
        start_ts = t.iloc[-1]['GNSSDate'] \
                    + t.iloc[-1]['GNSSTime'] \
                    - pd.Timedelta(seconds=t.iloc[-1]['Time'])
    else:
        start_ts = None

    nav = parse_series(lines, '$TIME', [
        ('Lat', gnss_pos_msg, 2, degmin),
        ('LatNS', gnss_pos_msg, 3, lambda k: -1 if k == "S" else 1),
        ('Lon', gnss_pos_msg, 4, degmin),
        ('LonEW', gnss_pos_msg, 5, lambda k: -1 if k == "W" else 1),
        ('Alt', gnss_pos_msg, 9, try_float),
        ('Time', 1, try_float),
        ('Speed', gnss_time_msg, 7, try_float)
    ])
    nav = pd.DataFrame({
        'Lat': nav['Lat'] * nav['LatNS'],
        'Lon': nav['Lon'] * nav['LonEW'],
        'Alt': nav['Alt'],
        'Time': nav['Time'],
        'Speed': nav['Speed']*1.852
    }).dropna()

    return (start_ts, nav)


# Přečte DATALOG.txt soubor a vrací list, ve kterém položky odpovídají jednomu běhu.
# Položka je pak tuple složený z
#     (a) první, identifikační řádky běhu a
#     (b) listu s dalšími řádkami běhu.
def read_datalog_lines(filename, greetingstart='$AIRDOS'):
    ret = []
    last_greeting = ""
    lines_accum = []

    with open(filename, 'r') as f:
        for l in f:
            l = l.strip()
            if l == "":
                continue
            if l.startswith(greetingstart):
                if lines_accum:
                    ret.append((last_greeting, lines_accum))
                lines_accum = []
                last_greeting = l
            else:
                lines_accum.append(l)
    
    if lines_accum:
        ret.append((last_greeting, lines_accum))

    return ret


def read_datalog_lines2(filename, greetingstart='$AIRDOS'):
    ret = []
    last_greeting = ""
    lines_accum = []

    with open(filename, 'r') as f:
        for l in f:
            l = l.strip()
            if l == "":
                continue
            if l.startswith(greetingstart):
                if lines_accum:
                    ret.append((last_greeting, lines_accum))
                lines_accum = []
                last_greeting = l
            else:
                lines_accum.append(l)
    
    if lines_accum:
        ret.append((last_greeting, lines_accum))

    return ret


# Vrátí množinu hlaviček, které se vyskytují v předaných řádkách
def headers_present(lines):
    ret = set()
    for l in lines:
        h = l.strip().split(",", 2)[0]
        if not h.startswith('$'):
            sys.stderr.write("Warning: In %s, skipping line: %s\n" % (filename, l.strip()))
            continue
        ret.add(h)
    return ret


# Zpracuje jeden běh v CF záznamu
def _read_airdos_cf_log(greeting, lines):
    assert '$AIRDOS,NF' in greeting

    global start_time

    headers = headers_present(lines)
    if 'Error' in headers:
        sys.stderr.write('Error message seen')

    candy = parse_series(lines, '$CANDY', [
        ('MeasNo', 1, try_int),
        ('Time', 2, try_float),
        ('Sync', 3, try_int),
        ('Pressure', 4, try_float),
        ('Temp', 5, try_float),
        ('Supress', 6, try_int),
        ('Flux', 7, try_int),
        ('Offset', 8, try_int),
        ('Bins', (9, 9+252), try_int)
    ]).dropna()

    start_ts, nav = read_gnss_lines(lines)
    start_time = start_ts

    if start_ts is not None:
        nav.index = start_ts + pd.to_timedelta(nav['Time'], unit='sec')
        candy.index = start_ts + pd.to_timedelta(candy['Time'], unit='sec')

    return (nav, candy)

def read_airdos_cf_log(filename, mergeruns=False):
    ret = [
        _read_airdos_cf_log(greeting, lines)
        for greeting, lines in read_datalog_lines(filename)
    ]

    # Převrátí uspořádání vnořených polí
    ret = tuple(zip(*ret))

    if mergeruns:
        return tuple(map(pd.concat, ret))
    else:
        return ret

def _read_airdos_ff_log(greeting, lines):
    assert '$AIRDOS,GF' in greeting

    headers = headers_present(lines)
    if 'Error' in headers:
        sys.stderr.write('Error message seen')

    candy = parse_series(lines, '$CANDY', [
        ('MeasNo', 1, try_int),
        ('Time', 2, try_float),
        ('Supress', 3, try_int),
        ('Flux', 4, try_int),
        ('Offset', 5, try_int),
        ('Bins', (7, 7+252), try_int)
    ]).dropna()

    start_ts, nav = read_gnss_lines(lines)
    start_ts = start_time

    if start_ts is not None:
        candy.index = start_ts + pd.to_timedelta(candy['Time'], unit='sec')
    else:
        sys.stderr.write('No fix for run of %d lines at %s\n' % (len(lines), greeting))

    return (candy, )

def read_airdos_ff_log(filename, mergeruns=False):
    ret = [
        _read_airdos_ff_log(greeting, lines)
        for greeting, lines in read_datalog_lines(filename)
    ]

    # Převrátí uspořádání vnořených polí
    ret = tuple(zip(*ret))

    if mergeruns:
        return tuple(map(pd.concat, ret))
    else:
        return ret

def _read_airdos_gm_log(greeting, lines):
    assert '$AIRDOS,GM' in greeting

    global start_time
    
    gm = parse_series(lines, '$GM', [
        ('MeasNo', 1, try_int),
        ('Time', 2, try_float),
        ('GMCount', 3, try_int),
        ('Humid', 5, try_float),
        ('Temp', 4, try_float)
    ]).dropna()
    gm = gm[gm['Time'] > 0]

    start_ts, nav = read_gnss_lines(lines)
    start_ts = start_time

    if start_ts is not None:
        nav.index = start_ts + pd.to_timedelta(nav['Time'], unit='sec')
        gm.index = start_ts + pd.to_timedelta(gm['Time'], unit='sec')
    else:
        sys.stderr.write('No fix for run of %d lines at %s\n' % (len(lines), greeting))

    return (nav, gm)

def read_airdos_gm_log(filename, mergeruns=False):
    ret = [
        _read_airdos_gm_log(greeting, lines)
        for greeting, lines in read_datalog_lines(filename)
    ]

    # Převrátí uspořádání vnořených polí
    ret = tuple(zip(*ret))

    if mergeruns:
        return tuple(map(pd.concat, ret))
    else:
        return ret

