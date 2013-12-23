/*
 * Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/* todo
 * support left > right
 * thread left/right channels
 * more array sharing
 * something to correct for clock drift (look at demod's dc bias?)
 * 4x oversampling (with cic up/down)
 * droop correction
 * alsa integration
 * better upsampler (libsamplerate?)
 * windows support
 * ais decoder
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <unistd.h>

#include <pthread.h>
#include <libusb.h>

#include <rtl-sdr.h>
#include "convenience.h"

#define DEFAULT_ASYNC_BUF_NUMBER	12
#define DEFAULT_BUF_LENGTH		(16 * 16384)
#define AUTO_GAIN			-100

static pthread_t demod_thread;
static pthread_cond_t ready;
static pthread_mutex_t ready_m;
static volatile int do_exit = 0;
static rtlsdr_dev_t *dev = NULL;

/* todo, less globals */
int16_t *merged;
int merged_len;
FILE *file;
int oversample = 0;

/* signals are not threadsafe by default */
#define safe_cond_signal(n, m) pthread_mutex_lock(m); pthread_cond_signal(n); pthread_mutex_unlock(m)
#define safe_cond_wait(n, m) pthread_mutex_lock(m); pthread_cond_wait(n, m); pthread_mutex_unlock(m)

struct downsample_state
{
	int16_t  *buf;
	int      len_in;
	int      len_out;
	int      rate_in;
	int      rate_out;
	int      downsample;
	int      downsample_passes;
	int16_t  lp_i_hist[10][6];
	int16_t  lp_q_hist[10][6];
	pthread_rwlock_t rw;
};

struct demod_state
{
	int16_t  *buf;
	int      buf_len;
	int16_t  *result;
	int      result_len;
	int      now_r, now_j;
	int      pre_r, pre_j;
};

struct upsample_stereo
{
	int16_t *buf_left;
	int16_t *buf_right;
	int16_t *result;
	int     bl_len;
	int     br_len;
	int     result_len;
	int     rate;
};

/* complex iq pairs */
struct downsample_state both;
struct downsample_state left;
struct downsample_state right;
/* iq pairs and real mono */
struct demod_state left_demod;
struct demod_state right_demod;
/* real stereo pairs (upsampled) */
struct upsample_stereo stereo;

void usage(void)
{
	fprintf(stderr,
		"rtl_ais, a simple AIS tuner\n"
		"\t and generic dual-frequency FM demodulator\n\n"
		"(probably not a good idea to use with e4000 tuners)\n"
		"Use: rtl_ais [options] [outputfile]\n"
		"\t[-l left_frequency (default: 161.975M)]\n"
		"\t[-r right_frequency (default: 162.025M)]\n"
		"\t    left freq < right freq\n"
		"\t    frequencies must be within 1.2MHz\n"
		"\t[-s sample_rate (default: 12k)]\n"
		"\t    minimum value, might be up to 2x specified\n"
		"\t[-o output_rate (default: 48k)]\n"
		"\t    must be equal or greater than twice -s value\n"
		"\t[-E toggle edge tuning (default: on)]\n"
		//"\t[-O toggle oversampling (default: off)\n"
		"\t[-d device_index (default: 0)]\n"
		"\t[-g tuner_gain (default: automatic)]\n"
		"\t[-p ppm_error (default: 0)]\n"
		"\tfilename (a '-' dumps samples to stdout)\n"
		"\t    omitting the filename also uses stdout\n\n"
		"\tOutput is stereo 2x16 bit signed ints\n\n"
		"rtl_ais | play -t raw -r 48k -es -b 16 -c 2 -V1 -\n"
		"\n");
	exit(1);
}

static void sighandler(int signum)
{
	fprintf(stderr, "Signal caught, exiting!\n");
	do_exit = 1;
	rtlsdr_cancel_async(dev);
}

void rotate_90(int16_t *buf, int len)
/* 90 rotation is 1+0j, 0+1j, -1+0j, 0-1j
   or [0, 1, -3, 2, -4, -5, 7, -6] */
{
	int i;
	int16_t tmp;
	for (i=0; i<len; i+=8) {
		tmp = buf[i+2];
		buf[i+2] = -buf[i+3];
		buf[i+3] = tmp;

		buf[i+4] = -buf[i+4];
		buf[i+5] = -buf[i+5];

		tmp = buf[i+6];
		buf[i+6] = buf[i+7];
		buf[i+7] = -tmp;
	}
}

void rotate_m90(int16_t *buf, int len)
/* -90 rotation is 1+0j, 0-1j, -1+0j, 0+1j
   or [0, 1, 3, -2, -4, -5, -7, 6] */
{
	int i;
	int16_t tmp;
	for (i=0; i<len; i+=8) {
		tmp = buf[i+2];
		buf[i+2] = buf[i+3];
		buf[i+3] = -tmp;

		buf[i+4] = -buf[i+4];
		buf[i+5] = -buf[i+5];

		tmp = buf[i+6];
		buf[i+6] = -buf[i+7];
		buf[i+7] = tmp;
	}
}

void fifth_order(int16_t *data, int length, int16_t *hist)
/* for half of interleaved data */
{
	int i;
	int16_t a, b, c, d, e, f;
	a = hist[1];
	b = hist[2];
	c = hist[3];
	d = hist[4];
	e = hist[5];
	f = data[0];
	/* a downsample should improve resolution, so don't fully shift */
	data[0] = (a + (b+e)*5 + (c+d)*10 + f) >> 4;
	for (i=4; i<length; i+=4) {
		a = c;
		b = d;
		c = e;
		d = f;
		e = data[i-2];
		f = data[i];
		data[i/2] = (a + (b+e)*5 + (c+d)*10 + f) >> 4;
	}
	/* archive */
	hist[0] = a;
	hist[1] = b;
	hist[2] = c;
	hist[3] = d;
	hist[4] = e;
	hist[5] = f;
}

void downsample(struct downsample_state *d)
{
	int i, ds_p;
	ds_p = d->downsample_passes;
	for (i=0; i<ds_p; i++) {
		fifth_order(d->buf,   (d->len_in >> i),   d->lp_i_hist[i]);
		fifth_order(d->buf+1, (d->len_in >> i)-1, d->lp_q_hist[i]);
	}
}

void multiply(int ar, int aj, int br, int bj, int *cr, int *cj)
{
	*cr = ar*br - aj*bj;
	*cj = aj*br + ar*bj;
}

int polar_discriminant(int ar, int aj, int br, int bj)
{
	int cr, cj;
	double angle;
	multiply(ar, aj, br, -bj, &cr, &cj);
	angle = atan2((double)cj, (double)cr);
	return (int)(angle / 3.14159 * (1<<14));
}

int fast_atan2(int y, int x)
/* pre scaled for int16 */
{
	int yabs, angle;
	int pi4=(1<<12), pi34=3*(1<<12);  // note pi = 1<<14
	if (x==0 && y==0) {
		return 0;
	}
	yabs = y;
	if (yabs < 0) {
		yabs = -yabs;
	}
	if (x >= 0) {
		angle = pi4  - pi4 * (x-yabs) / (x+yabs);
	} else {
		angle = pi34 - pi4 * (x+yabs) / (yabs-x);
	}
	if (y < 0) {
		return -angle;
	}
	return angle;
}

int polar_disc_fast(int ar, int aj, int br, int bj)
{
	int cr, cj;
	multiply(ar, aj, br, -bj, &cr, &cj);
	return fast_atan2(cj, cr);
}

void demodulate(struct demod_state *d)
{
	int i, pcm;
	int16_t *buf = d->buf;
	int16_t *result = d->result;
	pcm = polar_disc_fast(buf[0], buf[1],
		d->pre_r, d->pre_j);
	result[0] = (int16_t)pcm;
	for (i = 2; i < (d->buf_len-1); i += 2) {
		// add the other atan types?
		pcm = polar_disc_fast(buf[i], buf[i+1],
			buf[i-2], buf[i-1]);
		result[i/2] = (int16_t)pcm;
	}
	d->pre_r = buf[d->buf_len - 2];
	d->pre_j = buf[d->buf_len - 1];
}


void arbitrary_upsample(int16_t *buf1, int16_t *buf2, int len1, int len2)
/* linear interpolation, len1 < len2 */
{
	int i = 1;
	int j = 0;
	int tick = 0;
	double frac;  // use integers...
	while (j < len2) {
		frac = (double)tick / (double)len2;
		buf2[j] = (int16_t)((double)buf1[i-1]*(1-frac) + (double)buf1[i]*frac);
		j++;
		tick += len1;
		if (tick > len2) {
			tick -= len2;
			i++;
		}
		if (i >= len1) {
			i = len1 - 1;
			tick = len2;
		}
	}
}

static void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx)
{
	int i;
	if (do_exit) {
		return;}
	pthread_rwlock_wrlock(&both.rw);
	for (i=0; i<len; i++) {
		both.buf[i] = ((int16_t)buf[i]) - 127;
	}
	pthread_rwlock_unlock(&both.rw);
	safe_cond_signal(&ready, &ready_m);
}

void output(void)
{
	int i;
	for (i=0; i<stereo.bl_len; i++) {
		stereo.result[i*2]   = stereo.buf_left[i];
		stereo.result[i*2+1] = stereo.buf_right[i];
	}
	fwrite(stereo.result, 2, stereo.result_len, file);
}

static void *demod_thread_fn(void *arg)
{
	while (!do_exit) {
		safe_cond_wait(&ready, &ready_m);
		pthread_rwlock_wrlock(&both.rw);
		downsample(&both);
		memcpy(left.buf,  both.buf, 2*both.len_out);
		memcpy(right.buf, both.buf, 2*both.len_out);
		pthread_rwlock_unlock(&both.rw);
		rotate_90(left.buf, left.len_in);
		downsample(&left);
		memcpy(left_demod.buf, left.buf, 2*left.len_out);
		demodulate(&left_demod);
		//if (oversample) {
		//	downsample(&left);}
		arbitrary_upsample(left_demod.result, stereo.buf_left, left_demod.result_len, stereo.bl_len);
		rotate_m90(right.buf, right.len_in);
		downsample(&right);
		memcpy(right_demod.buf, right.buf, 2*right.len_out);
		demodulate(&right_demod);
		//if (oversample) {
		//	downsample(&right);}
		arbitrary_upsample(right_demod.result, stereo.buf_right, right_demod.result_len, stereo.br_len);
		output();
	}
	rtlsdr_cancel_async(dev);
	return 0;
}

void downsample_init(struct downsample_state *dss)
/* simple ints should be already set */
{
	int i, j;
	dss->buf = malloc(dss->len_in * sizeof(int16_t));
	dss->rate_out = dss->rate_in / dss->downsample;
	//dss->downsample_passes = (int)log2(dss->downsample);
	dss->len_out = dss->len_in / dss->downsample;
	for (i=0; i<10; i++) { for (j=0; j<6; j++) {
		dss->lp_i_hist[i][j] = 0;
		dss->lp_q_hist[i][j] = 0;
	}}
	pthread_rwlock_init(&dss->rw, NULL);
}

void demod_init(struct demod_state *ds)
{
	ds->buf = malloc(ds->buf_len * sizeof(int16_t));
	ds->result = malloc(ds->result_len * sizeof(int16_t));
}

void stereo_init(struct upsample_stereo *us)
{
	us->buf_left  = malloc(us->bl_len * sizeof(int16_t));
	us->buf_right = malloc(us->br_len * sizeof(int16_t));
	us->result    = malloc(us->result_len * sizeof(int16_t));
}

int main(int argc, char **argv)
{
	struct sigaction sigact;
	char *filename = NULL;
	int r, opt;
	int i, gain = AUTO_GAIN; /* tenths of a dB */
	int dev_index = 0;
	int dev_given = 0;
	int ppm_error = 0;
	int custom_ppm = 0;
	int left_freq = 161975000;
	int right_freq = 162025000;
	int sample_rate = 12000;
	int output_rate = 48000;
	int dongle_freq, dongle_rate, delta;
	int edge = 1;
	pthread_cond_init(&ready, NULL);
	pthread_mutex_init(&ready_m, NULL);

	while ((opt = getopt(argc, argv, "l:r:s:o:EOd:g:p:h")) != -1)
	{
		switch (opt) {
		case 'l':
			left_freq = (int)atofs(optarg);
			break;
		case 'r':
			right_freq = (int)atofs(optarg);
			break;
		case 's':
			sample_rate = (int)atofs(optarg);
			break;
		case 'o':
			output_rate = (int)atofs(optarg);
			break;
		case 'E':
			edge = !edge;
			break;
		case 'O':
			oversample = !oversample;
			break;
		case 'd':
			dev_index = verbose_device_search(optarg);
			dev_given = 1;
			break;
		case 'g':
			gain = (int)(atof(optarg) * 10);
			break;
		case 'p':
			ppm_error = atoi(optarg);
			custom_ppm = 1;
			break;
		case 'h':
		default:
			usage();
			return 2;
		}
	}

	if (argc <= optind) {
		filename = "-";
	} else {
		filename = argv[optind];
	}

	if (left_freq > right_freq) {
		usage();
		return 2;
	}

	/* precompute rates */
	dongle_freq = left_freq/2 + right_freq/2;
	if (edge) {
		dongle_freq -= sample_rate/2;}
	delta = right_freq - left_freq;
	if (delta > 1.2e6) {
		fprintf(stderr, "Frequencies may be at most 1.2MHz apart.");
		exit(1);
	}
	if (delta < 0) {
		fprintf(stderr, "Left channel must be lower than right channel.");
		exit(1);
	}
	i = (int)log2(2.4e6 / delta);
	dongle_rate = delta * (1<<i);
	both.rate_in = dongle_rate;
	both.rate_out = delta * 2;
	i = (int)log2(both.rate_in/both.rate_out);
	both.downsample_passes = i;
	both.downsample = 1 << i;
	left.rate_in = both.rate_out;
	i = (int)log2(left.rate_in / sample_rate);
	left.downsample_passes = i;
	left.downsample = 1 << i;
	left.rate_out = left.rate_in / left.downsample;
	
	right.rate_in = left.rate_in;
	right.rate_out = left.rate_out;
	right.downsample = left.downsample;
	right.downsample_passes = left.downsample_passes;

	if (left.rate_out > output_rate) {
		fprintf(stderr, "Channel bandwidth too high or output bandwidth too low.");
		exit(1);
	}

	stereo.rate = output_rate;

	if (edge) {
		fprintf(stderr, "Edge tuning enabled.\n");
	} else {
		fprintf(stderr, "Edge tuning disabled.\n");
	}
	fprintf(stderr, "Buffer size: %0.2f mS\n", 1000 * (double)DEFAULT_BUF_LENGTH / (double)dongle_rate);
	fprintf(stderr, "Downsample factor: %i\n", both.downsample * left.downsample);
	fprintf(stderr, "Low pass: %i Hz\n", left.rate_out);
	fprintf(stderr, "Output: %i Hz\n", output_rate);

	/* precompute lengths */
	both.len_in  = DEFAULT_BUF_LENGTH;
	both.len_out = both.len_in / both.downsample;
	left.len_in  = both.len_out;
	right.len_in = both.len_out;
	left.len_out = left.len_in / left.downsample;
	right.len_out = right.len_in / right.downsample;
	left_demod.buf_len = left.len_out;
	left_demod.result_len = left_demod.buf_len / 2;
	right_demod.buf_len = left_demod.buf_len;
	right_demod.result_len = left_demod.result_len;
	stereo.bl_len = (int)((long)(DEFAULT_BUF_LENGTH/2) * (long)output_rate / (long)dongle_rate);
	stereo.br_len = stereo.bl_len;
	stereo.result_len = stereo.br_len * 2;
	stereo.rate = output_rate;

	if (!dev_given) {
		dev_index = verbose_device_search("0");
	}

	if (dev_index < 0) {
		exit(1);
	}

	downsample_init(&both);
	downsample_init(&left);
	downsample_init(&right);
	demod_init(&left_demod);
	demod_init(&right_demod);
	stereo_init(&stereo);

	r = rtlsdr_open(&dev, (uint32_t)dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);
		exit(1);
	}
	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);

	if (strcmp(filename, "-") == 0) { /* Write samples to stdout */
		file = stdout;
		setvbuf(stdout, NULL, _IONBF, 0);
	} else {
		file = fopen(filename, "wb");
		if (!file) {
			fprintf(stderr, "Failed to open %s\n", filename);
			exit(1);
		}
	}

	/* Set the tuner gain */
	if (gain == AUTO_GAIN) {
		verbose_auto_gain(dev);
	} else {
		gain = nearest_gain(dev, gain);
		verbose_gain_set(dev, gain);
	}

	if (!custom_ppm) {
		verbose_ppm_eeprom(dev, &ppm_error);
	}
	verbose_ppm_set(dev, ppm_error);
	//r = rtlsdr_set_agc_mode(dev, 1);

	/* Set the tuner frequency */
	verbose_set_frequency(dev, dongle_freq);

	/* Set the sample rate */
	verbose_set_sample_rate(dev, dongle_rate);

	/* Reset endpoint before we start reading from it (mandatory) */
	verbose_reset_buffer(dev);

	pthread_create(&demod_thread, NULL, demod_thread_fn, (void *)(NULL));
	rtlsdr_read_async(dev, rtlsdr_callback, (void *)(NULL),
			      DEFAULT_ASYNC_BUF_NUMBER,
			      DEFAULT_BUF_LENGTH);

	if (do_exit) {
		fprintf(stderr, "\nUser cancel, exiting...\n");}
	else {
		fprintf(stderr, "\nLibrary error %d, exiting...\n", r);}
	rtlsdr_cancel_async(dev);
	safe_cond_signal(&ready, &ready_m);
	pthread_cond_destroy(&ready);
	pthread_mutex_destroy(&ready_m);

	if (file != stdout) {
		fclose(file);}

	rtlsdr_close(dev);
	return r >= 0 ? r : -r;
}

// vim: tabstop=8:softtabstop=8:shiftwidth=8:noexpandtab
