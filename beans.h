/*

BEANS - A single header solution for ANS-based compression

Denis Bredelet - https://github.com/jido


-- LICENSE: The MIT License(MIT)

Copyright(c) 2022 Denis Bredelet

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



-- Synopsis

// Define `BEANS_IMPLEMENTATION` in *one* C/C++ file before including this
// library to create the implementation.

#define BEANS_IMPLEMENTATION
#include "beans.h"

// Compress the input buffer to an array of 32-bit numbers. When the frequency
// table is NULL, frequencies are calculated from the input data itself.
int len_info = beans_compress(buffer, length, array, size, NULL);

// Uncompress the number array into the target buffer, updating the array.
beans_inflate(target, length, array, len_info, NULL);



-- Documentation

This library provides the following functions;
- beans_compress  -- compress a byte buffer in memory to an array of integers
- beans_inflate  -- expand compressed data from an array of integers to memory
(destructive operation: one shot only)
- beans_normalise_freqs  -- optimise symbol frequencies for reuse

See the function declaration below for the signature and more information.


*/

/* -----------------------------------------------------------------------------
Header - Public functions */

#ifndef BEANS_H
#define BEANS_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

#define BEANS_NUM_SYMBOLS 256
#define BEANS_CODE_LEN(e) (e & 0x1ffffff)
#define BEANS_FT_LEN(e) (e >> 25)

/* Compress a byte buffer in memory to an array of integers

Inputs: the byte buffer, its length, an allocated array of integers, its size,
 symbol counts [optional argument: can be NULL, in which case the counts are
 calculated from the byte buffer data and stored in compressed form together 
 with the rest of the data]

Output: compressed length information. Use BEANS_CODE_LEN and BEANS_FT_LEN
 to split the value between overall length and counts (frequency table) length
 The returned value is 0 in case of error (e.g. array size too small).
*/
int beans_compress(const unsigned char *bytes, int len, uint_least32_t result[], int size, const uint_least32_t *counts);

/* Expand compressed data from an array of integers to memory

Inputs: the memory buffer to decompress into, number of symbols to uncompress,
 integer array with the compressed data, compressed length information
 returned by compress function, frequency table (symbol counts used to
 compress) [optional argument: can be NULL, in which case the table is
 read from the compressed data array]
*/
void beans_inflate(unsigned char *bytes, int len, uint_least32_t code[], int nseg, const uint_least32_t counts[]);

/* Optimise symbol frequencies for reuse

Inputs: a frequency table, an array of integers where to save the optimised
 table. The main effect of this function is to scale the values so that they
 add up to BEANS_FREQ_TOTAL which is required for compression. Using this
 function before compress saves having to normalise every time a frequency
 table is reused.
*/
void beans_normalise_freqs(const uint_least32_t freq[BEANS_NUM_SYMBOLS], uint_least32_t norm[BEANS_NUM_SYMBOLS]);

#ifdef __cplusplus
}
#endif

#endif /*ifndef BEANS_H*/




#ifdef BEANS_IMPLEMENTATION
#include <stdlib.h>

#define BEANS_FREQ_BITS 10
#define BEANS_FREQ_TOTAL (1u << BEANS_FREQ_BITS)


static int beans_long_div(uint_fast32_t divisor, uint_least32_t number[], int nseg, uint_fast32_t *remainder) {
    uint_fast64_t rest = 0;
    if (divisor != 1) {
        if (divisor > number[nseg - 1]) {
            --nseg;
            rest = number[nseg];
        }
        for (int i = nseg - 1; i >= 0; --i) {
            uint_fast64_t segment = (rest << 32) | number[i];
            uint_fast32_t div1 = segment / divisor;
            rest = segment % divisor;
            number[i] = div1;
        }
    }
    if (remainder != NULL) {
        *remainder = rest;
    }
    return nseg;
}

static int beans_long_mul(uint_fast32_t factor, uint_least32_t number[], int nseg) {
    if (factor != 1) {
        uint_fast64_t carry = 0;
        for (int i = 0; i < nseg; ++i) {
            uint_fast64_t segment = carry + (uint_fast64_t) number[i] * factor;
            carry = segment >> 32;
            number[i] = segment & UINT32_MAX;
        }
        if (carry > 0) {
            number[nseg] = carry;
            ++nseg;
        }
    }
    return nseg;
}

static int beans_long_shr(int shift, uint_least32_t number[], int nseg, uint_fast32_t *remainder) {
    uint_fast32_t rest = 0;
    if (shift != 0) {
        uint_fast32_t mask = (1 << shift) - 1;
        if (mask >= number[nseg - 1]) {
            --nseg;
            rest = number[nseg];
        }
        for (int i = nseg - 1; i >= 0; --i) {
            uint_fast32_t segment = (rest << (32 - shift)) | (number[i] >> shift);
            rest = number[i] & mask;
            number[i] = segment;
        }
    }
    if (remainder != NULL) {
        *remainder = rest;
    }
    return nseg;
}

static int beans_long_shl(int shift, uint_least32_t number[], int nseg) {
    if (shift != 0) {
        uint_fast32_t carry = 0;
        for (int i = 0; i < nseg; ++i) {
            uint_fast32_t segment = carry | (number[i] << shift);
            carry = number[i] >> (32 - shift);
            number[i] = segment & UINT32_MAX;
        }
        if (carry > 0) {
            number[nseg] = carry;
            ++nseg;
        }
    }
    return nseg;
}

static const uint_least32_t beans_ft_freqs[BEANS_NUM_SYMBOLS] = {
    512, 128, 64, 32, 16, 8, 4, 4, 4, 2, 2, 2, 2, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

int beans_compress(const unsigned char *bytes, int len, uint_least32_t result[], int size, const uint_least32_t *counts) {
    if (bytes == NULL || len < 1 || result == NULL || size < 1) {
        return 0;
    }
    
    int n = 0;
    uint_least32_t freq[BEANS_NUM_SYMBOLS];
    uint_least32_t cumulf[BEANS_NUM_SYMBOLS + 1] = {0};

    if (counts != NULL) {
        beans_normalise_freqs(counts, freq);

        uint_fast32_t s = 0;
        for (int i = 0; i < BEANS_NUM_SYMBOLS; ++i) {
            cumulf[i] = s;
            s += freq[i];
        }
    }
    else {
        for (int i = 0; i < len; ++i) {
            cumulf[bytes[i]]++;
        }
        beans_normalise_freqs(cumulf, freq);

        unsigned char squashed[BEANS_NUM_SYMBOLS];
        int is_set = 0; 
        uint_fast32_t thresholds = 0;
        uint_fast32_t s = 0;
        for (int i = 0; i < BEANS_NUM_SYMBOLS; ++i) {
            squashed[i] = freq[i];
            cumulf[i] = s;
            s += freq[i];

            if (is_set < 1 && s >= 256) {
                thresholds |= i << 24;
                is_set = 1;
            }
            if (is_set < 2 && s >= 512) {
                thresholds |= i << 16;
                is_set = 2;
            }
            if (is_set < 3 && s >= 768) {
                thresholds |= i << 8;
                is_set = 3;
            }
            if (is_set < 4 && s >= 1024) {
                thresholds |= i;
                is_set = 4;
            }
        }
        
        result[n++] = thresholds;
        int ft_len = beans_compress(squashed, BEANS_NUM_SYMBOLS, result + n, size - n, beans_ft_freqs);
        if (ft_len == 0) {
            return 0;
        }
        ft_len = beans_long_shl(6, result + n, ft_len);
        if (ft_len > 64) {
            return 0;
        }
        result[n] |= (ft_len - 1);
        n += ft_len;
    }
    cumulf[BEANS_NUM_SYMBOLS] = BEANS_FREQ_TOTAL;
    
    int p = len;
    result[n] = cumulf[bytes[--p]];
    int nseg = 1;
    while (p > 0) {
        unsigned char b = bytes[--p];
        uint_fast32_t f = cumulf[b + 1] - cumulf[b];
        uint_fast32_t rest;
        int shift = (f == 2) ? 1 : (f == 4) ? 2 : (f == 8) ? 3 : (f == 16) ? 4 : (f == 32) ? 5 :
            (f == 64) ? 6 : (f == 128) ? 7 : (f == 256) ? 8 : (f == 512) ? 9 : (f == 1024) ? 10 : 
            0;
        if (shift) {
            nseg = beans_long_shr(shift, result + n, nseg, &rest);
        }
        else {
            nseg = beans_long_div(f, result + n, nseg, &rest);
        }
        if (n + nseg + 1 >= size) {
            return 0;
        }
        nseg = beans_long_shl(BEANS_FREQ_BITS, result + n, nseg);
        
        result[n] |= cumulf[b] + rest;
        if (nseg == 0) {
            nseg = 1;
        }
    }
    return (nseg + n);
}

void beans_inflate(unsigned char *bytes, int len, uint_least32_t code[], int nseg, const uint_least32_t counts[]) {
    unsigned char syms[BEANS_FREQ_TOTAL];
    uint_least32_t cumulf[BEANS_NUM_SYMBOLS + 1];
    int n = 0;
    
    if (counts != NULL) {
        uint_least32_t freq[BEANS_NUM_SYMBOLS];
        beans_normalise_freqs(counts, freq);

        uint_fast32_t s = 0;
        for (int i = 0; i < BEANS_NUM_SYMBOLS; ++i) {
            cumulf[i] = s;
            s += freq[i];
        }
        cumulf[BEANS_NUM_SYMBOLS] = BEANS_FREQ_TOTAL;
    }
    else {
        unsigned char freq[BEANS_NUM_SYMBOLS];
        int s = 0;
        uint_least32_t thresholds = code[0];
        int thres1 = 1 + (thresholds >> 24);
        int thres2 = 1 + ((thresholds >> 16) & 0xff);
        int thres3 = 1 + ((thresholds >> 8) & 0xff);
        int thres4 = 1 + (thresholds & 0xff);
        int m = 1;

        m += code[1] & 63;
        n = 1 + m;
        m = beans_long_shr(6, code + 1, m, NULL);
        beans_inflate(freq, BEANS_NUM_SYMBOLS, code + 1, m, beans_ft_freqs);

        for (int i = 0; i < thres1; ++i) {
            cumulf[i] = s;
            s += freq[i];
        }
        if (s < 256) {
            s += 256;
        }
        for (int i = thres1; i < thres2; ++i) {
            cumulf[i] = s;
            s += freq[i];
        }
        if (s < 512) {
            s += 256;
        }
        for (int i = thres2; i < thres3; ++i) {
            cumulf[i] = s;
            s += freq[i];
        }
        if (s < 768) {
            s += 256;
        }
        for (int i = thres3; i < thres4; ++i) {
            cumulf[i] = s;
            s += freq[i];
        }
        for (int i = thres4; i <= BEANS_NUM_SYMBOLS; ++i) {
            cumulf[i] = BEANS_FREQ_TOTAL;
        }
        nseg -= n;
    }
    for (int b = 0; b < BEANS_NUM_SYMBOLS; ++b) {
        uint_fast32_t start = cumulf[b], end = cumulf[b + 1];
        for (int i = start; i < end; ++i) {
            syms[i] = b;
        }
    }
    for (int i = 0; i < len; ++i) {
        uint_fast32_t rest;
        nseg = beans_long_shr(BEANS_FREQ_BITS, code + n, nseg, &rest);
        unsigned char b = syms[rest];
        bytes[i] = b;
        if (i < len - 1) {
            uint_fast32_t f = cumulf[b + 1] - cumulf[b];
            int shift = (f == 2) ? 1 : (f == 4) ? 2 : (f == 8) ? 3 : (f == 16) ? 4 : (f == 32) ? 5 :
                (f == 64) ? 6 : (f == 128) ? 7 : (f == 256) ? 8 : (f == 512) ? 9 : (f == 1024) ? 10 : 
                0;
            if (shift) {
                nseg = beans_long_shl(shift, code + n, nseg);
            }
            else {
                nseg = beans_long_mul(f, code + n, nseg);
            }
            code[n] += rest - cumulf[b];
            if (nseg == 0 && rest != cumulf[b]) {
                nseg = 1;
            }
        }
    }
}

void beans_normalise_freqs(const uint_least32_t freq[BEANS_NUM_SYMBOLS], uint_least32_t norm[BEANS_NUM_SYMBOLS]) {
    int nz = 0;
    int top = 0;    
    int s = 0;
    for (int i = 0, max = 0; i < BEANS_NUM_SYMBOLS; ++i) {
        norm[i] = freq[i];
        if (freq[i]) {
            if (freq[i] > max) {
                max = freq[i];
                top = i;
            }
            s += freq[i];
            nz++;
        }
    }
    
    if (s != BEANS_FREQ_TOTAL) {
        const int shift = 31 - BEANS_FREQ_BITS;
        const uint_fast32_t half = (1 << (shift - 1));
        uint_fast32_t t = BEANS_FREQ_TOTAL << shift;
        if (s > BEANS_FREQ_TOTAL) {
            t -= nz << (shift - 2);
        }
        uint_fast32_t r = t / s;
        
        s = 0;
        for (int i = 0; i < BEANS_NUM_SYMBOLS; ++i) {
            if (norm[i]) {
                uint_fast32_t a = (norm[i] * r + half) >> shift;
                norm[i] = (a > 0) ? a : 1;
                s += norm[i];
            }
        }
        if (s != BEANS_FREQ_TOTAL) {
            norm[top] += (BEANS_FREQ_TOTAL - s);
        }
    }
}

#endif /*ifdef SQOA_IMPLEMENTATION*/
