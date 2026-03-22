#include "../include/audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>

#define WAV_RIFF_MAGIC   0x46464952
#define WAV_WAVE_MAGIC   0x45564157
#define WAV_FMT_MAGIC    0x20746D66
#define WAV_DATA_MAGIC   0x61746164

#pragma pack(push, 1)
typedef struct {
    uint32_t riff_magic;
    uint32_t file_size;
    uint32_t wave_magic;
} WavHeader;

typedef struct {
    uint32_t fmt_magic;
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} WavFmtHeader;

typedef struct {
    uint32_t data_magic;
    uint32_t data_size;
} WavDataHeader;
#pragma pack(pop)

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} DynamicBuffer;

static void dynbuf_init(DynamicBuffer *buf)
{
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

static int dynbuf_grow(DynamicBuffer *buf, size_t needed)
{
    if (buf->size + needed <= buf->capacity) return 0;
    
    size_t new_cap = buf->capacity == 0 ? 4096 : buf->capacity * 2;
    while (new_cap < buf->size + needed) new_cap *= 2;
    
    uint8_t *new_data = (uint8_t *)realloc(buf->data, new_cap);
    if (!new_data) return -1;
    
    buf->data = new_data;
    buf->capacity = new_cap;
    return 0;
}

static void dynbuf_append(DynamicBuffer *buf, const uint8_t *data, size_t len)
{
    if (dynbuf_grow(buf, len) == 0) {
        memcpy(buf->data + buf->size, data, len);
        buf->size += len;
    }
}

static void dynbuf_free(DynamicBuffer *buf)
{
    if (buf->data) free(buf->data);
    dynbuf_init(buf);
}

AudioFormat audio_detect_format(const uint8_t *data, size_t data_size)
{
    if (data_size < 12) return AUDIO_FORMAT_UNKNOWN;

    if (memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WAVE", 4) == 0) {
        return AUDIO_FORMAT_WAV;
    }

    if (memcmp(data, "OggS", 4) == 0) {
        return AUDIO_FORMAT_OGG;
    }

    if (data[0] == 0xFF || (data[0] == 0xF3 && (data[1] & 0xEC) == 0xEC) ||
        (data[0] == 0xFB && (data[1] & 0xE0) == 0xE0)) {
        return AUDIO_FORMAT_MP3;
    }

    return AUDIO_FORMAT_UNKNOWN;
}

static int parse_wav(const uint8_t *data, size_t data_size, AudioData *audio)
{
    if (data_size < sizeof(WavHeader) + sizeof(WavFmtHeader) + sizeof(WavDataHeader)) {
        return -1;
    }

    const WavHeader *wav_header = (const WavHeader *)data;
    const WavFmtHeader *fmt_header = (const WavFmtHeader *)(data + sizeof(WavHeader));
    const WavDataHeader *data_header = (const WavDataHeader *)(data + sizeof(WavHeader) + sizeof(WavFmtHeader));

    if (wav_header->riff_magic != WAV_RIFF_MAGIC ||
        wav_header->wave_magic != WAV_WAVE_MAGIC ||
        fmt_header->fmt_magic != WAV_FMT_MAGIC ||
        data_header->data_magic != WAV_DATA_MAGIC) {
        return -1;
    }

    audio->sample_rate = fmt_header->sample_rate;
    audio->channels = fmt_header->num_channels;
    audio->bits_per_sample = fmt_header->bits_per_sample;
    audio->format = AUDIO_FORMAT_WAV;
    
    const uint8_t *audio_data = data + sizeof(WavHeader) + sizeof(WavFmtHeader) + sizeof(WavDataHeader);
    size_t audio_size = data_header->data_size;

    audio->data = (uint8_t *)malloc(audio_size);
    if (!audio->data) return -1;

    memcpy(audio->data, audio_data, audio_size);
    audio->data_size = audio_size;

    return 0;
}

int audio_load(AudioData *audio, const uint8_t *data, size_t data_size)
{
    memset(audio, 0, sizeof(*audio));

    AudioFormat format = audio_detect_format(data, data_size);
    
    if (format == AUDIO_FORMAT_WAV) {
        return parse_wav(data, data_size, audio);
    }

    if (format == AUDIO_FORMAT_UNKNOWN) {
        return -1;
    }

    audio->format = format;
    audio->data = (uint8_t *)malloc(data_size);
    if (!audio->data) return -1;

    memcpy(audio->data, data, data_size);
    audio->data_size = data_size;

    audio->sample_rate = 44100;
    audio->channels = 2;
    audio->bits_per_sample = 16;

    return 0;
}

void audio_free(AudioData *audio)
{
    if (!audio) return;
    if (audio->data) {
        free(audio->data);
        audio->data = NULL;
    }
    audio->data_size = 0;
}

int audio_save_wav(AudioData *audio, const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;

    size_t data_size = audio->data_size;
    size_t file_size = 44 + data_size;

    WavHeader header = {
        WAV_RIFF_MAGIC,
        (uint32_t)(file_size - 8),
        WAV_WAVE_MAGIC
    };
    fwrite(&header, sizeof(header), 1, fp);

    WavFmtHeader fmt = {
        WAV_FMT_MAGIC,
        16,
        1,
        audio->channels,
        audio->sample_rate,
        audio->sample_rate * audio->channels * (audio->bits_per_sample / 8),
        (uint16_t)(audio->channels * (audio->bits_per_sample / 8)),
        audio->bits_per_sample
    };
    fwrite(&fmt, sizeof(fmt), 1, fp);

    WavDataHeader data_header = {
        WAV_DATA_MAGIC,
        (uint32_t)data_size
    };
    fwrite(&data_header, sizeof(data_header), 1, fp);

    fwrite(audio->data, 1, data_size, fp);
    fclose(fp);

    return 0;
}

int audio_save_ogg(AudioData *audio, const char *path, int quality)
{
    uint8_t *ogg_data;
    size_t ogg_size;
    
    if (audio_convert_to_ogg(audio, &ogg_data, &ogg_size, quality) != 0) {
        return -1;
    }
    
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(ogg_data);
        return -1;
    }
    
    size_t written = fwrite(ogg_data, 1, ogg_size, fp);
    fclose(fp);
    free(ogg_data);
    
    return (written == ogg_size) ? 0 : -1;
}

int audio_convert_to_ogg(AudioData *audio, uint8_t **out_ogg, size_t *out_size, int quality)
{
    if (!audio || !audio->data || audio->data_size == 0) {
        return -1;
    }
    
    if (audio->format == AUDIO_FORMAT_OGG) {
        *out_ogg = (uint8_t *)malloc(audio->data_size);
        if (!*out_ogg) return -1;
        memcpy(*out_ogg, audio->data, audio->data_size);
        *out_size = audio->data_size;
        return 0;
    }
    
    vorbis_info vi;
    vorbis_comment vc;
    vorbis_dsp_state vd;
    vorbis_block vb;
    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;
    
    vorbis_info_init(&vi);
    
    float q = 0.1f + (quality * 0.09f);
    if (q > 1.0f) q = 1.0f;
    if (q < 0.1f) q = 0.1f;
    
    int ret = vorbis_encode_init_vbr(&vi, audio->channels, audio->sample_rate, q);
    if (ret != 0) {
        vorbis_info_clear(&vi);
        return -1;
    }
    
    vorbis_comment_init(&vc);
    vorbis_comment_add_tag(&vc, "ENCODER", "f3tool");
    
    ret = vorbis_analysis_init(&vd, &vi);
    if (ret != 0) {
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);
        return -1;
    }
    
    ret = vorbis_block_init(&vd, &vb);
    if (ret != 0) {
        vorbis_dsp_clear(&vd);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);
        return -1;
    }
    
    srand((unsigned int)time(NULL));
    ogg_stream_init(&os, rand());
    
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;
    
    ret = vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
    if (ret != 0) {
        ogg_stream_clear(&os);
        vorbis_block_clear(&vb);
        vorbis_dsp_clear(&vd);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);
        return -1;
    }
    
    ogg_stream_packetin(&os, &header);
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);
    
    DynamicBuffer obuf;
    dynbuf_init(&obuf);
    
    while (1) {
        int result = ogg_stream_flush(&os, &og);
        if (result == 0) break;
        dynbuf_append(&obuf, (uint8_t *)og.header, og.header_len);
        dynbuf_append(&obuf, (uint8_t *)og.body, og.body_len);
    }
    
    int16_t *samples = (int16_t *)audio->data;
    size_t sample_count = audio->data_size / sizeof(int16_t);
    size_t samples_per_channel = sample_count / audio->channels;
    
    float **vorbis_buffer = vorbis_analysis_buffer(&vd, samples_per_channel);
    
    for (size_t i = 0; i < samples_per_channel; i++) {
        for (int ch = 0; ch < audio->channels; ch++) {
            size_t idx = i * audio->channels + ch;
            if (idx < sample_count) {
                vorbis_buffer[ch][i] = samples[idx] / 32768.0f;
            } else {
                vorbis_buffer[ch][i] = 0.0f;
            }
        }
    }
    
    ret = vorbis_analysis_wrote(&vd, samples_per_channel);
    if (ret != 0) {
        dynbuf_free(&obuf);
        ogg_stream_clear(&os);
        vorbis_block_clear(&vb);
        vorbis_dsp_clear(&vd);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);
        return -1;
    }
    
    int eos = 0;
    while (vorbis_analysis_blockout(&vd, &vb) == 1) {
        vorbis_analysis(&vb, NULL);
        vorbis_bitrate_addblock(&vb);
        
        while (vorbis_bitrate_flushpacket(&vd, &op)) {
            ogg_stream_packetin(&os, &op);
        }
    }
    
    while (!eos) {
        int result = ogg_stream_pageout(&os, &og);
        if (result == 0) break;
        dynbuf_append(&obuf, (uint8_t *)og.header, og.header_len);
        dynbuf_append(&obuf, (uint8_t *)og.body, og.body_len);
        if (ogg_page_eos(&og)) eos = 1;
    }
    
    ret = vorbis_analysis_wrote(&vd, 0);
    while (!eos) {
        int result = ogg_stream_pageout(&os, &og);
        if (result == 0) break;
        dynbuf_append(&obuf, (uint8_t *)og.header, og.header_len);
        dynbuf_append(&obuf, (uint8_t *)og.body, og.body_len);
        if (ogg_page_eos(&og)) eos = 1;
    }
    
    *out_ogg = obuf.data;
    *out_size = obuf.size;
    
    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
    
    return 0;
}

int audio_convert_to_wav(AudioData *audio, uint8_t **out_wav, size_t *out_size)
{
    (void)audio;
    (void)out_wav;
    (void)out_size;
    return -1;
}