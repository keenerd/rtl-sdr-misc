#! /usr/bin/env python

from PIL import Image, ImageDraw, ImageFont
import sys, gzip, math, argparse, colorsys, datetime
from collections import defaultdict
from itertools import *

# todo:
# matplotlib powered --interactive
# arbitrary freq marker spacing
# pksato's timestamps
# ppm
# blue-less marker grid
# fast summary thing
# time-based slicing
# gain normalization

parser = argparse.ArgumentParser(description='Convert rtl_power CSV files into graphics.')
parser.add_argument('input_path', metavar='INPUT', type=str,
    help='Input CSV file. (may be a .csv.gz)')
parser.add_argument('output_path', metavar='OUTPUT', type=str,
    help='Output image. (various extensions supported)')
parser.add_argument('--offset', dest='offset_freq', default=None,
    help='Shift the entire frequency range, for up/down converters.')
parser.add_argument('--ytick', dest='time_tick', default=None,
    help='Place ticks along the Y axis every N seconds.')
slicegroup = parser.add_argument_group('Slicing',
    'Efficiently render a portion of the data. (optional)')
slicegroup.add_argument('--low', dest='low_freq', default=None,
    help='Minimum frequency for a subrange.')
slicegroup.add_argument('--high', dest='high_freq', default=None,
    help='Maximum frequency for a subrange.')
"""
slicegroup.add_argument('--begin', dest='begin_time', default=None,
    help='Timestamp to start at.')
slicegroup.add_argument('--end', dest='end_time', default=None,
    help='Timestamp to stop at.')
slicegroup.add_argument('--head', dest='head_time', default=None,
    help='Duration to use, starting at the beginning.')
slicegroup.add_argument('--tail', dest='tail_time', default=None,
    help='Duration to use, stopping at the end.')
"""
args = parser.parse_args()

def frange(start, stop, step):
    i = 0
    while (i*step + start <= stop):
        yield i*step + start
        i += 1

def min_filter(row):
    size = 3
    result = []
    for i in range(size):
        here = row[i]
        near = row[0:i] + row[i+1:size]
        if here > min(near):
            result.append(here)
            continue
        result.append(min(near))
    for i in range(size-1, len(row)):
        here = row[i]
        near = row[i-(size-1):i]
        if here > min(near):
            result.append(here)
            continue
        result.append(min(near))
    return result

def freq_parse(s):
    suffix = 1
    if s.lower().endswith('k'):
        suffix = 1e3
    if s.lower().endswith('m'):
        suffix = 1e6
    if s.lower().endswith('g'):
        suffix = 1e9
    if suffix != 1:
        s = s[:-1]
    return float(s) * suffix

def freq_parse(s):
    suffix = 1
    if s.lower().endswith('k'):
        suffix = 1e3
    if s.lower().endswith('m'):
        suffix = 1e6
    if s.lower().endswith('g'):
        suffix = 1e9
    if suffix != 1:
        s = s[:-1]
    return float(s) * suffix

def duration_parse(s):
    suffix = 1
    if s.lower().endswith('s'):
        suffix = 1
    if s.lower().endswith('m'):
        suffix = 60
    if s.lower().endswith('h'):
        suffix = 60 * 60
    if suffix != 1 or s.lower().endswith('s'):
        s = s[:-1]
    return float(s) * suffix

def gzip_wrap(path):
    "hides silly CRC errors"
    iterator = gzip.open(path, 'rb')
    running = True
    while running:
        try:
            yield next(iterator)
        except IOError:
            running = False

path = args.input_path
output = args.output_path

raw_data = lambda: open(path)
if path.endswith('.gz'):
    raw_data = lambda: gzip_wrap(path)

if args.low_freq is not None:
    args.low_freq = freq_parse(args.low_freq)
if args.high_freq is not None:
    args.high_freq = freq_parse(args.high_freq)
if args.offset_freq is not None:
    args.offset_freq = freq_parse(args.offset_freq)
else:
    args.offset_freq = 0
if args.time_tick is not None:
    args.time_tick = duration_parse(args.time_tick)

print("loading")

def slice_columns(columns, low_freq, high_freq):
    start_col = 0
    stop_col  = len(columns)
    if args.low_freq  is not None and low <= args.low_freq  <= high:
        start_col = sum(f<args.low_freq   for f in columns)
    if args.high_freq is not None and low <= args.high_freq <= high:
        stop_col  = sum(f<=args.high_freq for f in columns)
    return start_col, stop_col-1

def parse_time(t):
    return datetime.datetime.strptime(t, '%Y-%m-%d %H:%M:%S')

freqs = set()
f_cache = set()
times = set()
labels = set()
min_z = 0
max_z = -100
start, stop = None, None
for line in raw_data():
    line = [s.strip() for s in line.strip().split(',')]
    #line = [line[0], line[1]] + [float(s) for s in line[2:] if s]
    line = [s for s in line if s]

    low  = int(line[2]) + args.offset_freq
    high = int(line[3]) + args.offset_freq
    step = float(line[4])
    if args.low_freq  is not None and high < args.low_freq:
        continue
    if args.high_freq is not None and args.high_freq < low:
        continue
    columns = list(frange(low, high, step))
    start_col, stop_col = slice_columns(columns, args.low_freq, args.high_freq)
    f_key = (columns[start_col], columns[stop_col], step)
    if f_key not in f_cache:
        freqs.update(list(frange(*f_key)))
        freqs.add(f_key[1])  # high
        labels.add(f_key[0])  # low
        f_cache.add(f_key)

    t = line[0] + ' ' + line[1]
    times.add(t)

    zs = line[6+start_col:6+stop_col+1]
    zs = [float(z) for z in zs]
    #zs = min_filter(line[6:])
    min_z = min(min_z, min(z for z in zs if not math.isinf(z)))
    max_z = max(max_z, max(zs))

    if start is None:
        start = parse_time(line[0] + ' ' + line[1])
    stop = parse_time(line[0] + ' ' + line[1])

freqs = list(sorted(list(freqs)))
times = list(sorted(list(times)))
labels = list(sorted(list(labels)))

if len(labels) == 1:
    delta = (max(freqs) - min(freqs)) / (len(freqs) / 500.0)
    delta = round(delta / 10**int(math.log10(delta))) * 10**int(math.log10(delta))
    delta = int(delta)
    lower = int(math.ceil(min(freqs) / delta) * delta)
    labels = list(range(lower, int(max(freqs)), delta))

print("x: %i, y: %i, z: (%f, %f)" % (len(freqs), len(times), min_z, max_z))

def rgb2(z):
    g = (z - min_z) / (max_z - min_z)
    return (int(g*255), int(g*255), 50)

def rgb3(z):
    g = (z - min_z) / (max_z - min_z)
    c = colorsys.hsv_to_rgb(0.65-(g-0.08), 1, 0.2+g)
    return (int(c[0]*256),int(c[1]*256),int(c[2]*256))

print("drawing")
img = Image.new("RGB", (len(freqs), len(times)))
pix = img.load()
x_size = img.size[0]
for line in raw_data():
    line = [s.strip() for s in line.strip().split(',')]
    #line = [line[0], line[1]] + [float(s) for s in line[2:] if s]
    line = [s for s in line if s]
    t = line[0] + ' ' + line[1]
    if t not in times:
        continue  # happens with live files
    y = times.index(t)
    low = int(line[2]) + args.offset_freq
    high = int(line[3]) + args.offset_freq
    step = float(line[4])
    columns = list(frange(low, high, step))
    start_col, stop_col = slice_columns(columns, args.low_freq, args.high_freq)
    x_start = freqs.index(columns[start_col])
    #zs = line[6:]
    #zs = min_filter(line[6:])
    zs = line[6+start_col:6+stop_col+1]
    for i in range(len(zs)):
        x = x_start + i
        if x >= x_size:
            continue
        z = float(zs[i])
        # fast check for nan/-inf
        if not z >= min_z:
            z = min_z
        pix[x,y] = rgb2(z)

print("labeling")
draw = ImageDraw.Draw(img)
font = ImageFont.load_default()
pixel_width = step
for label in labels:
    y = 10
    #x = freqs.index(label)
    x = int((label-min(freqs)) / pixel_width)
    s = '%.3fMHz' % (label/1000000.0)
    draw.text((x, y), s, font=font, fill='white')

if args.time_tick:
    label_last = start
    for y,t in enumerate(times):
        label_time = parse_time(t)
        label_diff = label_time - label_last
        if label_diff.seconds >= args.time_tick:
            draw.text((2, y), '%s' % t.split(' ')[-1], font=font, fill='white')
            label_last = label_time


duration = stop - start
duration = duration.days * 24*60*60 + duration.seconds + 30
pixel_height = duration / len(times)
hours = int(duration / 3600)
minutes = int((duration - 3600*hours) / 60)
margin = 2
if args.time_tick:
    margin = 60
draw.text((margin, img.size[1] - 45), 'Duration: %i:%02i' % (hours, minutes), font=font, fill='white')
draw.text((margin, img.size[1] - 35), 'Range: %.2fMHz - %.2fMHz' % (min(freqs)/1e6, max(freqs)/1e6), font=font, fill='white')
draw.text((margin, img.size[1] - 25), 'Pixel: %.2fHz x %is' % (pixel_width, int(round(pixel_height))), font=font, fill='white')
draw.text((margin, img.size[1] - 15), 'Started: {0}'.format(start), font=font, fill='white')
# bin size

print("saving")
img.save(output)






