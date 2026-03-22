#ifndef F3_IMAGE_H
#define F3_IMAGE_H

#include <stdint.h>
#include <stddef.h>

#define DDS_MAGIC         0x20534444  /* "DDS " in little-endian */

#define DDS_FLAG_CAPS        0x1
#define DDS_FLAG_HEIGHT       0x2
#define DDS_FLAG_WIDTH        0x4
#define DDS_FLAG_PIXELFORMAT 0x1000

#define DDS_PF_ALPHAPIXELS  0x1
#define DDS_PF_ALPHA        0x2
#define DDS_PF_FOURCC       0x4
#define DDS_PF_RGB          0x40

#define DDS_FOURCC_DXT1  0x31545844
#define DDS_FOURCC_DXT3  0x33545844
#define DDS_FOURCC_DXT5  0x35545844

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitch_or_linear_size;
    uint32_t depth;
    uint32_t mipmap_count;
    uint32_t reserved1[11];
} DDSHeader;

typedef struct {
    uint32_t size;
    uint32_t flags;
    uint32_t fourcc;
    uint32_t rgb_bit_count;
    uint32_t r_bit_mask;
    uint32_t g_bit_mask;
    uint32_t b_bit_mask;
    uint32_t a_bit_mask;
} DDSPixelFormat;

typedef struct {
    DDSHeader header;
    DDSPixelFormat pf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
} DDSFileHeader;

#pragma pack(pop)

typedef struct {
    uint8_t *data;
    size_t data_size;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    int has_alpha;
} DDSImage;

int dds_parse(DDSImage *image, const uint8_t *data, size_t data_size);
void dds_free(DDSImage *image);

int dds_decode_to_rgba(DDSImage *dds, uint8_t **out_rgba, size_t *out_size);
int dds_save_png(DDSImage *dds, const char *path);

int dds_get_width(DDSImage *image);
int dds_get_height(DDSImage *image);

#endif /* F3_IMAGE_H */
