#! /usr/bin/env python

"""
takes raw iq, turns into heatmap
extremely crude, lacks features like windowing
"""

import sys, math, struct
import numpy
from PIL import Image

def help():
    print("raw_iq.py bins averages sample-type input.raw")
    print("  sample_types: u1 (uint8), s1 (int8), s2 (int16)")
    sys.exit()

def byte_reader(path, sample):
    dtype = None
    offset = 0
    scale = 2**7
    if sample == 'u1':
        dtype = numpy.uint8
        offset = -127
    if sample == 's1':
        dtype = numpy.int8
    if sample == 's2':
        dtype = numpy.int16
        scale = 2**15
    raw = numpy.fromfile(path, dtype).astype(numpy.float64)
    raw += offset
    raw /= scale
    return raw[0::2] + 1j * raw[1::2]

def psd(data, bin_count, averages):
    "really basic, lacks windowing"
    length = len(data)
    table = [numpy.zeros(bin_count)]
    ave = 0
    for i in range(0, length, bin_count):
        sub_data = numpy.array(data[i:i+bin_count])
        dc_bias = sum(sub_data) / len(sub_data)
        #sub_data -= dc_bias
        fft = numpy.fft.fft(sub_data)
        table[-1] = table[-1] + numpy.real(numpy.conjugate(fft)*fft)
        ave += 1
        if ave >= averages:
            ave = max(1, ave)
            row = table[-1]
            row = numpy.concatenate((row[bin_count//2:], row[:bin_count//2]))
            # spurious warnings
            table[-1] = 10 * numpy.log10(row / ave)
            table.append(numpy.zeros(bin_count))
            ave = 0
    if ave != 0:
        row = table[-1]
        row = numpy.concatenate((row[bin_count//2:], row[:bin_count//2]))
        table[-1] = 10 * numpy.log10(row / ave)
    if ave == 0:
        table.pop(-1)
    return table

def rgb2(z, lowest, highest):
    g = (z - lowest) / (highest - lowest)
    return (int(g*255), int(g*255), 50)

def heatmap(table):
    lowest = -1
    highest = -100
    for row in table:
        lowest = min(lowest, min(z for z in row if not math.isinf(z)))
        highest = max(highest, max(row))
    img = Image.new("RGB", (len(table[0]), len(table)))
    pix = img.load()
    for y,row in enumerate(table):
        for x,val in enumerate(row):
            if not val >= lowest:  # fast nan/-inf test
                val = lowest
            pix[x,y] = rgb2(val, lowest, highest)
    return img

if __name__ == '__main__':
    try:
        _, bin_count, averages, sample, path = sys.argv
        bin_count = int(bin_count)
        bin_count = 2**(math.ceil(math.log2(bin_count)))
        averages = int(averages)
    except:
        help()
    print("loading data")
    data = byte_reader(path, sample)
    print("estimated size: %i x %i" % (bin_count,
        int(len(data) / (bin_count*averages))))
    print("crunching fft")
    fft_table = psd(data, bin_count, averages)
    print("drawing image")
    img = heatmap(fft_table)
    print("saving image")
    img.save(path + '.png')



