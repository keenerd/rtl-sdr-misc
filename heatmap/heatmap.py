#! /usr/bin/env python

import os
import json
from PIL import Image, ImageDraw, ImageFont
import sys, gzip, math, argparse, colorsys, datetime
from collections import defaultdict
from itertools import *

# Version
# Add --parameters feature
# Can add text from --parameters

# todo:
# matplotlib powered --interactive
# arbitrary freq marker spacing
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
parser.add_argument('--db', dest='db_limit', nargs=2, default=None,
    help='Maximum and minimum db values.')
slicegroup = parser.add_argument_group('Slicing',
    'Efficiently render a portion of the data. (optional)')
slicegroup.add_argument('--low', dest='low_freq', default=None,
    help='Minimum frequency for a subrange.')
slicegroup.add_argument('--high', dest='high_freq', default=None,
    help='Maximum frequency for a subrange.')
slicegroup.add_argument('--parameters', dest='heatmap_parameters', default=None,
    help='heatmap parameters JSON file')
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

# hack, http://stackoverflow.com/questions/9025204/
for i, arg in enumerate(sys.argv):
    if (arg[0] == '-') and arg[1].isdigit():
        sys.argv[i] = ' ' + arg
args = parser.parse_args()

fontsize = 10
try:
    gblfont = ImageFont.truetype("Vera.ttf", fontsize)
except:
    print('Please download the Vera.ttf font and place it in the current directory.')
    sys.exit(1)


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

def floatify(zs):
    # nix errors with -inf, windows errors with -1.#J
    zs2 = []
    previous = 0  # awkward for single-column rows
    for z in zs:
        try:
            z = float(z)
        except ValueError:
            z = previous
        if math.isinf(z):
            z = previous
        if math.isnan(z):
            z = previous
        zs2.append(z)
        previous = z
    return zs2

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

def load_jsonfile(filename):
    exists = os.path.isfile(filename)
    if exists:
        configlines = open(filename).read()
        return json.loads(configlines)

    return None

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
min_z = 0
max_z = -100
start, stop = None, None

db_limit_isset = False
if args.db_limit:
    min_z = min(map(float, args.db_limit))
    max_z = max(map(float, args.db_limit))
    db_limit_isset = True

# Load heatmap parameters
heatmap_parameters = None
if args.heatmap_parameters is not None:
    heatmap_parameters = load_jsonfile(args.heatmap_parameters)

    if heatmap_parameters and 'db' in heatmap_parameters:
        min_z = heatmap_parameters['db']['min']
        max_z = heatmap_parameters['db']['max']
        db_limit_isset = True

print("loading")
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
    zs = line[6+start_col:6+stop_col+1]
    if f_key not in f_cache:
        freq2 = list(frange(*f_key))[:len(zs)]
        freqs.update(freq2)
        f_cache.add(f_key)

    t = line[0] + ' ' + line[1]
    times.add(t)

    if not db_limit_isset:
        zs = floatify(zs)
        min_z = min(min_z, min(zs))
        max_z = max(max_z, max(zs))

    if start is None:
        start = parse_time(line[0] + ' ' + line[1])
    stop = parse_time(line[0] + ' ' + line[1])

freqs = list(sorted(list(freqs)))
times = list(sorted(list(times)))

print("x: %i, y: %i, z: (%f, %f)" % (len(freqs), len(times), min_z, max_z))

def rgb2(z):
    g = (z - min_z) / (max_z - min_z)
    return (int(g*255), int(g*255), 50)

def rgb3(z):
    g = (z - min_z) / (max_z - min_z)
    c = colorsys.hsv_to_rgb(0.65-(g-0.08), 1, 0.2+g)
    return (int(c[0]*256),int(c[1]*256),int(c[2]*256))

print("drawing")
tape_height = 25
img = Image.new("RGB", (len(freqs), tape_height + len(times)))
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
    zs = floatify(line[6+start_col:6+stop_col+1])
    for i in range(len(zs)):
        x = x_start + i
        if x >= x_size:
            continue
        pix[x,y+tape_height] = rgb2(zs[i])

def closest_index(n, m_list, interpolate=False):
    "assumes sorted m_list, returns two points for interpolate"
    i = len(m_list) // 2
    jump = len(m_list) // 2
    while jump > 1:
        i_down = i - jump
        i_here = i
        i_up =   i + jump
        if i_down < 0:
            i_down = i
        if i_up >= len(m_list):
            i_up = i
        e_down = abs(m_list[i_down] - n)
        e_here = abs(m_list[i_here] - n)
        e_up   = abs(m_list[i_up]   - n)
        e_best = min([e_down, e_here, e_up])
        if e_down == e_best:
            i = i_down
        if e_up == e_best:
            i = i_up
        if e_here == e_best:
            i = i_here
        jump = jump // 2
    if not interpolate:
        return i
    if n < m_list[i] and i > 0:
        return i-1, i
    if n > m_list[i] and i < len(m_list)-1:
        return i, i+1
    return i, i

def word_aa(label, pt, fg_color, bg_color):
    f = ImageFont.truetype("Vera.ttf", pt*3)
    s = f.getsize(label)
    s = (s[0], pt*3 + 3)  # getsize lies, manually compute
    w_img = Image.new("RGB", s, bg_color)
    w_draw = ImageDraw.Draw(w_img)
    w_draw.text((0, 0), label, font=f, fill=fg_color)
    return w_img.resize((s[0]//3, s[1]//3), Image.ANTIALIAS)

def blend(percent, c1, c2):
    "c1 and c2 are RGB tuples"
    # probably isn't gamma correct
    r = c1[0] * percent + c2[0] * (1 - percent)
    g = c1[1] * percent + c2[1] * (1 - percent)
    b = c1[2] * percent + c2[2] * (1 - percent)
    c3 = map(int, map(round, [r,g,b]))
    return tuple(c3)

def tape_lines(interval, y1, y2, used=set()):
    "returns the number of lines"
    low_f = (min(freqs) // interval) * interval
    high_f = (1 + max(freqs) // interval) * interval
    hits = 0
    blur = lambda p: blend(p, (255, 255, 0), (0, 0, 0))
    for i in range(int(low_f), int(high_f), int(interval)):
        if not (min(freqs) < i < max(freqs)):
            continue
        hits += 1
        if i in used:
            continue
        x1,x2 = closest_index(i, freqs, interpolate=True)
        if x1 == x2:
            draw.line([x1,y1,x1,y2], fill='black')
        else:
            percent = (i - freqs[x1]) / float(freqs[x2] - freqs[x1])
            draw.line([x1,y1,x1,y2], fill=blur(percent))
            draw.line([x2,y1,x2,y2], fill=blur(1-percent))
        used.add(i)
    return hits

def tape_text(interval, y, used=set()):
    low_f = (min(freqs) // interval) * interval
    high_f = (1 + max(freqs) // interval) * interval
    for i in range(int(low_f), int(high_f), int(interval)):
        if i in used:
            continue
        if not (min(freqs) < i < max(freqs)):
            continue
        x = closest_index(i, freqs)
        s = str(i)
        if interval >= 1e6:
            s = '%iM' % (i/1e6)
        elif interval > 1000:
            s = '%ik' % ((i/1e3) % 1000)
            if s.startswith('0'):
                s = '%iM' % (i/1e6)
        else:
            s = '%i' % (i%1000)
            if s.startswith('0'):
                s = '%ik' % ((i/1e3) % 1000)
            if s.startswith('0'):
                s = '%iM' % (i/1e6)
        w = word_aa(s, tape_pt, 'black', 'yellow')
        img.paste(w, (x - w.size[0]//2, y))
        used.add(i)

# Add text in the global texts array
def add_text(text, font = None, fg_color=None, bg_color=None):
    textinfo = {}
    textinfo['text'] = text
    textinfo['font'] = font if font else gblfont
    if fg_color:
        textinfo['fg_color'] = fg_color
    if bg_color:
        textinfo['bg_color'] = bg_color
    texts.append(textinfo)

# Draw text from python list
def draw_textfromlist(leftpos, toppos, imgsize, textlist, reverse=True):
    textpos = toppos

    if reverse:
        textlist = reversed(textlist)

    for textinfo in textlist:
        # Get default text parameters
        font = textinfo['font'] if 'font' in textinfo else gblfont
        fg_color = textinfo['fg_color'] if 'fg_color' in textinfo else 'white'
        bg_color = textinfo['bg_color'] if 'bg_color' in textinfo else 'black'

        # Draw text
        text = textinfo['text']
        shadow_text(leftpos, imgsize - (textpos + fontsize), text, font, fg_color, bg_color)
        textpos += fontsize

# Concatenate texts content (defaut heatmap and from --parameters)
def draw_texts(leftpos, imgsize):
    textpos = 5

    alltexts = []
    alltexts = alltexts + texts
    if heatmap_parameters and 'texts' in heatmap_parameters:
        alltexts = alltexts + heatmap_parameters['texts']

    reverse = heatmap_parameters and 'reversetextsorder' in heatmap_parameters and heatmap_parameters['reversetextsorder'] == True
    draw_textfromlist(leftpos, textpos, imgsize, alltexts, reverse)


def shadow_text(x, y, s, font, fg_color='white', bg_color='black'):
    draw.text((x+1, y+1), s, font=font, fill=bg_color)
    draw.text((x, y), s, font=font, fill=fg_color)

print("labeling")
tape_pt = 10
draw = ImageDraw.Draw(img)
gblfont = ImageFont.load_default()
pixel_width = step

draw.rectangle([0,0,img.size[0],tape_height], fill='yellow')
min_freq = min(freqs)
max_freq = max(freqs)
delta = max_freq - min_freq
width = len(freqs)
label_base = 8

for i in range(8, 0, -1):
    interval = int(10**i)
    low_f = (min_freq // interval) * interval
    high_f = (1 + max_freq // interval) * interval
    hits = len(range(int(low_f), int(high_f), interval))
    if hits >= 4:
        label_base = i
        break
label_base = 10**label_base

for scale,y in [(1,10), (5,15), (10,19), (50,22), (100,24), (500, 25)]:
    hits = tape_lines(label_base/scale, y, tape_height)
    pixels_per_hit = width / hits
    if pixels_per_hit > 50:
        tape_text(label_base/scale, y-tape_pt)
    if pixels_per_hit < 10:
        break

if args.time_tick:
    label_last = start
    for y,t in enumerate(times):
        label_time = parse_time(t)
        label_diff = label_time - label_last
        if label_diff.seconds >= args.time_tick:
            shadow_text(2, y+tape_height, '%s' % t.split(' ')[-1], gblfont)
            label_last = label_time


duration = stop - start
duration = duration.days * 24*60*60 + duration.seconds + 30
pixel_height = duration / len(times)
hours = int(duration / 3600)
minutes = int((duration - 3600*hours) / 60)

# Show the text
imgheight = img.size[1]
margin = 2
if args.time_tick:
    margin = 60

texts = []
add_text('Started: {0}'.format(start))
add_text('Pixel: %.2fHz x %is' % (pixel_width, int(round(pixel_height))))
add_text('Range: %.2fMHz - %.2fMHz' % (min(freqs) / 1e6, (max(freqs) + pixel_width) / 1e6))
add_text('Duration: %i:%02i' % (hours, minutes))
draw_texts(margin, imgheight)

print("saving")
img.save(output)






