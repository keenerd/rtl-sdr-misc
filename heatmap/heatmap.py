#!/usr/bin/env python

from PIL import Image, ImageDraw, ImageFont
import sys, gzip, math, colorsys, datetime
from collections import defaultdict
from itertools import *

# todo: matplotlib powered --interactive
# arbitrary freq marker spacing

path = sys.argv[1]
output = sys.argv[2]
freq_shift=-50 #if use upconverter
time_label_interval=1800 #seconds
time_line_marker = False

raw_data = lambda: open(path)
if path.endswith('.gz'):
    raw_data = lambda: gzip.open(path, 'rb')

def frange(start, stop, step):
    i = 0
    while (i*step + start <= stop):
        yield i*step + start
        i += 1

print("loading")

freqs = set()
f_cache = set()
times = set()
labels = set()
min_z = 0
max_z = -100
start, stop = None, None
for line in raw_data():
    line = [s.strip() for s in line.strip().split(',')]
    line = [line[0], line[1]] + [float(s) for s in line[2:] if s]

    low = line[2]
    high = line[3]
    step = line[4]
    f_key = (int(low), int(high), step)
    if f_key not in f_cache:
        freqs.update(list(frange(int(low), int(high), step)))
        freqs.add(high)
        labels.add(low)
        f_cache.add(f_key)

    t = line[0] + ' ' + line[1]
    times.add(t)

    zs = line[6:]
    min_z = min(min_z, min(z for z in zs if not math.isinf(z)))
    max_z = max(max_z, max(zs))

    if start is None:
        start = datetime.datetime.strptime(line[0] + ' ' + line[1], '%Y-%m-%d %H:%M:%S')
    stop = datetime.datetime.strptime(line[0] + ' ' + line[1], '%Y-%m-%d %H:%M:%S')

freqs = list(sorted(list(freqs)))
times = list(sorted(list(times)))
labels = list(sorted(list(labels)))

if len(labels) == 1:
    delta = (max(freqs) - min(freqs)) / (len(freqs) / 500)
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
    line = [line[0], line[1]] + [float(s) for s in line[2:] if s]
    t = line[0] + ' ' + line[1]
    if t not in times:
        continue  # happens with live files
    y = times.index(t)
    low = line[2]
    high = line[3]
    step = line[4]
    x_start = freqs.index(low)
    for i in range(len(line[6:])):
        x = x_start + i
        if x >= x_size:
            continue
        z = line[6+i]
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
    s = '%.3fMHz' % (label/1000000.0+freq_shift)
    draw.text((x, y), s, font=font, fill='white')

# time labels
y=0
label_last = start
for time in times:
    label_time=datetime.datetime.strptime(time, '%Y-%m-%d %H:%M:%S')
    label_diff=label_time - label_last
    if label_diff.seconds >= time_label_interval:
        draw.text((2, y), '%s' % time[11:11+5], font=font, fill='white')
        if time_line_marker:
            draw.line((0, y, img.size[0], y), fill='white', width=1)
        label_last=label_time
    y+=1

duration = stop - start
#total_time=duration
#print(duration)
duration = (duration.days*86400)+duration.seconds
pixel_height = duration / len(times)
#print(duration)
hours = int(duration / 3600)
minutes = int((duration - 3600*hours) / 60)
draw.text((40, img.size[1] - 45), 'Duration:  %i:%02i' % (hours, minutes), font=font, fill='white')
draw.text((40, img.size[1] - 35), 'Range: %.2fMHz - %.2fMHz' % ((min(freqs)/1e6)+freq_shift, (max(freqs)/1e6)+freq_shift), font=font, fill='white')
draw.text((40, img.size[1] - 25), 'Pixel: %.2fHz x %is' % (pixel_width, int(round(pixel_height))), font=font, fill='white')
draw.text((40, img.size[1] - 15), 'Started: {0}'.format(start), font=font, fill='white')
# bin size

print("saving")
#img.save(output,quality=92, optimize=True)
#img.save(output, quality=92)
img.save(output, quality=80)
print(start)
print(stop)

