#! /usr/bin/env python

import os
import json
from PIL import Image, ImageDraw, ImageFont
import sys, gzip, math, argparse, colorsys, datetime

# Version
# Add --parameters feature
# Can add text from --parameters
# Create class & Refactoring

# todo:
# matplotlib powered --interactive
# arbitrary freq marker spacing
# ppm
# blue-less marker grid
# fast summary thing
# time-based slicing
# gain normalization


class HeatmapGenerator(object):
    fontsize = 10
    font = None
    tape_height = 25
    legend_height = 0
    waterwall_width = 0
    waterwall_height = 0
    tape_pt = 10
    min_z = 100
    max_z = -100
    freqs = set()
    times = set()
    start, stop, step = None, None, None
    low_freq, high_freq = None, None
    offset_freq = 0
    time_tick = None
    db_limit_isset = False
    img = None
    csv_path, png_path = "", ""
    texts = []
    img_width, img_height = 0, 0
    heatmap_parameters = None
    texts = []

    def __init__(self, ):
        try:
            self.font = ImageFont.truetype("Vera.ttf", self.fontsize)
        except IOError:
            print('Please download the Vera.ttf font and place it in the current directory.')
            sys.exit(1)

    # Init image object
    def init_heatmap(self):
        self.waterwall_width = len(self.freqs)
        self.waterwall_height = len(self.times)

        self.img_width = self.waterwall_width
        self.img_height = self.tape_height + self.waterwall_height + self.legend_height
        self.img = Image.new("RGB", (self.img_width, self.img_height))

    # Save image object to file
    def save(self, filename):
        print("saving")
        self.img.save(filename)

    # Set heatmap parameters from JSON file
    def set_heatmap_parameters(self, content):
        self.heatmap_parameters = content

        # Override dB limit
        if self.heatmap_parameters and 'db' in self.heatmap_parameters:
            self.set_db_limit(self.heatmap_parameters['db']['min'], self.heatmap_parameters['db']['max'])

        if self.heatmap_parameters and 'legens' in self.heatmap_parameters:
            self.legend_height = 25

    # Overide minimal and maximal dB limit
    def set_db_limit(self, min_z, max_z):
        self.db_limit_isset = True
        self.min_z = min(min_z, max_z)
        self.max_z = max(min_z, max_z)

    # Calc power db level color
    def rgb2(self, z):
        g = (z - self.min_z) / (self.max_z - self.min_z)
        return int(g * 255), int(g * 255), 50

    def slice_columns(self, columns, low, high):
        start_col = 0
        stop_col = len(columns)
        if self.low_freq is not None and low <= self.low_freq <= high:
            start_col = sum(f < args.low_freq for f in columns)
        if self.high_freq is not None and low <= self.high_freq <= high:
            stop_col = sum(f <= args.high_freq for f in columns)
        return start_col, stop_col - 1

    # Compute the CSV datas summary
    def calc_summary(self, filename):
        self.freqs = set()
        f_cache = set()
        self.times = set()
        self.start, self.stop = None, None

        # Create a loader function
        raw_data = lambda: open(filename)
        if filename.endswith('.gz'):
            raw_data = lambda: gzip_wrap(filename)

        # Load CSV datas
        print("loading")
        for line in raw_data():
            line = [s.strip() for s in line.strip().split(',')]
            # line = [line[0], line[1]] + [float(s) for s in line[2:] if s]
            line = [s for s in line if s]

            low = int(line[2]) + self.offset_freq
            high = int(line[3]) + self.offset_freq
            self.step = float(line[4])
            if self.low_freq is not None and high < self.low_freq:
                continue
            if self.high_freq is not None and self.high_freq < low:
                continue
            columns = list(frange(low, high, self.step))
            start_col, stop_col = self.slice_columns(columns, low, high)
            f_key = (columns[start_col], columns[stop_col], self.step)
            zs = line[6 + start_col:6 + stop_col + 1]
            if f_key not in f_cache:
                freq2 = list(frange(*f_key))[:len(zs)]
                self.freqs.update(freq2)
                f_cache.add(f_key)

            t = line[0] + ' ' + line[1]
            self.times.add(t)

            if not self.db_limit_isset:
                zs = floatify(zs)
                self.min_z = min(self.min_z, min(zs))
                self.max_z = max(self.max_z, max(zs))

            if self.start is None:
                self.start = parse_time(line[0] + ' ' + line[1])
            self.stop = parse_time(line[0] + ' ' + line[1])

        self.freqs = list(sorted(list(self.freqs)))
        self.times = list(sorted(list(self.times)))


        print("x: %i, y: %i, z: (%f, %f)" % (len(self.freqs), len(self.times), self.min_z, self.max_z))


    # Draw the rtl_power signal result
    def draw_heatmap(self, filename):
        # Create a loader function
        raw_data = lambda: open(filename)
        if filename.endswith('.gz'):
            raw_data = lambda: gzip_wrap(filename)

        pix = self.img.load()
        print("drawing")
        for line in raw_data():
            line = [s.strip() for s in line.strip().split(',')]
            # line = [line[0], line[1]] + [float(s) for s in line[2:] if s]
            line = [s for s in line if s]
            t = line[0] + ' ' + line[1]
            if t not in self.times:
                continue  # happens with live files
            y = self.times.index(t)
            low = int(line[2]) + self.offset_freq
            high = int(line[3]) + self.offset_freq
            columns = list(frange(low, high, self.step))
            start_col, stop_col = self.slice_columns(columns, low, high)
            x_start = self.freqs.index(columns[start_col])
            zs = floatify(line[6 + start_col:6 + stop_col + 1])
            for idx in range(len(zs)):
                x = x_start + idx
                if x >= self.img_width:
                    continue
                pix[x, y + self.tape_height] = self.rgb2(zs[idx])

    def draw_texts(self):
        duration = self.stop - self.start
        duration = duration.days * 24 * 60 * 60 + duration.seconds + 30
        pixel_height = duration / len(self.times)
        hours = int(duration / 3600)
        minutes = int((duration - 3600 * hours) / 60)

        # Add Text
        self.add_text('Started: {0}'.format(self.start))
        self.add_text('Pixel: %.2fHz x %is' % (self.step, int(round(pixel_height))))
        self.add_text('Range: %.2fMHz - %.2fMHz' % (min(self.freqs) / 1e6, (max(self.freqs) + self.step) / 1e6))
        self.add_text('Duration: %i:%02i' % (hours, minutes))

        # Add text from parameters
        if self.heatmap_parameters and 'texts' in self.heatmap_parameters:
            self.texts = self.texts + self.heatmap_parameters['texts']

        self.draw_textfromlist()

    def draw_legends(self):
        pass


    # Draw text from python list
    def draw_textfromlist(self):
        ypos = 5
        margin = 2
        if args.time_tick:
            margin = 60


        reverse = self.heatmap_parameters and 'reversetextsorder' in self.heatmap_parameters and self.heatmap_parameters[
            'reversetextsorder']
        if reverse:
            textlist = reversed(self.texts)

        for textinfo in self.texts:
            # Get default text parameters
            fg_color = textinfo['fg_color'] if 'fg_color' in textinfo else 'white'
            bg_color = textinfo['bg_color'] if 'bg_color' in textinfo else 'black'

            # Draw text
            text = textinfo['text']
            self.shadow_text(margin, self.img_width - (ypos + self.fontsize), text, self.font, fg_color, bg_color)
            ypos += self.fontsize

    def shadow_text(self, x, y, s, font, fg_color='white', bg_color='black'):
        draw = ImageDraw.Draw(self.img)
        draw.text((x + 1, y + 1), s, font=font, fill=bg_color)
        draw.text((x, y), s, font=font, fill=fg_color)

    # Add text in the global texts array
    def add_text(self, text, fg_color=None, bg_color=None):
        textinfo = {'text': text}
        if fg_color:
            textinfo['fg_color'] = fg_color
        if bg_color:
            textinfo['bg_color'] = bg_color

        self.texts.append(textinfo)


    def label_heatmap(self):
        print("labeling")
        draw = ImageDraw.Draw(self.img)
        # gblfont = ImageFont.load_default()


        # Init tape
        draw.rectangle([0, 0, self.img_width, self.tape_height], fill='yellow')
        min_freq = min(self.freqs)
        max_freq = max(self.freqs)
        width = len(self.freqs)

        # Compute label base
        label_base = 8
        for idx in range(8, 0, -1):
            interval = int(10 ** idx)
            low_f = (min_freq // interval) * interval
            high_f = (1 + max_freq // interval) * interval
            hits = len(range(int(low_f), int(high_f), interval))
            if hits >= 4:
                label_base = idx
                break
        label_base = 10 ** label_base

        for scale, y in [(1, 10), (5, 15), (10, 19), (50, 22), (100, 24), (500, 25)]:
            hits = self.tape_lines(label_base / scale, y, self.tape_height)
            pixels_per_hit = width / hits
            if pixels_per_hit > 50:
                self.tape_text(label_base / scale, y - self.tape_pt)
            if pixels_per_hit < 10:
                break

        if args.time_tick:
            label_last = self.start
            for y, t in enumerate(self.times):
                label_time = parse_time(t)
                label_diff = label_time - label_last
                if label_diff.seconds >= args.time_tick:
                    self.shadow_text(2, y + self.tape_height, '%s' % t.split(' ')[-1], self.font)
                    label_last = label_time

    def tape_lines(self, interval, y1, y2, used=set()):
        """returns the number of lines"""
        draw = ImageDraw.Draw(self.img)
        low_f = (min(self.freqs) // interval) * interval
        high_f = (1 + max(self.freqs) // interval) * interval
        hits = 0
        blur = lambda p: blend(p, (255, 255, 0), (0, 0, 0))
        for idx in range(int(low_f), int(high_f), int(interval)):
            if not (min(self.freqs) < idx < max(self.freqs)):
                continue
            hits += 1
            if idx in used:
                continue
            x1, x2 = closest_index(idx, self.freqs, interpolate=True)
            if x1 == x2:
                draw.line([x1, y1, x1, y2], fill='black')
            else:
                percent = (idx - self.freqs[x1]) / float(self.freqs[x2] - self.freqs[x1])
                draw.line([x1, y1, x1, y2], fill=blur(percent))
                draw.line([x2, y1, x2, y2], fill=blur(1 - percent))
            used.add(idx)
        return hits

    def tape_text(self, interval, y, used=set()):
        low_f = (min(self.freqs) // interval) * interval
        high_f = (1 + max(self.freqs) // interval) * interval
        for idx in range(int(low_f), int(high_f), int(interval)):
            if idx in used:
                continue
            if not (min(self.freqs) < idx < max(self.freqs)):
                continue
            x = closest_index(idx, self.freqs)
            if interval >= 1e6:
                s = '%iM' % (idx / 1e6)
            elif interval > 1000:
                s = '%ik' % ((idx / 1e3) % 1000)
                if s.startswith('0'):
                    s = '%iM' % (idx / 1e6)
            else:
                s = '%i' % (idx % 1000)
                if s.startswith('0'):
                    s = '%ik' % ((idx / 1e3) % 1000)
                if s.startswith('0'):
                    s = '%iM' % (idx / 1e6)
            w = word_aa(s, self.tape_pt, 'black', 'yellow')
            self.img.paste(w, (x - w.size[0] // 2, y))
            used.add(idx)


# #######################################
# Functions
########################################

def frange(start, stop, step):
    idx = 0
    while idx * step + start <= stop:
        yield idx * step + start
        idx += 1


# def min_filter(row):
#     size = 3
#     result = []
#     for i in range(size):
#         here = row[i]
#         near = row[0:i] + row[i+1:size]
#         if here > min(near):
#             result.append(here)
#             continue
#         result.append(min(near))
#     for i in range(size-1, len(row)):
#         here = row[i]
#         near = row[i-(size-1):i]
#         if here > min(near):
#             result.append(here)
#             continue
#         result.append(min(near))
#     return result

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
    """hides silly CRC errors"""
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


def parse_time(t):
    return datetime.datetime.strptime(t, '%Y-%m-%d %H:%M:%S')




# def rgb3(z):
#     g = (z - min_z) / (max_z - min_z)
#     c = colorsys.hsv_to_rgb(0.65-(g-0.08), 1, 0.2+g)
#     return (int(c[0]*256),int(c[1]*256),int(c[2]*256))

def closest_index(n, m_list, interpolate=False):
    """assumes sorted m_list, returns two points for interpolate"""
    idx = len(m_list) // 2
    jump = len(m_list) // 2
    while jump > 1:
        i_down = idx - jump
        i_here = idx
        i_up = idx + jump
        if i_down < 0:
            i_down = idx
        if i_up >= len(m_list):
            i_up = idx
        e_down = abs(m_list[i_down] - n)
        e_here = abs(m_list[i_here] - n)
        e_up = abs(m_list[i_up] - n)
        e_best = min([e_down, e_here, e_up])
        if e_down == e_best:
            idx = i_down
        if e_up == e_best:
            idx = i_up
        if e_here == e_best:
            idx = i_here
        jump //= 2
    if not interpolate:
        return idx
    if n < m_list[idx] and idx > 0:
        return idx - 1, idx
    if n > m_list[idx] and idx < len(m_list) - 1:
        return idx, idx + 1
    return idx, idx


def word_aa(label, pt, fg_color, bg_color):
    f = ImageFont.truetype("Vera.ttf", pt * 3)
    s = f.getsize(label)
    s = (s[0], pt * 3 + 3)  # getsize lies, manually compute
    w_img = Image.new("RGB", s, bg_color)
    w_draw = ImageDraw.Draw(w_img)
    w_draw.text((0, 0), label, font=f, fill=fg_color)
    return w_img.resize((s[0] // 3, s[1] // 3), Image.ANTIALIAS)


def blend(percent, c1, c2):
    """c1 and c2 are RGB tuples"""
    # probably isn't gamma correct
    r = c1[0] * percent + c2[0] * (1 - percent)
    g = c1[1] * percent + c2[1] * (1 - percent)
    b = c1[2] * percent + c2[2] * (1 - percent)
    c3 = map(int, map(round, [r, g, b]))
    return tuple(c3)

########################################
# Main
########################################

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
slicegroup.add_argument('--parameters', dest='heatmap_parameters', default=None, action='append',
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


# Init heatmap generator
heatmap_generator = HeatmapGenerator()

# Check frequencies command line parameters
if args.low_freq is not None:
    heatmap_generator.low_freq = freq_parse(args.low_freq)
if args.high_freq is not None:
    heatmap_generator.high_freq = freq_parse(args.high_freq)
if args.offset_freq is not None:
    heatmap_generator.offset_freq = freq_parse(args.offset_freq)

if args.time_tick is not None:
    heatmap_generator.time_tick = duration_parse(args.time_tick)

# Modify dB limit
if args.db_limit:
    heatmap_generator.set_db_limit(min(map(float, args.db_limit)), max(map(float, args.db_limit)))

# Load heatmap parameters JSON files
hparameters = None
if args.heatmap_parameters is not None:
    global_heatmap_params = {}
    for filename in args.heatmap_parameters:
        hparameters = load_jsonfile(filename)
        global_heatmap_params.update(hparameters)
    heatmap_generator.set_heatmap_parameters(global_heatmap_params)

# Compute CSV datas
heatmap_generator.calc_summary(args.input_path)
heatmap_generator.init_heatmap()

# Draw the heatmap
heatmap_generator.draw_heatmap(args.input_path)
heatmap_generator.label_heatmap()
heatmap_generator.draw_texts()
#heatmap_generator.draw_legend()

# Save the result
heatmap_generator.save(args.output_path)
