#include "../include/image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>

int dds_parse(DDSImage *image, const uint8_t *data, size_t data_size)
{
    memset(image, 0, sizeof(*image));

    if (data_size < sizeof(DDSHeader) + 4) {
        return -1;
    }

    uint32_t magic = *(uint32_t *)data;
    if (magic != DDS_MAGIC) {
        fprintf(stderr, "Not a DDS file: magic 0x%X, expected 0x%X\n",
                magic, DDS_MAGIC);
        return -1;
    }

    const DDSHeader *header = (const DDSHeader *)(data + 4);
    
    image->width = header->width;
    image->height = header->height;
    image->data = (uint8_t *)malloc(data_size - sizeof(DDSHeader) - 4);
    
    if (!image->data) {
        return -1;
    }

    memcpy(image->data, data + 4 + sizeof(DDSHeader), data_size - sizeof(DDSHeader) - 4);
    image->data_size = data_size - sizeof(DDSHeader) - 4;

    const DDSPixelFormat *pf = (const DDSPixelFormat *)(data + 4 + sizeof(DDSHeader));
    
    if (pf->flags & DDS_PF_FOURCC) {
        if (pf->fourcc == DDS_FOURCC_DXT1) {
            image->format = 1;
        } else if (pf->fourcc == DDS_FOURCC_DXT3) {
            image->format = 2;
        } else if (pf->fourcc == DDS_FOURCC_DXT5) {
            image->format = 3;
        } else {
            image->format = 0;
        }
        image->has_alpha = (pf->fourcc == DDS_FOURCC_DXT1 || pf->fourcc == DDS_FOURCC_DXT5);
    } else if (pf->flags & DDS_PF_ALPHAPIXELS) {
        if (pf->rgb_bit_count == 32) {
            image->format = 4;
            image->has_alpha = 1;
        }
    }

    return 0;
}

void dds_free(DDSImage *image)
{
    if (!image) return;
    if (image->data) {
        free(image->data);
        image->data = NULL;
    }
    image->data_size = 0;
}

static void dxt1_decode_block(uint8_t *out, const uint8_t *block)
{
    uint16_t c0 = *(uint16_t *)block;
    uint16_t c1 = *(uint16_t *)(block + 2);
    
    uint8_t r0 = (c0 & 0x1F) << 3;
    uint8_t g0 = ((c0 >> 5) & 0x3F) << 2;
    uint8_t b0 = ((c0 >> 11) & 0x1F) << 3;
    
    uint8_t r1 = (c1 & 0x1F) << 3;
    uint8_t g1 = ((c1 >> 5) & 0x3F) << 2;
    uint8_t b1 = ((c1 >> 11) & 0x1F) << 3;
    
    uint32_t indices = *(uint32_t *)(block + 4);
    
    for (int i = 0; i < 16; i++) {
        int idx = (indices >> (i * 2)) & 0x03;
        uint8_t r, g, b, a = 255;
        
        if (c0 > c1) {
            switch (idx) {
                case 0: r = r0; g = g0; b = b0; break;
                case 1: r = r1; g = g1; b = b1; break;
                case 2: r = (2*r0 + r1) / 3; g = (2*g0 + g1) / 3; b = (2*b0 + b1) / 3; break;
                case 3: r = (r0 + 2*r1) / 3; g = (g0 + 2*g1) / 3; b = (b0 + 2*b1) / 3; break;
            }
        } else {
            switch (idx) {
                case 0: r = r0; g = g0; b = b0; break;
                case 1: r = r1; g = g1; b = b1; break;
                case 2: r = (r0 + r1) / 2; g = (g0 + g1) / 2; b = (b0 + b1) / 2; break;
                case 3: r = 0; g = 0; b = 0; a = 0; break;
            }
        }
        
        out[i * 4 + 0] = r;
        out[i * 4 + 1] = g;
        out[i * 4 + 2] = b;
        out[i * 4 + 3] = a;
    }
}

static void dxt5_decode_block(uint8_t *out, const uint8_t *block)
{
    uint16_t c0 = *(uint16_t *)block;
    uint16_t c1 = *(uint16_t *)(block + 2);
    
    uint8_t r0 = (c0 & 0x1F) << 3;
    uint8_t g0 = ((c0 >> 5) & 0x3F) << 2;
    uint8_t b0 = ((c0 >> 11) & 0x1F) << 3;
    
    uint8_t r1 = (c1 & 0x1F) << 3;
    uint8_t g1 = ((c1 >> 5) & 0x3F) << 2;
    uint8_t b1 = ((c1 >> 11) & 0x1F) << 3;
    
    uint64_t alpha_indices = *(uint64_t *)block & 0xFFFFFFFFFFFFULL;
    
    uint8_t a0 = block[8];
    uint8_t a1 = block[9];
    
    for (int i = 0; i < 16; i++) {
        int alpha_idx = (alpha_indices >> (i * 3)) & 0x07;
        uint8_t a;
        
        if (a0 > a1) {
            switch (alpha_idx) {
                case 0: a = a0; break;
                case 1: a = a1; break;
                case 2: a = (6*a0 + 1*a1) / 7; break;
                case 3: a = (5*a0 + 2*a1) / 7; break;
                case 4: a = (4*a0 + 3*a1) / 7; break;
                case 5: a = (3*a0 + 4*a1) / 7; break;
                case 6: a = (2*a0 + 5*a1) / 7; break;
                case 7: a = (1*a0 + 6*a1) / 7; break;
            }
        } else {
            switch (alpha_idx) {
                case 0: a = a0; break;
                case 1: a = a1; break;
                case 2: a = (4*a0 + 1*a1) / 5; break;
                case 3: a = (3*a0 + 2*a1) / 5; break;
                case 4: a = (2*a0 + 3*a1) / 5; break;
                case 5: a = (1*a0 + 4*a1) / 5; break;
                case 6: a = 0; break;
                case 7: a = 255; break;
            }
        }
        
        out[i * 4 + 3] = a;
    }
    
    uint32_t indices = *(uint32_t *)(block + 12);
    
    for (int i = 0; i < 16; i++) {
        int idx = (indices >> (i * 2)) & 0x03;
        uint8_t r, g, b;
        
        if (c0 > c1) {
            switch (idx) {
                case 0: r = r0; g = g0; b = b0; break;
                case 1: r = r1; g = g1; b = b1; break;
                case 2: r = (2*r0 + r1) / 3; g = (2*g0 + g1) / 3; b = (2*b0 + b1) / 3; break;
                case 3: r = (r0 + 2*r1) / 3; g = (g0 + 2*g1) / 3; b = (b0 + 2*b1) / 3; break;
            }
        } else {
            switch (idx) {
                case 0: r = r0; g = g0; b = b0; break;
                case 1: r = r1; g = g1; b = b1; break;
                case 2: r = (r0 + r1) / 2; g = (g0 + g1) / 2; b = (b0 + b1) / 2; break;
                case 3: r = 0; g = 0; b = 0; break;
            }
        }
        
        out[i * 4 + 0] = r;
        out[i * 4 + 1] = g;
        out[i * 4 + 2] = b;
    }
}

int dds_decode_to_rgba(DDSImage *dds, uint8_t **out_rgba, size_t *out_size)
{
    if (dds->format == 0) {
        return -1;
    }

    size_t rgba_size = dds->width * dds->height * 4;
    uint8_t *rgba = (uint8_t *)malloc(rgba_size);
    if (!rgba) return -1;

    memset(rgba, 0, rgba_size);

    if (dds->format == 4) {
        memcpy(rgba, dds->data, rgba_size);
        *out_rgba = rgba;
        *out_size = rgba_size;
        return 0;
    }

    int block_width = (dds->width + 3) / 4;
    int block_height = (dds->height + 3) / 4;
    int bytes_per_block = (dds->format == 1) ? 8 : 16;

    for (int by = 0; by < block_height; by++) {
        for (int bx = 0; bx < block_width; bx++) {
            const uint8_t *block = dds->data + (by * block_width + bx) * bytes_per_block;
            uint8_t *out_block = rgba + (by * 4 * 4 * dds->width) + (bx * 4 * 4);

            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = bx * 4 + x;
                    int py = by * 4 + y;
                    
                    if (px >= (int)dds->width || py >= (int)dds->height) {
                        continue;
                    }
                    
                    uint8_t *pixel = out_block + y * dds->width * 4 + x * 4;
                    
                    if (dds->format == 1) {
                        dxt1_decode_block(pixel, block);
                    } else {
                        dxt5_decode_block(pixel, block);
                    }
                }
            }
        }
    }

    *out_rgba = rgba;
    *out_size = rgba_size;
    return 0;
}

static void write_png_callback(png_structp png_ptr, png_bytep data, png_size_t length)
{
    FILE *fp = (FILE *)png_get_io_ptr(png_ptr);
    fwrite(data, 1, length, fp);
}

int dds_save_png(DDSImage *dds, const char *path)
{
    uint8_t *rgba;
    size_t rgba_size;
    
    if (dds_decode_to_rgba(dds, &rgba, &rgba_size) != 0) {
        return -1;
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(rgba);
        return -1;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        free(rgba);
        return -1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        free(rgba);
        return -1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        free(rgba);
        return -1;
    }

    png_set_IHDR(png, info, dds->width, dds->height, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * dds->height);
    if (!row_pointers) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        free(rgba);
        return -1;
    }

    for (uint32_t y = 0; y < dds->height; y++) {
        row_pointers[y] = rgba + y * dds->width * 4;
    }

    png_set_rows(png, info, row_pointers);

    png_set_write_fn(png, fp, write_png_callback, NULL);
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, NULL);

    png_destroy_write_struct(&png, &info);
    free(row_pointers);
    fclose(fp);
    free(rgba);

    return 0;
}

int dds_get_width(DDSImage *image)
{
    return image ? (int)image->width : 0;
}

int dds_get_height(DDSImage *image)
{
    return image ? (int)image->height : 0;
}
