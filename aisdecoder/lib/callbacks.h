#ifndef CALLBACKS_H
#define CALLBACKS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*receiver_on_level_changed)(float level, int channel, unsigned char high);
typedef void (*decoder_on_nmea_sentence_received)(const char *sentence,
                                          unsigned int length,
                                          unsigned char sentences,
                                          unsigned char sentencenum);

extern receiver_on_level_changed on_sound_level_changed;
extern decoder_on_nmea_sentence_received on_nmea_sentence_received;

#ifdef __cplusplus
}
#endif
#endif // CALLBACKS_H
