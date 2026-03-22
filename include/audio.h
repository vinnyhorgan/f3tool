#ifndef F3_AUDIO_H
#define F3_AUDIO_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    AUDIO_FORMAT_WAV,
    AUDIO_FORMAT_MP3,
    AUDIO_FORMAT_OGG,
    AUDIO_FORMAT_UNKNOWN
} AudioFormat;

typedef struct {
    uint8_t *data;
    size_t data_size;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    AudioFormat format;
} AudioData;

AudioFormat audio_detect_format(const uint8_t *data, size_t data_size);

int audio_load(AudioData *audio, const uint8_t *data, size_t data_size);
void audio_free(AudioData *audio);

int audio_convert_to_wav(AudioData *audio, uint8_t **out_wav, size_t *out_size);
int audio_convert_to_ogg(AudioData *audio, uint8_t **out_ogg, size_t *out_size, int quality);

int audio_save_wav(AudioData *audio, const char *path);
int audio_save_ogg(AudioData *audio, const char *path, int quality);

#endif /* F3_AUDIO_H */
