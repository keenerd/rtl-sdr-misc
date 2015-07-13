#ifndef SOUNDDECODER_H
#define SOUNDDECODER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOUND_CHANNELS_MONO,
    SOUND_CHANNELS_STEREO,
    SOUND_CHANNELS_LEFT,
    SOUND_CHANNELS_RIGHT
} Sound_Channels;

typedef enum {
#ifdef HAVE_ALSA
    DRIVER_ALSA,
#endif
#ifdef HAVE_PULSEAUDIO
    DRIVER_PULSE,
#endif
#ifdef WIN32
    DRIVER_WINMM,
#endif
    DRIVER_FILE
} Sound_Driver;

extern char errorSoundDecoder[];
int initSoundDecoder(int buf_len,int _time_print_stats);
void runSoundDecoder(int *stop);
void freeSoundDecoder(void);
void run_mem_decoder(short * buf, int len,int max_buf_len);

#ifdef __cplusplus
}
#endif
#endif // SOUNDDECODER_H
