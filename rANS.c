#include <stdlib.h>
#include <stdint.h>

int beans_long_div(uint_fast32_t divisor, uint_least32_t number[], int nseg, uint_fast32_t *remainder) {
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

int beans_long_mul(uint_fast32_t factor, uint_least32_t number[], int nseg) {
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

int beans_long_shr(int shift, uint_least32_t number[], int nseg, uint_fast32_t *remainder) {
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

int beans_long_shl(int shift, uint_least32_t number[], int nseg) {
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

#define BEANS_FREQ_BITS 10
#define BEANS_FREQ_TOTAL (1 << BEANS_FREQ_BITS)
#define BEANS_NUM_SYMBOLS 256
#define BEANS_CODE_LEN(e) (e >> 6)

void beans_normalise_freqs(const uint_least32_t freq[BEANS_NUM_SYMBOLS], uint_least32_t norm[BEANS_NUM_SYMBOLS]);

const uint_least32_t beans_ft_freqs[BEANS_NUM_SYMBOLS] = {
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
        n += ft_len >> 6;
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
    return n | (nseg << 6);
}

void beans_inflate(unsigned char *bytes, int len, uint_least32_t code[], int nseg, const uint_least32_t freq[]) {
    unsigned char syms[BEANS_FREQ_TOTAL];
    uint_least32_t cumulf[BEANS_NUM_SYMBOLS + 1];
    int s = 0;
    for (int i = 0; i < BEANS_NUM_SYMBOLS; ++i) {
        cumulf[i] = s;
        s += freq[i];
    }
    cumulf[BEANS_NUM_SYMBOLS] = BEANS_FREQ_TOTAL;
    for (int b = 0; b < BEANS_NUM_SYMBOLS; ++b) {
        uint_fast32_t start = cumulf[b], end = cumulf[b + 1];
        for (int i = start; i < end; ++i) {
            syms[i] = b;
        }
    }
    for (int i = 0; i < len; ++i) {
        uint_fast32_t rest;
        nseg = beans_long_shr(BEANS_FREQ_BITS, code, nseg, &rest);
        unsigned char b = syms[rest];
        bytes[i] = b;
        if (i < len - 1) {
            uint_fast32_t f = cumulf[b + 1] - cumulf[b];
            int shift = (f == 2) ? 1 : (f == 4) ? 2 : (f == 8) ? 3 : (f == 16) ? 4 : (f == 32) ? 5 :
                (f == 64) ? 6 : (f == 128) ? 7 : (f == 256) ? 8 : (f == 512) ? 9 : (f == 1024) ? 10 : 
                0;
            if (shift) {
                nseg = beans_long_shl(shift, code, nseg);
            }
            else {
                nseg = beans_long_mul(f, code, nseg);
            }
            code[0] += rest - cumulf[b];
            if (nseg == 0 && rest != cumulf[b]) {
                nseg = 1;
            }
        }
    }
}

void restore_cumulf(uint_fast32_t cumulf[BEANS_NUM_SYMBOLS + 1], uint_fast32_t thresholds, unsigned char *freqs) {
    int s = 0;
    int thres1 = 1 + (thresholds >> 24);
    int thres2 = 1 + ((thresholds >> 16) & 0xff);
    int thres3 = 1 + ((thresholds >> 8) & 0xff);
    int thres4 = 1 + (thresholds & 0xff);

    for (int i = 0; i < thres1; ++i) {
        cumulf[i] = s;
        s += freqs[i];
    }
    if (s < 256) {
        s += 256;
    }
    for (int i = thres1; i < thres2; ++i) {
        cumulf[i] = s;
        s += freqs[i];
    }
    if (s < 512) {
        s += 256;
    }
    for (int i = thres2; i < thres3; ++i) {
        cumulf[i] = s;
        s += freqs[i];
    }
    if (s < 768) {
        s += 256;
    }
    for (int i = thres3; i < thres4; ++i) {
        cumulf[i] = s;
        s += freqs[i];
    }
    for (int i = thres4; i <= BEANS_NUM_SYMBOLS; ++i) {
        cumulf[i] = BEANS_FREQ_TOTAL;
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

#include <stdio.h>
#define SZ 4096

int main() {
    unsigned char buf[SZ];
    int r = fread(buf, 1, SZ, stdin);
    uint_fast32_t num[1500];

    int wc = beans_compress(buf, r, num, 1500, NULL);
    printf("Used %d x 32 bit words (freq table: %d).\n", BEANS_CODE_LEN(wc), wc & 63);

    unsigned char o[256] = {0};
    uint_least32_t ft[257] = {0};
    beans_inflate(o, 256, num + 1, wc & 63, beans_ft_freqs);
    restore_cumulf(ft, num[0], o);
    uint_least32_t counts[257] = {0};
    uint_least32_t freq[257];
    int bad = 0;
    int s = 0;
    for (int i = 0; i < r; ++i) {
        counts[buf[i]]++;
    }
    beans_normalise_freqs(counts, freq);
    for (int i = 0; i < 256; ++i) {
        counts[i] = s;
        s += freq[i];
    }
    counts[256] = s;
    for (int i = 0; i <= 256; ++i) {
        if (counts[i] != ft[i]) {
            ++bad;
            printf("Non-matching symbol at %d, was: %.2x but got: %.2x\n", i, counts[i], ft[i]);
        }
    }
    printf("\nbad = %d\n", bad);
/*    
    unsigned char result[SZ];
    expand(result, r, num, wc, ft);
    bad = 0;
    for (int i = 0; i < r; ++i) {
        if (result[i] != buf[i]) {
            ++bad;
        }
    }
    printf("\nbad = %d\n", bad);*/
}
