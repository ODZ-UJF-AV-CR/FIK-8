import numpy as np
import pandas as pd
import sys
from matplotlib import pyplot as plt

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


class DATALOG:
    def __init__(self, lines, greeting):
        self.lines, self.greeting = lines, greeting
        self.headers = set()
        for l in self.lines:
            h = l.strip().split(",", 2)[0]
            if not h.startswith('$'):
                sys.stderr.write("Warning: In %s, skipping line: %s\n" % (filename, l.strip()))
                continue
            self.headers.add(h)
        self._std_series()
        '''
        with open(filename, 'r') as f:
            self._from_lines([l.strip() for l in f \
                              if l.strip() != ""])    
        '''

    @classmethod
    def split_runs(self, filename, header='$AIRDOS'):
        ret = []
        last_greeting = ""
        lines_accum = []
        
        with open(filename, 'r') as f:
            for l in f:
                l = l.strip()
                if l == "":
                    continue
                if l.startswith('$AIRDOS'):
                    if lines_accum:
                        ret.append(DATALOG(lines_accum, last_greeting))
                    lines_accum = []
                    last_greeting = l
                else:
                    lines_accum.append(l)
        
        if lines_accum:
            ret.append(DATALOG(lines_accum, last_greeting))
        return ret
    
    def series(self, header, fields=[]):
        last_msg = dict()
        series = []
        
        def get_fields(v, r, t):
            try:
                if isinstance(r, int):
                    return t(v[r])
                else:
                    return list(map(t, v[r[0]:r[1]]))
            except IndexError:
                return None
            
        
        for l in self.lines:
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

    def alt_for(self, m):
        alt = self.alt['Alt']
        return alt.reindex(alt.index.union(m.index)).interpolate(method='index', limit=5).reindex(m.index)


    def _std_series(self):
        if 'Error' in self.headers:
            sys.stderr.write('Error message seen')
        
        # compatible with FF
        if '$CANDY' in self.headers and '$AIRDOS,GF' in self.greeting:
            self.candy = self.series('$CANDY', [
                ('MeasNo', 1, try_int),
                ('Time', 2, try_float),
                ('Sync', 3, try_int),
                ('Supress', 4, try_int),
                ('Flux', 5, try_int),
                ('Offset', 6, try_int),
                ('Bins', (7, 7+252), try_int)
            ])
        
        if '$CANDY' in self.headers and '$AIRDOS,NF' in self.greeting:
            self.candy = self.series('$CANDY', [
                ('MeasNo', 1, try_int),
                ('Time', 2, try_float),
                ('Sync', 3, try_int),
                ('Pressure', 4, try_float),
                ('Temp', 5, try_float),
                ('Supress', 6, try_int),
                ('Flux', 7, try_int),
                ('Offset', 8, try_int),
                ('Bins', (9, 9+252), try_int)
            ])

        if '$GM' in self.headers:
            gm = self.series('$GM', [
                ('MeasNo', 1, try_int),
                ('Time', 2, try_float),
                ('GMCount', 3, try_int),
                ('Humid', 5, try_float),
                ('Temp', 4, try_float)
            ])
            self.gm = gm[gm['Time'] > 0]
        
        gnss_time_msg = '$GPRMC' if '$GPRMC' in self.headers else '$GNRMC'
        gnss_pos_msg = '$GPGGA' if '$GPGGA' in self.headers else '$GNGGA'
        
        t = self.series('$TIME', [
            ('LocalT', 1, try_float),
            ('GNSSTime', gnss_time_msg, 1, gprmc_time),
            ('GNSSDate', gnss_time_msg, 9, gprmc_date)
        ])
        t = t[np.logical_and(pd.notnull(t.GNSSTime), pd.notnull(t.GNSSDate))]
        if len(t) > 0 and '$TIME' in self.headers:
            start_ts = t.iloc[-1]['GNSSDate'] + t.iloc[-1]['GNSSTime']
            self.start_ts = start_ts - pd.Timedelta(seconds=t.iloc[-1]['LocalT'])
        else:
            #sys.stderr.write('No fix for run of %d lines at %s\n' % (len(self.lines), self.greeting))
            self.start_ts = None
        
        nav = self.series('$TIME', [
            ('Lat', gnss_pos_msg, 2, degmin),
            ('LatNS', gnss_pos_msg, 3, lambda k: -1 if k == "S" else 1),
            ('Lon', gnss_pos_msg, 4, degmin),
            ('LonEW', gnss_pos_msg, 5, lambda k: -1 if k == "W" else 1),
            ('Alt', gnss_pos_msg, 9, try_float),
            ('Time', 1, try_float)
        ])
        nav = pd.DataFrame({
            'Lat': nav['Lat'] * nav['LatNS'],
            'Lon': nav['Lon'] * nav['LonEW'],
            'Alt': nav['Alt'],
            'Time': nav['Time']
        })
        nav = nav[pd.notna(nav['Lat'])]
        self.nav = nav
    
        if self.start_ts is not None:
            self.nav.index = self.start_ts + pd.to_timedelta(self.nav['Time'], unit='sec')
        
        if self.start_ts is not None and '$GM' in self.headers:
            self.gm.index = self.start_ts + pd.to_timedelta(self.gm['Time'], unit='sec')
        
        if self.start_ts is not None and '$CANDY' in self.headers:
            self.candy.index = self.start_ts + pd.to_timedelta(self.candy['Time'], unit='sec')
