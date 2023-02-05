/*

Copyright (c) 2021, Dominic Szablewski - https://phoboslab.org
SPDX-License-Identifier: MIT


SQOA - Losslessly squash image size, fast
based on:
QOI - The "Quite OK Image" format for fast, lossless image compression

-- About

SQOA encodes and decodes images in a lossless format. Compared to stb_image and
stb_image_write SQOA offers 20x-50x faster encoding, 3x-4x faster decoding and
20% better compression.
Compared to QOI, SQOA files are about 0.7% smaller on the sample set with no 
noticeable performance penalty.

-- Synopsis

// Define `SQOA_IMPLEMENTATION` in *one* C/C++ file before including this
// library to create the implementation.

#define SQOA_IMPLEMENTATION
#include "sqoa.h"

// Encode and store an RGBA buffer to the file system. The sqoa_desc describes
// the input pixel data. Set qoi_compat to 1 to write to QOI instead of SQOA.
sqoa_write("image_new.sqoa", rgba_pixels, &(sqoa_desc){
    .width = 1920,
    .height = 1080,
    .channels = 4,
    .colorspace = SQOA_SRGB,
    .qoi_compat = 0
});

// Load and decode a SQOA image from the file system into a 32bbp RGBA buffer.
// The sqoa_desc struct will be filled with the width, height, number of channels
// and colorspace read from the file header. Compatible with QOI images.
sqoa_desc desc;
void *rgba_pixels = sqoa_read("image.sqoa", &desc, 4);



-- Documentation

This library provides the following functions;
- sqoa_read    -- read and decode a SQOA/QOI file
- sqoa_decode  -- decode the raw bytes of a SQOA/QOI image from memory
- sqoa_write   -- encode and write a SQOA/QOI file
- sqoa_encode  -- encode an rgba buffer into a SQOA/QOI image in memory

See the function declaration below for the signature and more information.

If you don't want/need the sqoa_read and sqoa_write functions, you can define
SQOA_NO_STDIO before including this library.

This library uses malloc() and free(). To supply your own malloc implementation
you can define SQOA_MALLOC and SQOA_FREE before including this library.

This library uses memset() to zero-initialize the index. To supply your own
implementation you can define SQOA_ZEROARR before including this library.


-- Data Format

A SQOA file has a 14 byte header, followed by a start byte, any number of data 
"chunks" and an 8-byte end marker.

struct sqoa_header_t {
    char     magic[4];   // magic bytes "Sqoa"
    uint32_t width;      // image width in pixels (BE)
    uint32_t height;     // image height in pixels (BE)
    uint8_t  channels;   // 3 = RGB, 4 = RGBA
    uint8_t  colorspace; // 0 = sRGB with linear alpha, 1 = all channels linear
    uint8_t  qoi_compat; // 0 = no compatibility, 1 = compatible with QOI
};

The value of the start byte is 48 ('0'). If the start byte is missing, the
decoder switches to QOI compatibility mode.

Images are encoded row by row, left to right, top to bottom. The decoder and
encoder start with {r: 0, g: 0, b: 0, a: 255} as the previous pixel value. An
image is complete when all pixels specified by width * height have been covered.

Pixels are encoded as
 - a run of the previous pixel
 - an index into an array of previously seen pixels
 - a difference to the previous pixel value in r,g,b
 - full r,g,b or r,g,b,a values

The color channels are assumed to not be premultiplied with the alpha channel
("un-premultiplied alpha"). The indexed and delta pixels can receive an update
to the alpha channel.

A running array[64] (zero-initialized) of previously seen pixel values is
maintained by the encoder and decoder. Each pixel that is seen by the encoder
and decoder is put into this array at the position formed by a hash function of
the color value. In the encoder, if the pixel value at the index matches the
current pixel, this index position is written to the stream as SQOA_OP_INDEX.
The hash function for the index is:

    index_position = (r * 3 + g * 5 + b * 7 + a * 11) % 64

Each chunk starts with a 2- or 8-bit tag, followed by a number of data bits. The
bit length of chunks is divisible by 8 - i.e. all chunks are byte aligned. All
values encoded in these data bits have the most significant bit on the left.

The 8-bit tags have precedence over the 2-bit tags. A decoder must check for the
presence of an 8-bit tag first.

The byte stream's end is marked with 7 0x00 bytes followed a single 0x01 byte.


The possible chunks are:


.- SQOA_OP_INDEX ---------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----------------|
|  0  0 |     index       |
`-------------------------`
2-bit tag b00
6-bit index into the color index array: 0..63

The alpha value must be updated separately else it remains unchanged.

It is not valid to use SQOA_OP_INDEX to repeat the last pixel once, unless it
differs from the current pixel in alpha channel. If it is identical SQOA_OP_RUN 
should be used instead.


.- SQOA_OP_DIFF ----------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----+-----+-----|
|  0  1 |  dr |  dg |  db |
`-------------------------`
2-bit tag b01
2-bit   red channel difference from the previous pixel between -2..1
2-bit green channel difference from the previous pixel between -2..1
2-bit  blue channel difference from the previous pixel between -2..1

The difference to the current channel values are using a wraparound operation,
so "1 - 2" will result in 255, while "255 + 1" will result in 0.

Values are stored as unsigned integers with a bias of 2. E.g. -2 is stored as
0 (b00). 1 is stored as 3 (b11).

It is not valid to have all of dr, dg and db be zero. To repeat the last pixel
once SQOA_OP_RUN should be used instead.

The alpha value must be updated separately else it remains unchanged.


.- SQOA_OP_LUMA ------------------------------------.
|         Byte[0]         |         Byte[1]         |
|  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 |
|-------+-----------------+-------------+-----------|
|  1  0 |  green diff     |   dr - dg   |  db - dg  |
`---------------------------------------------------`
2-bit tag b10
6-bit green channel difference from the previous pixel -32..31
4-bit   red channel difference minus green channel difference -8..7
4-bit  blue channel difference minus green channel difference -8..7

The green channel is used to indicate the general direction of change and is
encoded in 6 bits. The red and blue channels (dr and db) base their diffs off
of the green channel difference and are encoded in 4 bits. I.e.:
    dr_dg = (cur_px.r - prev_px.r) - (cur_px.g - prev_px.g)
    db_dg = (cur_px.b - prev_px.b) - (cur_px.g - prev_px.g)

The difference to the current channel values are using a wraparound operation,
so "10 - 13" will result in 253, while "250 + 7" will result in 1.

Values are stored as unsigned integers with a bias of 32 for the green channel
and a bias of 8 for the red and blue channel.

The alpha value must be updated separately else it remains unchanged.


.- SQOA_OP_ALPHA -------------------.
|         Byte[0]         | Byte[1] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  |
|-------------------------+---------|
|  0  1  1  0  1  0  1  0 |  alpha  |
`-----------------------------------`
8-bit tag b01101010
8-bit alpha channel value

The alpha channel update applies to last pixel.


.- SQOA_OP_RUN -----------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----------------|
|  1  1 |       run       |
`-------------------------`
2-bit tag b11
6-bit run-length repeating the previous pixel: 1..61

The run-length is stored with a bias of -1. 

Note that the run-lengths 62, 63 and 64 (b111101 to b111111)
are illegal since they are reserved for other operations.


.- SQOA_OP_BIGRUN --------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------------------------|
|  1  1  1  1  1  1  0  1 |
`-------------------------`
8-bit tag b11111101

The run length is set to SQOA_MAXRUN (512).


.- SQOA_OP_RGB -----------------------------------------.
|         Byte[0]         | Byte[1] | Byte[2] | Byte[3] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  | 7 .. 0  | 7 .. 0  |
|-------------------------+---------+---------+---------|
|  1  1  1  1  1  1  1  0 |   red   |  green  |  blue   |
`-------------------------------------------------------`
8-bit tag b11111110
8-bit   red channel value
8-bit green channel value
8-bit  blue channel value

The alpha value remains unchanged from the previous pixel.


.- SQOA_OP_RGBA --------------------------------------------------.
|         Byte[0]         | Byte[1] | Byte[2] | Byte[3] | Byte[4] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  | 7 .. 0  | 7 .. 0  | 7 .. 0  |
|-------------------------+---------+---------+---------+---------|
|  1  1  1  1  1  1  1  1 |   red   |  green  |  blue   |  alpha  |
`-----------------------------------------------------------------`
8-bit tag b11111111
8-bit   red channel value
8-bit green channel value
8-bit  blue channel value
8-bit alpha channel value



-- QOI Compatibility Mode

The only differences in compatibility mode are the lack of a start byte and
that QOI_OP_RUN replaces all of SQOA_OP_RUN, SQOA_OP_BIGRUN, SQOA_OP_ALPHA.

.- QOI_OP_RUN ------------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----------------|
|  1  1 |       run       |
`-------------------------`
2-bit tag b11
6-bit run-length repeating the previous pixel: 1..62

The run-length is stored with a bias of -1. Note that the run-lengths 63 and 64
(b111110 and b111111) are illegal as they are occupied by the SQOA_OP_RGB and
SQOA_OP_RGBA tags.


*/


/* -----------------------------------------------------------------------------
Header - Public functions */

#ifndef SQOA_H
#define SQOA_H

#ifdef __cplusplus
extern "C" {
#endif

/* A pointer to a sqoa_desc struct has to be supplied to all of sqoa's functions.
It describes either the input format (for sqoa_write and sqoa_encode), or is
filled with the description read from the file header (for sqoa_read and
sqoa_decode).

The colorspace in this sqoa_desc is an enum where
    0 = sRGB, i.e. gamma scaled RGB channels and a linear alpha channel
    1 = all channels are linear
You may use the constants SQOA_SRGB or SQOA_LINEAR. The colorspace is purely
informative. It will be saved to the file header, but does not affect
how chunks are en-/decoded.
The qoi_compat field indicates if the image is in QOI format (for decode) or
if QOI compatibility is requested (for encode). */

#define SQOA_SRGB   0
#define SQOA_LINEAR 1

typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned char channels;
    unsigned char colorspace;
    unsigned char qoi_compat;
} sqoa_desc;

#ifndef SQOA_NO_STDIO

/* Encode raw RGB or RGBA pixels into a SQOA image and write it to the file
system. The sqoa_desc struct must be filled with the image width, height,
number of channels (3 = RGB, 4 = RGBA) and the colorspace. If qoi_compat is
non-zero, a QOI image is written instead.

The function returns 0 on failure (invalid parameters, or fopen or malloc
failed) or the number of bytes written on success. */

int sqoa_write(const char *filename, const void *data, const sqoa_desc *desc);


/* Read and decode a SQOA image from the file system. If channels is 0, the
number of channels from the file header is used. If channels is 3 or 4 the
output format will be forced into this number of channels.

The function either returns NULL on failure (invalid data, or malloc or fopen
failed) or a pointer to the decoded pixels. On success, the sqoa_desc struct
will be filled with the description from the file header. Can also read a QOI
image which sets the qoi_compat value to 1 in sqoa_desc.

The returned pixel data should be free()d after use. */

void *sqoa_read(const char *filename, sqoa_desc *desc, int channels);

#endif /* SQOA_NO_STDIO */


/* Encode raw RGB or RGBA pixels into a SQOA or QOI image in memory.

The function either returns NULL on failure (invalid parameters or malloc
failed) or a pointer to the encoded data on success. On success the out_len
is set to the size in bytes of the encoded data.

The returned sqoa data should be free()d after use. */

void *sqoa_encode(const void *data, const sqoa_desc *desc, int *out_len);


/* Decode a SQOA or QOI image from memory.

The function either returns NULL on failure (invalid parameters or malloc
failed) or a pointer to the decoded pixels. On success, the sqoa_desc struct
is filled with the description from the file header.

The returned pixel data should be free()d after use. */

void *sqoa_decode(const void *data, int size, sqoa_desc *desc, int channels);


#ifdef __cplusplus
}
#endif
#endif /* SQOA_H */


/* -----------------------------------------------------------------------------
Implementation */

#ifdef SQOA_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>

#ifndef SQOA_MALLOC
    #define SQOA_MALLOC(sz) malloc(sz)
    #define SQOA_FREE(p)    free(p)
#endif
#ifndef SQOA_ZEROARR
    #define SQOA_ZEROARR(a) memset((a),0,sizeof(a))
#endif

#define SQOA_OP_INDEX  0x00 /* 00xxxxxx */
#define SQOA_OP_DIFF   0x40 /* 01xxxxxx */
#define SQOA_OP_LUMA   0x80 /* 10xxxxxx */
#define SQOA_OP_RUN    0xc0 /* 11xxxxxx */
#define SQOA_OP_ALPHA  0x6a /* 01101010 */
#define SQOA_OP_BIGRUN 0xfd /* 11111101 */
#define SQOA_OP_RGB    0xfe /* 11111110 */
#define SQOA_OP_RGBA   0xff /* 11111111 */
//#define QOI_OP_RUN     0xc0 /* 11xxxxxx */

#define SQOA_MASK_2    0xc0 /* 11000000 */

#define SQOA_MAXRUN 512
#define QOI_MAXRUN  62
#define SQOA_RGBA_HASH(R, G, B, A) (R*3 + G*5 + B*7 + A*11)
#define SQOA_COLOR_HASH(C) SQOA_RGBA_HASH(C.rgba.r, C.rgba.g, C.rgba.b, C.rgba.a)
#define SQOA_MAGIC \
    (((unsigned int)'S') << 24 | ((unsigned int)'q') << 16 | \
     ((unsigned int)'o') <<  8 | ((unsigned int)'a'))
#define QOI_MAGIC \
    (((unsigned int)'q') << 24 | ((unsigned int)'o') << 16 | \
     ((unsigned int)'i') <<  8 | ((unsigned int)'f'))
#define SQOA_HEADER_SIZE 14
#define SQOA_START_BYTE 48

/* 2GB is the max file size that this implementation can safely handle. We guard
against anything larger than that, assuming the worst case with 5 bytes per
pixel, rounded down to a nice clean value. 400 million pixels ought to be
enough for anybody. */
#define SQOA_PIXELS_MAX ((unsigned int)400000000)

typedef union {
    struct { unsigned char r, g, b, a; } rgba;
    unsigned int v;
} sqoa_rgba_t;

static const unsigned char sqoa_padding[8] = {0,0,0,0,0,0,0,1};

static void sqoa_write_32(unsigned char *bytes, int *p, unsigned int v) {
    bytes[(*p)++] = (0xff000000 & v) >> 24;
    bytes[(*p)++] = (0x00ff0000 & v) >> 16;
    bytes[(*p)++] = (0x0000ff00 & v) >> 8;
    bytes[(*p)++] = (0x000000ff & v);
}

static unsigned int sqoa_read_32(const unsigned char *bytes, int *p) {
    unsigned int a = bytes[(*p)++];
    unsigned int b = bytes[(*p)++];
    unsigned int c = bytes[(*p)++];
    unsigned int d = bytes[(*p)++];
    return a << 24 | b << 16 | c << 8 | d;
}

void *sqoa_encode(const void *data, const sqoa_desc *desc, int *out_len) {
    int max_size, max_run, p, run, qoi_compat;
    int px_len, px_end, px_pos, channels;
    unsigned char *bytes;
    const unsigned char *pixels;
    sqoa_rgba_t index[64];
    unsigned char alpha[64] = {0xff};
    sqoa_rgba_t px, px_prev;

    if (
        data == NULL || out_len == NULL || desc == NULL ||
        desc->width == 0 || desc->height == 0 ||
        desc->channels < 3 || desc->channels > 4 ||
        desc->colorspace > 1 ||
        desc->height >= SQOA_PIXELS_MAX / desc->width
    ) {
        return NULL;
    }

    max_size =
        desc->width * desc->height * (desc->channels + 1) +
        SQOA_HEADER_SIZE + sizeof(sqoa_padding);

    qoi_compat = desc->qoi_compat;
    p = 0;
    bytes = (unsigned char *) SQOA_MALLOC(max_size);
    if (!bytes) {
        return NULL;
    }

    if (qoi_compat) {
        sqoa_write_32(bytes, &p, QOI_MAGIC);
    }
    else {
        sqoa_write_32(bytes, &p, SQOA_MAGIC);
    }
    sqoa_write_32(bytes, &p, desc->width);
    sqoa_write_32(bytes, &p, desc->height);
    bytes[p++] = desc->channels;
    bytes[p++] = desc->colorspace;
    
    if (qoi_compat) {
        max_run = QOI_MAXRUN;
    }
    else {
        max_run = SQOA_MAXRUN;
        bytes[p++] = SQOA_START_BYTE;
    }

    pixels = (const unsigned char *)data;

    SQOA_ZEROARR(index);

    run = 0;
    px_prev.rgba.r = 0;
    px_prev.rgba.g = 0;
    px_prev.rgba.b = 0;
    px_prev.rgba.a = 255;
    px.rgba.a = 255;

    px_len = desc->width * desc->height * desc->channels;
    px_end = px_len - desc->channels;
    channels = desc->channels;

    for (px_pos = 0; px_pos < px_len; px_pos += channels) {
        px.rgba.r = pixels[px_pos + 0];
        px.rgba.g = pixels[px_pos + 1];
        px.rgba.b = pixels[px_pos + 2];

        if (channels == 4) {
            px.rgba.a = pixels[px_pos + 3];
        }

        if (px.v == px_prev.v) {
            run++;
            if (run == max_run) {
                bytes[p++] = SQOA_OP_BIGRUN;
                run = 0;
            }
        }
        else {
            int index_pos = SQOA_COLOR_HASH(px) % 64;
            
            if (run > 0) {
                while (run > 61) {
                    bytes[p++] = SQOA_OP_RUN | 60;
                    run = run - 61;
                }
                bytes[p++] = SQOA_OP_RUN | (run - 1);
                run = 0;
            }

            if (index[index_pos].v == px.v) {
                bytes[p++] = SQOA_OP_INDEX | index_pos;
            }
            else {
                int found_ixa = 0;
                
                if (!qoi_compat && channels == 4) {
                    int index_a = SQOA_RGBA_HASH(px.rgba.r, px.rgba.g, px.rgba.b, 0xff) % 64;
                    sqoa_rgba_t px_alpha = px;
                    px_alpha.rgba.a = alpha[index_a];
                    alpha[index_a] = px.rgba.a;
                
                    index_a = SQOA_COLOR_HASH(px_alpha) % 64;
                    
                    found_ixa = (index[index_a].v == px_alpha.v);
                    if (found_ixa) {
                        bytes[p++] = SQOA_OP_INDEX | index_a;
                        bytes[p++] = SQOA_OP_ALPHA;
                        bytes[p++] = px.rgba.a;
                    }
                }
                
                index[index_pos] = px;
                
                if (!found_ixa) {
                    signed char vr = px.rgba.r - px_prev.rgba.r;
                    signed char vg = px.rgba.g - px_prev.rgba.g;
                    signed char vb = px.rgba.b - px_prev.rgba.b;
                    signed char va = px.rgba.a - px_prev.rgba.a;
                    signed char vg_r = vr - vg;
                    signed char vg_b = vb - vg;
                    int needs_alpha = (va != 0);

                    if (qoi_compat && needs_alpha) {
                        bytes[p++] = SQOA_OP_RGBA;
                        bytes[p++] = px.rgba.r;
                        bytes[p++] = px.rgba.g;
                        bytes[p++] = px.rgba.b;
                        bytes[p++] = px.rgba.a;
                    }
                    else {
                        if (
                            vr > -3 && vr < 2 &&
                            vg > -3 && vg < 2 &&
                            vb > -3 && vb < 2
                        ) {
                            bytes[p++] = SQOA_OP_DIFF | (vr + 2) << 4 | (vg + 2) << 2 | (vb + 2);
                        }
                        else if (
                            vg_r >  -9 && vg_r <  8 &&
                            vg   > -33 && vg   < 32 &&
                            vg_b >  -9 && vg_b <  8
                        ) {
                            bytes[p++] = SQOA_OP_LUMA    | (vg   + 32);
                            bytes[p++] = (vg_r + 8) << 4 | (vg_b +  8);
                        }
                        else {
                            bytes[p++] = SQOA_OP_RGB | needs_alpha;
                            bytes[p++] = px.rgba.r;
                            bytes[p++] = px.rgba.g;
                            bytes[p++] = px.rgba.b;
                            if (needs_alpha) {
                                bytes[p++] = px.rgba.a;
                                needs_alpha = 0;
                            }
                        }
        
                        if (needs_alpha) {
                            bytes[p++] = SQOA_OP_ALPHA;
                            bytes[p++] = px.rgba.a;
                        }
                    }
                }
            }
            px_prev = px;
        }
    }
    
    if (run > 0) {
        bytes[p++] = SQOA_OP_BIGRUN;
    }

    for (int i = 0; i < (int)sizeof(sqoa_padding); i++) {
        bytes[p++] = sqoa_padding[i];
    }

    *out_len = p;
    return bytes;
}

void *sqoa_decode(const void *data, int size, sqoa_desc *desc, int channels) {
    const unsigned char *bytes;
    unsigned int header_magic;
    unsigned char *pixels;
    sqoa_rgba_t index[64];
    sqoa_rgba_t px;
    int px_len, chunks_len, px_pos, qoi_compat;
    int p = 0, run = 0;

    if (
        data == NULL || desc == NULL ||
        (channels != 0 && channels != 3 && channels != 4) ||
        size < SQOA_HEADER_SIZE + (int)sizeof(sqoa_padding)
    ) {
        return NULL;
    }

    bytes = (const unsigned char *)data;

    header_magic = sqoa_read_32(bytes, &p);
    desc->width = sqoa_read_32(bytes, &p);
    desc->height = sqoa_read_32(bytes, &p);
    desc->channels = bytes[p++];
    desc->colorspace = bytes[p++];
    desc->qoi_compat = (bytes[p] != SQOA_START_BYTE);

    if (
        desc->width == 0 || desc->height == 0 ||
        desc->channels < 3 || desc->channels > 4 ||
        desc->colorspace > 1 ||
        !(header_magic == QOI_MAGIC || header_magic == SQOA_MAGIC) ||
        (header_magic == QOI_MAGIC && !desc->qoi_compat) ||
        desc->height >= SQOA_PIXELS_MAX / desc->width
    ) {
        return NULL;
    }
    
    if (channels == 0) {
        channels = desc->channels;
    }

    px_len = desc->width * desc->height * channels;
    pixels = (unsigned char *) SQOA_MALLOC(px_len);
    if (!pixels) {
        return NULL;
    }

    qoi_compat = desc->qoi_compat;
    if (!qoi_compat) {
        p++;
    }

    SQOA_ZEROARR(index);
    px.rgba.r = 0;
    px.rgba.g = 0;
    px.rgba.b = 0;
    px.rgba.a = 255;

    chunks_len = size - (int)sizeof(sqoa_padding);
    for (px_pos = 0; px_pos < px_len; px_pos += channels) {
        if (run > 0) {
            run--;
        }
        else if (p < chunks_len) {
            int b1 = bytes[p++];

            if (b1 == SQOA_OP_RGB) {
                px.rgba.r = bytes[p++];
                px.rgba.g = bytes[p++];
                px.rgba.b = bytes[p++];
            }
            else if (b1 == SQOA_OP_RGBA) {
                px.rgba.r = bytes[p++];
                px.rgba.g = bytes[p++];
                px.rgba.b = bytes[p++];
                px.rgba.a = bytes[p++];
            }
            else if ((b1 & SQOA_MASK_2) == SQOA_OP_INDEX) {
                px = index[b1];
            }
            else if (
                (qoi_compat || b1 != SQOA_OP_ALPHA) &&
                (b1 & SQOA_MASK_2) == SQOA_OP_DIFF
            ) {
                px.rgba.r += ((b1 >> 4) & 0x03) - 2;
                px.rgba.g += ((b1 >> 2) & 0x03) - 2;
                px.rgba.b += ( b1       & 0x03) - 2;
            }
            else if ((b1 & SQOA_MASK_2) == SQOA_OP_LUMA) {
                int b2 = bytes[p++];
                int vg = (b1 & 0x3f) - 32;
                px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
                px.rgba.g += vg;
                px.rgba.b += vg - 8 +  (b2       & 0x0f);
            }
            else if (!qoi_compat && b1 == SQOA_OP_BIGRUN) {
                run = SQOA_MAXRUN - 1;
            }
            else {
                run = (b1 & 0x3f);
            }

            if (!qoi_compat && bytes[p] == SQOA_OP_ALPHA) {
                p++;
                px.rgba.a = bytes[p++];
            }
            
            index[SQOA_COLOR_HASH(px) % 64] = px;
        }

        pixels[px_pos + 0] = px.rgba.r;
        pixels[px_pos + 1] = px.rgba.g;
        pixels[px_pos + 2] = px.rgba.b;
        
        if (channels == 4) {
            pixels[px_pos + 3] = px.rgba.a;
        }
    }

    return pixels;
}

#ifndef SQOA_NO_STDIO
#include <stdio.h>

int sqoa_write(const char *filename, const void *data, const sqoa_desc *desc) {
    FILE *f = fopen(filename, "wb");
    int size;
    void *encoded;

    if (!f) {
        return 0;
    }

    encoded = sqoa_encode(data, desc, &size);
    if (!encoded) {
        fclose(f);
        return 0;
    }

    fwrite(encoded, 1, size, f);
    fclose(f);

    SQOA_FREE(encoded);
    return size;
}

void *sqoa_read(const char *filename, sqoa_desc *desc, int channels) {
    FILE *f = fopen(filename, "rb");
    int size, bytes_read;
    void *pixels, *data;

    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    data = SQOA_MALLOC(size);
    if (!data) {
        fclose(f);
        return NULL;
    }

    bytes_read = fread(data, 1, size, f);
    fclose(f);

    pixels = sqoa_decode(data, bytes_read, desc, channels);
    SQOA_FREE(data);
    return pixels;
}

#endif /* SQOA_NO_STDIO */
#endif /* SQOA_IMPLEMENTATION */
