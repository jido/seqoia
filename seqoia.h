/*

SQOA - The fast image compression which losslessly squashes image size

       Based on QOI https://phoboslab.org/log/2021/12/qoi-specification

Denis Bredelet - https://github.com/jido


-- LICENSE: The MIT License(MIT), see below.

-- About

As compared to QOI the SQOA format loses some range for the DIFF operation,
with gradients between -1 and +1 instead of between -2 and +1. In exchange
it gains operations for long runs (up to 65855 pixels) and Alpha Update, which
uses a single byte to update the alpha channel of the next pixel with a range
of values between -6 and +6.


Original header:


¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨
QOI - The "Quite OK Image" format for fast, lossless image compression

Dominic Szablewski - https://phoboslab.org


-- LICENSE: The MIT License(MIT)

Copyright(c) 2021 Dominic Szablewski

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files(the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions :
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


-- About

QOI encodes and decodes images in a lossless format. Compared to stb_image and
stb_image_write QOI offers 20x-50x faster encoding, 3x-4x faster decoding and
20% better compression.

¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨



-- Synopsis

// Define `SQOA_IMPLEMENTATION` in *one* C/C++ file before including this
// library to create the implementation.

#define SQOA_IMPLEMENTATION
#include "seqoia.h"

// Encode and store an RGBA buffer to the file system. The sqoa_desc describes
// the input pixel data.
sqoa_write("image_new.sqoa", rgba_pixels, &(sqoa_desc){
    .width = 1920,
    .height = 1080,
    .channels = 4,
    .colorspace = SQOA_SRGB
});

// Load and decode a SQOA image from the file system into a 32bbp RGBA buffer.
// The sqoa_desc struct will be filled with the width, height, number of channels
// and colorspace read from the file header.
sqoa_desc desc;
void *rgba_pixels = sqoa_read("image.sqoa", &desc, 4);



-- Documentation

This library provides the following functions;
- sqoa_read    -- read and decode a SQOA file
- sqoa_decode  -- decode the raw bytes of a SQOA image from memory
- sqoa_write   -- encode and write a SQOA file
- sqoa_encode  -- encode an rgba buffer into a SQOA image in memory

See the function declaration below for the signature and more information.

If you don't want/need the sqoa_read and sqoa_write functions, you can define
SQOA_NO_STDIO before including this library.

This library uses malloc() and free(). To supply your own malloc implementation
you can define SQOA_MALLOC and SQOA_FREE before including this library.

This library uses memset() to zero-initialize the index. To supply your own
implementation you can define SQOA_ZEROARR before including this library.


-- Data Format

A SQOA file has a 14 byte header, followed by any number of data "chunks" and an
8-byte end marker.

struct sqoa_header_t {
    char     magic[4];   // magic bytes "Sqoa"
    uint32_t width;      // image width in pixels (BE)
    uint32_t height;     // image height in pixels (BE)
    uint8_t  channels;   // 3 = RGB, 4 = RGBA
    uint8_t  colorspace; // 0 = sRGB with linear alpha, 1 = all channels linear
};

Images are encoded row by row, left to right, top to bottom. The decoder and
encoder start with {r: 0, g: 0, b: 0, a: 255} as the previous pixel value. An
image is complete when all pixels specified by width * height have been covered.

Pixels are encoded as
 - a run of the previous pixel
 - an index into an array of previously seen pixels
 - a difference to the previous pixel value in r,g,b
 - full r,g,b or r,g,b,a values

The color channels are assumed to not be premultiplied with the alpha channel
("un-premultiplied alpha"). If the difference between the alpha value of the
pixel and the reference pixel (indexed or previous) is at most 6 in amplitude,
this can also be encoded.

A running array[64] (zero-initialized) of previously seen pixel values is
maintained by the encoder and decoder. Each pixel that is seen by the encoder
and decoder is put into this array at the position formed by a hash function of
the color value. In the encoder, if the pixel value at the index matches the
current pixel, this index position is written to the stream as SQOA_OP_INDEX.
The hash function for the index is:

    index_position = (r * 3 + g * 5 + b * 7) % 64

Each chunk starts with a 2- or 8-bit tag, followed by a number of data bits. The
bit length of chunks is divisible by 8 - i.e. all chunks are byte aligned. All
values encoded in these data bits have the most significant bit on the left.

The 8-bit tags have precedence over the 2-bit tags. A decoder must check for the
presence of an 8-bit tag first. The Alpha Update operation uses values between
SQOA_ALPHA_LO and SQOA_ALPHA_HI exclusive, with SQOA_ALPHA_MID equally excluded.
A decoder must advance to the next chunk after reading an Alpha Update chunk.

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

A valid encoder must not issue 2 or more consecutive SQOA_OP_INDEX chunks to the
same index. SQOA_OP_RUN should be used instead.


.- SQOA_OP_DIFF ----------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+--------+--------|
|  0  1 |r+ g+ b+|r- b- g-|
`-------------------------`
2-bit tag b01
3-bit RGB channel increase from the previous pixel 0..1 x 3
3-bit RGB channel decrease from the previous pixel 0..1 x 3

The difference to the current channel values are using a wraparound operation,
so "0 - 1" will result in 255, while "255 + 1" will result in 0.

Values are stored as one bit per channel. E.g. green+1 blue+1 red-1 is stored
as 3, 4 (b011100). Green-1 is stored as 0, 2 (b000010).

Note that a channel cannot be up and down at the same time, which means the
valid values starting with b0111 are limited to 0x70, 0x71 and 0x78.


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


.- SQOA_OP_RUN -----------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----------------|
|  1  1 |       run       |
`-------------------------`
2-bit tag b11
6-bit run-length repeating the previous pixel: 1..63

The run-length is stored with a bias of -1. Note that the run-length 64
(b11111111) is illegal as it is occupied by the SQOA_OP_RGB tag.


.- SQOA_OP_BIGRUN ------------------.
|         Byte[0]         | Byte[1] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  |
|-------------------------+---------|
|  0  1  0  0  1  1  1  1 |   run   |
`-----------------------------------`
8-bit tag b01001111
8-bit run-length repeating the previous pixel: 64..319

The run-length is stored with a bias of -64.


.- SQOA_OP_MAXRUN ----------------------------.
|         Byte[0]         | Byte[1] | Byte[2] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  | 7 .. 0  |
|-------------------------+---------+---------|
|  0  1  1  0  1  1  1  1 |        run        |
`---------------------------------------------`
8-bit tag b01101111
16-bit run-length repeating the previous pixel: 320..65855

The run-length is stored in with the most significant byte first and with
a bias of -320.


.- SQOA_OP_RGB -----------------------------------------.
|         Byte[0]         | Byte[1] | Byte[2] | Byte[3] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  | 7 .. 0  | 7 .. 0  |
|-------------------------+---------+---------+---------|
|  1  1  1  1  1  1  1  1 |   red   |  green  |  blue   |
`-------------------------------------------------------`
8-bit tag b11111111
8-bit   red channel value
8-bit green channel value
8-bit  blue channel value


.- SQOA_OP_RGBA --------------------------------------------------.
|         Byte[0]         | Byte[1] | Byte[2] | Byte[3] | Byte[4] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  | 7 .. 0  | 7 .. 0  | 7 .. 0  |
|-------------------------+---------+---------+---------+---------|
|  0  1  1  1  1  1  1  1 |   red   |  green  |  blue   |  alpha  |
`-----------------------------------------------------------------`
8-bit tag b01111111
8-bit   red channel value
8-bit green channel value
8-bit  blue channel value
8-bit alpha channel value


.- Alpha Update ----------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------------+-----------|
|  0  1  1  1 |alpha diff |
`-------------------------`
4-bit tag b0111
4-bit alpha channel difference from previous pixel -6..6

This operation does not produce a pixel, instead it applies to next pixel.
Unless an Alpha Update operation is used the alpha channel of the next pixel 
remains unchanged from previous pixel.

The difference to the current channel values are using a wraparound operation,
so "2 - 5" will result in 253, while "255 + 2" will result in 1.

The value is stored as an unsigned integer with a bias of 8. Note that value
120 (which would correspond to an alpha update of 0) is invalid as it is used
for the SQOA_OP_DIFF tag.

Alpha Update cannot be used together with the various RUN operations or with
the RGBA operation.

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
how chunks are en-/decoded. */

#define SQOA_SRGB   0
#define SQOA_LINEAR 1

typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned char channels;
    unsigned char colorspace;
} sqoa_desc;

#define SQOA_BLOCK_SIZE 4096
#define SQOA_UNCOMPRESSED 0
#define SQOA_COMP_BEANS 1

#define SQOA_BLOCK_LEN(h, l) (1 + ((((int)(h) << 8) | (l)) & 0xfff))
#define SQOA_BLOCK_COMPT(h, l) ((h) >> 4)

#ifndef SQOA_NO_STDIO

/* Encode raw RGB or RGBA pixels into a SQOA image and write it to the file
system. The sqoa_desc struct must be filled with the image width, height,
number of channels (3 = RGB, 4 = RGBA) and the colorspace.

The function returns 0 on failure (invalid parameters, or fopen or malloc
failed) or the number of bytes written on success. */

int sqoa_write(const char *filename, const void *data, const sqoa_desc *desc);


/* Read and decode a SQOA image from the file system. If channels is 0, the
number of channels from the file header is used. If channels is 3 or 4 the
output format will be forced into this number of channels.

The function either returns NULL on failure (invalid data, or malloc or fopen
failed) or a pointer to the decoded pixels. On success, the sqoa_desc struct
will be filled with the description from the file header.

The returned pixel data should be free()d after use. */

void *sqoa_read(const char *filename, sqoa_desc *desc, int channels);

#endif /* SQOA_NO_STDIO */


/* Encode raw RGB or RGBA pixels into a SQOA image in memory.

The function either returns NULL on failure (invalid parameters or malloc
failed) or a pointer to the encoded data on success. On success the out_len
is set to the size in bytes of the encoded data.

The returned sqoa data should be free()d after use. */

void *sqoa_encode(const void *data, const sqoa_desc *desc, int *out_len);


/* Decode a SQOA image from memory.

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

#ifdef SQOA_COMPRESSION
#include "beans.h"
#endif

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
#define SQOA_OP_BIGRUN 0x4f /* 01001111 */
#define SQOA_OP_MAXRUN 0x6f /* 01101111 */
#define SQOA_OP_RGBA   0x7f /* 01111111 */
#define SQOA_OP_RGB    0xff /* 11111111 */

#define SQOA_MASK_2    0xc0 /* 11000000 */

#define SQOA_ALPHA_LO  113  /* 01110001 */
#define SQOA_ALPHA_MID 120  /* 01111000 */
#define SQOA_ALPHA_HI  127  /* 01111111 */

#define SQOA_RUN_LIMIT 65855 /* 64 + 256 + 65535 */

#define SQOA_COLOR_HASH(C) (C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7)
#define SQOA_MAGIC \
    (((unsigned int)'S') << 24 | ((unsigned int)'q') << 16 | \
     ((unsigned int)'o') <<  8 | ((unsigned int)'a'))
#define SQOA_HEADER_SIZE 14

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
    int i, max_size, p, run, len;
    int px_len, px_end, px_pos, channels;
    unsigned char *bytes;
    unsigned char block[SQOA_BLOCK_SIZE];
    const unsigned char *pixels;
    sqoa_rgba_t index[64];
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

    p = 0;
    bytes = (unsigned char *) SQOA_MALLOC(max_size);
    if (!bytes) {
        return NULL;
    }

    sqoa_write_32(bytes, &p, SQOA_MAGIC);
    sqoa_write_32(bytes, &p, desc->width);
    sqoa_write_32(bytes, &p, desc->height);
    bytes[p++] = desc->channels;
    bytes[p++] = desc->colorspace;


    pixels = (const unsigned char *)data;

    SQOA_ZEROARR(index);

    len = 0;
    run = 0;
    px_prev.rgba.r = 0;
    px_prev.rgba.g = 0;
    px_prev.rgba.b = 0;
    px_prev.rgba.a = 255;
    px = px_prev;

    px_len = desc->width * desc->height * desc->channels;
    px_end = px_len - desc->channels;
    channels = desc->channels;

    for (px_pos = 0; px_pos < px_len; px_pos += channels) {
        unsigned char op[8];
        int op_len = 0;
        signed char va;
        int index_pos;
        sqoa_rgba_t px_cached;

        px.rgba.r = pixels[px_pos + 0];
        px.rgba.g = pixels[px_pos + 1];
        px.rgba.b = pixels[px_pos + 2];

        if (channels == 4) {
            px.rgba.a = pixels[px_pos + 3];
        }
        
        if (px_prev.v == px.v) {
            run++;
        }
        if (
            run == SQOA_RUN_LIMIT || 
            (run > 0 && (px_pos == px_end || px_prev.v != px.v))
        ) {
            if (run < 64) {
                op[op_len++] = SQOA_OP_RUN | (run - 1);
            }
            else if (run < 320) {
                op[op_len++] = SQOA_OP_BIGRUN;
                op[op_len++] = run - 64;
            }
            else {
                op[op_len++] = SQOA_OP_MAXRUN;
                op[op_len++] = (run - 320) >> 8;
                op[op_len++] = (unsigned char) (run - 320);
            }
            run = 0;
        }
        if (px_prev.v != px.v) {
            index_pos = SQOA_COLOR_HASH(px) % 64;
            px_cached = index[index_pos];
            va = px.rgba.a - px_cached.rgba.a;

            if (
                px_cached.rgba.r == px.rgba.r && 
                px_cached.rgba.g == px.rgba.g && 
                px_cached.rgba.b == px.rgba.b && 
                va > -7 && va < 7
            ) {
                if (va != 0) {
                    op[op_len++] = SQOA_ALPHA_MID + va;
                    index[index_pos] = px;
                }
                op[op_len++] = SQOA_OP_INDEX | index_pos;
            }
            else {
                index[index_pos] = px;

                va = px.rgba.a - px_prev.rgba.a;
                
                if (va < -6 || va > 6) {
                    op[op_len++] = SQOA_OP_RGBA;
                    op[op_len++] = px.rgba.r;
                    op[op_len++] = px.rgba.g;
                    op[op_len++] = px.rgba.b;
                    op[op_len++] = px.rgba.a;
                }
                else {
                    signed char vr = px.rgba.r - px_prev.rgba.r;
                    signed char vg = px.rgba.g - px_prev.rgba.g;
                    signed char vb = px.rgba.b - px_prev.rgba.b;

                    signed char vg_r = vr - vg;
                    signed char vg_b = vb - vg;
                    
                    if (va != 0) {
                        op[op_len++] = SQOA_ALPHA_MID + va;
                    }

                    if (
                        vr > -2 && vr < 2 &&
                        vg > -2 && vg < 2 &&
                        vb > -2 && vb < 2
                    ) {
                        op[op_len++] = SQOA_OP_DIFF |
                            (vr > 0) << 5 | (vg > 0) << 4 | (vb > 0) << 3 |
                            (vr < 0) << 2 | (vg < 0) << 1 | (vb < 0);
                    }
                    else if (
                        vg_r >  -9 && vg_r <  8 &&
                        vg   > -33 && vg   < 32 &&
                        vg_b >  -9 && vg_b <  8
                    ) {
                        op[op_len++] = SQOA_OP_LUMA    | (vg   + 32);
                        op[op_len++] = (vg_r + 8) << 4 | (vg_b +  8);
                    }
                    else {
                        op[op_len++] = SQOA_OP_RGB;
                        op[op_len++] = px.rgba.r;
                        op[op_len++] = px.rgba.g;
                        op[op_len++] = px.rgba.b;
                    }
                }
            }
            px_prev = px;
            if (len + op_len > SQOA_BLOCK_SIZE) {
#ifdef SQOA_COMPRESSION
                uint_least32_t compressed[SQOA_BLOCK_SIZE >> 2];
                int code_len = beans_compress(block, len, compressed, (len - 1) >> 2, NULL);
                if (code_len != 0) {
                    sqoa_write_32(bytes, &p, (SQOA_COMP_BEANS << 28) | (((code_len << 2) + 1) << 16) | len);
                    for (int i = 0; i < code_len; ++i) {
                        sqoa_write_32(bytes, &p, compressed[i]);
                    }
                }
                else {
                    bytes[p++] = (SQOA_UNCOMPRESSED << 4) | ((len - 1) >> 8);
                    bytes[p++] = (unsigned char)(len - 1);
                    for (int i = 0; i < len; ++i) {
                        bytes[p++] = block[i];
                    }
                }
#else
                bytes[p++] = (SQOA_UNCOMPRESSED << 4) | ((len - 1) >> 8);
                bytes[p++] = (unsigned char)(len - 1);
                for (int i = 0; i < len; ++i) {
                    bytes[p++] = block[i];
                }
#endif
                len = 0; 
            }
            for (int i = 0; i < op_len; ++i) {
                block[len++] = op[i];
            }
        }
    }
    bytes[p++] = (SQOA_UNCOMPRESSED << 4) | ((len - 1) >> 8);
    bytes[p++] = (unsigned char)(len - 1);
    for (int i = 0; i < len; ++i) {
        bytes[p++] = block[i];
    }

    *out_len = p;
    return bytes;
}

void *sqoa_decode(const void *data, int size, sqoa_desc *desc, int channels) {
#ifdef SQOA_COMPRESSION
    unsigned char block[SQOA_BLOCK_SIZE];
#else
    const unsigned char *block;
#endif
    const unsigned char *bytes;
    unsigned int header_magic;
    unsigned char *pixels;
    sqoa_rgba_t index[64];
    sqoa_rgba_t px;
    unsigned char h, l;
    int px_len, block_len, px_pos, block_pos, b1, debug_last;
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

    if (
        desc->width == 0 || desc->height == 0 ||
        desc->channels < 3 || desc->channels > 4 ||
        desc->colorspace > 1 ||
        header_magic != SQOA_MAGIC ||
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

    SQOA_ZEROARR(index);
    px.rgba.r = 0;
    px.rgba.g = 0;
    px.rgba.b = 0;
    px.rgba.a = 255;

    int block_size;
    block_len = 0;
    block_pos = 0;
    for (px_pos = 0; px_pos < px_len; px_pos += channels) {
        if (run > 0) {
            run--;
        }
        else if (p < size || block_pos < block_len) {
            if (block_pos == block_len) {
#ifdef SQOA_COMPRESSION
                h = bytes[p];
                if (SQOA_BLOCK_COMPT(h, 0) == SQOA_COMP_BEANS) {
                    uint_least32_t data[SQOA_BLOCK_SIZE >> 2];
                    unsigned int info = sqoa_read_32(bytes, &p);
                    l = (info >> 16) & 0xff;
                    block_size = SQOA_BLOCK_LEN(h, l) >> 2;
                    block_len = info & 0xffff;
                    for (int i = 0; i < block_size; ++i) {
                        data[i] = sqoa_read_32(bytes, &p);
                    }
                    debug_last = data[block_size - 1];
                    beans_inflate(block, block_len, data, block_size, NULL);
                    //printf("Inflate: %d codes -> %d bytes Sample: %.2x %.2x %.2x %.2x ... %.2x %.2x %.2x %.2x\n", block_size, block_len,
                    //     block[0], block[1], block[2], block[3], block[block_len - 4], block[block_len - 3], block[block_len - 2], block[block_len - 1]);
                    block_pos = 0;
                }
                else {
                    h = bytes[p++];
                    l = bytes[p++];
                    block_len = SQOA_BLOCK_LEN(h, l);
                    if (SQOA_BLOCK_COMPT(h, l) != SQOA_UNCOMPRESSED) {
                        printf("Unknown block type at %x\n", p - 2);
                        free(pixels);
                        return NULL;
                    }
                    for (int i = 0; i < block_len; ++i) {
                        block[i] = bytes[p++];
                    }
                    block_pos = 0;
                }
#else
                h = bytes[p++];
                l = bytes[p++];
                block_len = SQOA_BLOCK_LEN(h, l);
                if (SQOA_BLOCK_COMPT(h, l) != SQOA_UNCOMPRESSED) {
                    printf("Unknown block type at %x\n", p - 2);
                    free(pixels);
                    return NULL;
                }
                block = bytes + p;
                p += block_len;
                block_pos = 0;
#endif
            }
            else if (block_pos > block_len) {
                printf("Data error: position past block end (block at %x last code=%.4x last op=%.2x)\n", p - block_size * 4, debug_last, b1);
                free(pixels);
                return NULL;
            }
            b1 = block[block_pos++];
            signed char va = 0;
            
            if (b1 > SQOA_ALPHA_LO && b1 < SQOA_ALPHA_HI) {
                va = b1 - SQOA_ALPHA_MID;
                if (va != 0) {
                    b1 = block[block_pos++];
                }
            }

            if (b1 == SQOA_OP_RGB) {
                px.rgba.r = block[block_pos++];
                px.rgba.g = block[block_pos++];
                px.rgba.b = block[block_pos++];
            }
            else if (b1 == SQOA_OP_RGBA) {
                px.rgba.r = block[block_pos++];
                px.rgba.g = block[block_pos++];
                px.rgba.b = block[block_pos++];
                px.rgba.a = block[block_pos++];
            }
            else if ((b1 & SQOA_MASK_2) == SQOA_OP_INDEX) {
                px = index[b1];
            }
            else if ((b1 & SQOA_MASK_2) == SQOA_OP_RUN) {
                run = (b1 & 0x3f);
            }
            else if (b1 == SQOA_OP_BIGRUN) {
                run = 63 + block[block_pos++];
            }
            else if (b1 == SQOA_OP_MAXRUN) {
                int hibits = block[block_pos++];
                run = 319 + (hibits << 8) + block[block_pos++];
            }
            else if ((b1 & SQOA_MASK_2) == SQOA_OP_DIFF) {
                px.rgba.r += ((b1 >> 5) & 1) - ((b1 >> 2) & 1);
                px.rgba.g += ((b1 >> 4) & 1) - ((b1 >> 1) & 1);
                px.rgba.b += ((b1 >> 3) & 1) - (b1 & 1);
            }
            else if ((b1 & SQOA_MASK_2) == SQOA_OP_LUMA) {
                int b2 = block[block_pos++];
                int vg = (b1 & 0x3f) - 32;
                px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
                px.rgba.g += vg;
                px.rgba.b += vg - 8 +  (b2       & 0x0f);
            }

            if (run == 0) {
                px.rgba.a += va;
                index[SQOA_COLOR_HASH(px) % 64] = px;
            }
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
