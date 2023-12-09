#! /usr/bin/env python

import sys
from collections import defaultdict

# todo
# interval based summary
# tall vs wide vs super wide output

def help():
    print("flatten.py input.csv")
    print("turns any rtl_power csv into a more compact summary")
    sys.exit()

if len(sys.argv) <= 1:
    help()

if len(sys.argv) > 2:
    help()

path = sys.argv[1]

sums = defaultdict(float)
counts = defaultdict(int)

def frange(start, stop, step):
    i = 0
    f = start
    while f <= stop:
        f = start + step*i
        yield f
        i += 1

for line in open(path):
    line = line.strip().split(', ')
    low = int(line[2])
    high = int(line[3])
    step = float(line[4])
    weight = int(line[5])
    dbm = [float(d) for d in line[6:]]
    for f,d in zip(frange(low, high, step), dbm):
        sums[f] += d*weight
        counts[f] += weight

ave = defaultdict(float)
for f in sums:
    ave[f] = sums[f] / counts[f]

for f in sorted(ave):
    print(','.join([str(f), str(ave[f])]))
    

