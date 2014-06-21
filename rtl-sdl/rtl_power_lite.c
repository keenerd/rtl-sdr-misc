/*
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
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

// a quick and horrible hack job of rtl_power.c
// 1024 element FFT
// no downsampling
// dedicated thread
// external flags for retune, gain change, data ready, quit
// todo, preface with fft_

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>

#include <math.h>
#include <pthread.h>
#include <libusb.h>

#include "rtl-sdr.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define FFT_LEVEL                 10
#define FFT_STACK                 4
#define FFT_SIZE                  (1 << FFT_LEVEL)
#define DEFAULT_BUF_LENGTH        (2 * FFT_SIZE * FFT_STACK)
#define BUFFER_DUMP               (1<<12)
#define DEFAULT_ASYNC_BUF_NUMBER  32
#define SAMPLE_RATE               3200000
#define PRESCALE                  8
#define POSTSCALE                 2
#define FREQ_MIN                  27000000
#define FREQ_MAX                  1700000000

struct buffer
{
    // each buffer should have one writer and one reader thread
    // the reader waits for the cond
    int16_t buf[DEFAULT_BUF_LENGTH];
    int len;
    pthread_rwlock_t rw;
    pthread_cond_t ready;
    pthread_mutex_t ready_m;    
    int ready_fast;
};

// shared items

static volatile int do_exit = 0;
static rtlsdr_dev_t *dev = NULL;
static struct buffer fft_out;
static int frequency = 97000000;

// local items

struct buffer rtl_out;
struct buffer fft_tmp;

int16_t* Sinewave;
double* power_table;
int N_WAVE, LOG2_N_WAVE;
int next_power;
int16_t *fft_buf;
int *window_coefs;

pthread_t dongle_thread;
pthread_t fft_thread;

#define safe_cond_signal(n, m) pthread_mutex_lock(m); pthread_cond_signal(n); pthread_mutex_unlock(m)
#define safe_cond_wait(n, m) pthread_mutex_lock(m); pthread_cond_wait(n, m); pthread_mutex_unlock(m)

// some functions from convenience.c

void gain_default(void)
{
    int count;
    int* gains;
    count = rtlsdr_get_tuner_gains(dev, NULL);
    if (count <= 0)
        {return;}
    gains = malloc(sizeof(int) * count);
    count = rtlsdr_get_tuner_gains(dev, gains);
    rtlsdr_set_tuner_gain(dev, gains[count-1]);
    free(gains);
}

void gain_increase(void)
{
    int i, g, count;
    int* gains;
    count = rtlsdr_get_tuner_gains(dev, NULL);
    if (count <= 0)
        {return;}
    gains = malloc(sizeof(int) * count);
    count = rtlsdr_get_tuner_gains(dev, gains);
    g = rtlsdr_get_tuner_gain(dev);
    for (i=0; i<(count-1); i++)
    {
        if (gains[i] == g)
        {
            rtlsdr_set_tuner_gain(dev, gains[i+1]);
            break;
        } 
    }
    free(gains);
}

void gain_decrease(void)
{
    int i, g, count;
    int* gains;
    count = rtlsdr_get_tuner_gains(dev, NULL);
    if (count <= 0)
        {return;}
    gains = malloc(sizeof(int) * count);
    count = rtlsdr_get_tuner_gains(dev, gains);
    g = rtlsdr_get_tuner_gain(dev);
    for (i=1; i<count; i++)
    {
        if (gains[i] == g)
        {
            rtlsdr_set_tuner_gain(dev, gains[i-1]);
            break;
        } 
    }
    free(gains);
}

void frequency_set(void)
{
    if (frequency < FREQ_MIN)
        {frequency = FREQ_MIN;}
    if (frequency > FREQ_MAX)
        {frequency = FREQ_MAX;}
    rtlsdr_set_center_freq(dev, frequency);
}

// fft stuff

void sine_table(int size)
{
    int i;
    double d;
    LOG2_N_WAVE = size;
    N_WAVE = 1 << LOG2_N_WAVE;
    Sinewave = malloc(sizeof(int16_t) * N_WAVE*3/4);
    power_table = malloc(sizeof(double) * N_WAVE);
    for (i=0; i<N_WAVE*3/4; i++)
    {
        d = (double)i * 2.0 * M_PI / N_WAVE;
        Sinewave[i] = (int)round(32767*sin(d));
    }
}

inline int16_t FIX_MPY(int16_t a, int16_t b)
/* fixed point multiply and scale */
{
	int c = ((int)a * (int)b) >> 14;
	b = c & 0x01;
	return (c >> 1) + b;
}

int fix_fft(int16_t iq[], int m)
/* interleaved iq[], 0 <= n < 2**m, changes in place */
{
	int mr, nn, i, j, l, k, istep, n, shift;
	int16_t qr, qi, tr, ti, wr, wi;
	n = 1 << m;
	if (n > N_WAVE)
		{return -1;}
	mr = 0;
	nn = n - 1;
	/* decimation in time - re-order data */
	for (m=1; m<=nn; ++m) {
		l = n;
		do
			{l >>= 1;}
		while (mr+l > nn);
		mr = (mr & (l-1)) + l;
		if (mr <= m)
			{continue;}
		// real = 2*m, imag = 2*m+1
		tr = iq[2*m];
		iq[2*m] = iq[2*mr];
		iq[2*mr] = tr;
		ti = iq[2*m+1];
		iq[2*m+1] = iq[2*mr+1];
		iq[2*mr+1] = ti;
	}
	l = 1;
	k = LOG2_N_WAVE-1;
	while (l < n) {
		shift = 1;
		istep = l << 1;
		for (m=0; m<l; ++m) {
			j = m << k;
			wr =  Sinewave[j+N_WAVE/4];
			wi = -Sinewave[j];
			if (shift) {
				wr >>= 1; wi >>= 1;}
			for (i=m; i<n; i+=istep) {
				j = i + l;
				tr = FIX_MPY(wr,iq[2*j]) - FIX_MPY(wi,iq[2*j+1]);
				ti = FIX_MPY(wr,iq[2*j+1]) + FIX_MPY(wi,iq[2*j]);
				qr = iq[2*i];
				qi = iq[2*i+1];
				if (shift) {
					qr >>= 1; qi >>= 1;}
				iq[2*j] = qr - tr;
				iq[2*j+1] = qi - ti;
				iq[2*i] = qr + tr;
				iq[2*i+1] = qi + ti;
			}
		}
		--k;
		l = istep;
	}
	return 0;
}

void remove_dc(int16_t *data, int length)
/* works on interleaved data */
{
	int i;
	int16_t ave;
	long sum = 0L;
	for (i=0; i < length; i+=2) {
		sum += data[i];
	}
	ave = (int16_t)(sum / (long)(length));
	if (ave == 0) {
		return;}
	for (i=0; i < length; i+=2) {
		data[i] -= ave;
	}
}

int32_t real_conj(int16_t real, int16_t imag)
/* real(n * conj(n)) */
{
    return ((int32_t)real*(int32_t)real + (int32_t)imag*(int32_t)imag);
}

// threading stuff

void rtl_callback_fn(unsigned char *buf, uint32_t len, void *ctx)
{
    int i;
    if (do_exit)
        {return;}
    pthread_rwlock_wrlock(&rtl_out.rw);
    for (i=0; i<len; i++)
    {
        rtl_out.buf[i] = ((int16_t)buf[i]) - 127;
    }
    rtl_out.len = len;
    pthread_rwlock_unlock(&rtl_out.rw);
    safe_cond_signal(&rtl_out.ready, &rtl_out.ready_m);
}

void* dongle_thread_fn(void *arg)
{
    rtlsdr_read_async(dev, rtl_callback_fn, NULL,
        DEFAULT_ASYNC_BUF_NUMBER, DEFAULT_BUF_LENGTH);
    return 0;
}

void* fft_thread_fn(void *arg)
{
    int i, i2, p, len, offset;
    int16_t buf1[DEFAULT_BUF_LENGTH];
    int16_t buf2[DEFAULT_BUF_LENGTH];
    while (!do_exit)
    {
        safe_cond_wait(&rtl_out.ready, &rtl_out.ready_m);
        pthread_rwlock_rdlock(&rtl_out.rw);
        for (i=0; i<rtl_out.len; i++)
        {
            buf1[i] = rtl_out.buf[i] * PRESCALE;
        }
        len = rtl_out.len;
        pthread_rwlock_unlock(&rtl_out.rw);
        // compute
        //remove_dc(fft_buf, buf_len / ds);
        //remove_dc(fft_buf+1, (buf_len / ds) - 1);
        for (offset=0; offset<len; offset+=(2*FFT_SIZE))
        {
            fix_fft(buf1+offset, FFT_LEVEL);
            for (i=0; i<FFT_SIZE; i++)
            {
                if (offset == 0)
                    {buf2[i] = 0;}
                //buf1[i] = rtl_out.buf[i];
                //p = buf1[i] * buf1[i];
                i2 = offset + i*2;
                p = (int16_t)real_conj(buf1[i2], buf1[i2 + 1]);
                buf2[i] += p;
            }
        }
        pthread_rwlock_wrlock(&fft_out.rw);
        fft_out.len = FFT_SIZE;
        // fft is 180 degrees off
        len = FFT_SIZE / 2;
        for (i=0; i<len; i++)
        {
            fft_out.buf[i] = (int)log10(POSTSCALE * (float)buf2[i+len]);
            fft_out.buf[i+len] = (int)log10(POSTSCALE * (float)buf2[i]);
        }
        pthread_rwlock_unlock(&fft_out.rw);
        safe_cond_signal(&fft_out.ready, &fft_out.ready_m);
        fft_out.ready_fast = 1;
    }
    return 0;
}

int buffer_init(struct buffer* buf)
{
    pthread_rwlock_init(&buf->rw, NULL);
    pthread_cond_init(&buf->ready, NULL);
    pthread_mutex_init(&buf->ready_m, NULL);
    return 0;
}

int buffer_cleanup(struct buffer* buf)
{
    pthread_rwlock_destroy(&buf->rw);
    pthread_cond_destroy(&buf->ready);
    pthread_mutex_destroy(&buf->ready_m);
    return 0;
}

static int fft_launch(void)
{
    sine_table(FFT_LEVEL);

    buffer_init(&rtl_out);
    buffer_init(&fft_tmp);
    buffer_init(&fft_out);

    rtlsdr_open(&dev, 0);  // todo, verbose_device_search()

    // settings
    rtlsdr_reset_buffer(dev);
    rtlsdr_set_center_freq(dev, frequency);
    rtlsdr_set_sample_rate(dev, SAMPLE_RATE);
    rtlsdr_set_tuner_gain_mode(dev, 1);
    gain_default();

    pthread_create(&dongle_thread, NULL, &dongle_thread_fn, NULL);
    pthread_create(&fft_thread, NULL, &fft_thread_fn, NULL);
    return 0;
}

static int fft_cleanup(void)
{
    do_exit = 1;
    usleep(10000);
    rtlsdr_cancel_async(dev);
    pthread_join(dongle_thread, NULL);
    safe_cond_signal(&rtl_out.ready, &rtl_out.ready_m);
    pthread_join(fft_thread, NULL);
    safe_cond_signal(&fft_out.ready, &fft_out.ready_m);

    rtlsdr_close(dev);

    buffer_cleanup(&rtl_out);
    buffer_cleanup(&fft_tmp);
    buffer_cleanup(&fft_out);

    return 0;
}

// vim: tabstop=4:softtabstop=4:shiftwidth=4:expandtab
