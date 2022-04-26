#include <stdio.h>
#include <stdint.h>

#define MASK_32BIT 0xfffffffful

int long_div(uint_fast32_t divisor, uint_fast32_t number[], int nseg, uint_fast32_t *remainder) {
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

int long_mul(uint_fast32_t factor, uint_fast32_t number[], int nseg) {
    if (factor != 1) {
        uint_fast64_t carry = 0;
        for (int i = 0; i < nseg; ++i) {
            uint_fast64_t segment = carry + (uint_fast64_t) number[i] * factor;
            carry = segment >> 32;
            number[i] = segment & MASK_32BIT;
        }
        if (carry > 0) {
            number[nseg] = carry;
            ++nseg;
        }
    }
    return nseg;
}

int long_shr(int shift, uint_fast32_t number[], int nseg, uint_fast32_t *remainder) {
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

int long_shl(int shift, uint_fast32_t number[], int nseg) {
    if (shift != 0) {
        uint_fast32_t carry = 0;
        for (int i = 0; i < nseg; ++i) {
            uint_fast32_t segment = carry | (number[i] << shift);
            carry = number[i] >> (32 - shift);
            number[i] = segment & MASK_32BIT;
        }
        if (carry > 0) {
            number[nseg] = carry;
            ++nseg;
        }
    }
    return nseg;
}

#define FREQ_BITS 10
#define FREQ_TOTAL (1 << FREQ_BITS)
#define NUM_SYMBOLS 256

int compress(unsigned char *bytes, size_t len, uint_fast32_t result[], int size, uint_fast32_t cumulf[]) {
    int nseg = 1;
    int p = len - 1;
    result[0] = cumulf[p];
    for (; p >= 0; --p) {
        unsigned char b = bytes[p];
        uint_fast32_t f = cumulf[b + 1] - cumulf[b];
        uint_fast32_t rest;
        int shift = (f == 2) ? 1 : (f == 4) ? 2 : (f == 8) ? 3 : (f == 16) ? 4 : (f == 32) ? 5 :
            (f == 64) ? 6 : (f == 128) ? 7 : (f == 256) ? 8 : (f == 512) ? 9 : (f == 1024) ? 10 : 
            0;
        if (shift) {
            nseg = long_shr(shift, result, nseg, &rest);
        }
        else {
            nseg = long_div(f, result, nseg, &rest);
        }
        nseg = long_shl(FREQ_BITS, result, nseg);
        if (nseg > 1000) {printf("@%d: %d ", p, nseg);}
        
        result[0] |= cumulf[b] + rest;
        if (nseg == 0) {
            nseg = 1;
        }
    }
    return nseg;
}

void expand(unsigned char *bytes, size_t len, uint_fast32_t source[], int nseg, uint_fast32_t cumulf[]) {
    unsigned char syms[FREQ_TOTAL];
    for (int b = 0; b < NUM_SYMBOLS; ++b) {
        uint_fast32_t start = cumulf[b], end = cumulf[b + 1];
        for (int i = start; i < end; ++i) {
            syms[i] = b;
        }
    }
    for (int i = 0; i < len; ++i) {
        uint_fast32_t rest;
        nseg = long_shr(FREQ_BITS, source, nseg, &rest);
        unsigned char b = syms[rest];
        bytes[i] = b;
        if (i < len - 1) {
            uint_fast32_t f = cumulf[b + 1] - cumulf[b];
            int shift = (f == 2) ? 1 : (f == 4) ? 2 : (f == 8) ? 3 : (f == 16) ? 4 : (f == 32) ? 5 :
                (f == 64) ? 6 : (f == 128) ? 7 : (f == 256) ? 8 : (f == 512) ? 9 : (f == 1024) ? 10 : 
                0;
            if (shift) {
                nseg = long_shl(shift, source, nseg);
            }
            else {
                nseg = long_mul(f, source, nseg);
            }
            source[0] += rest - cumulf[b];
            if (nseg == 0 && rest != cumulf[b]) {
                nseg = 1;
            }
        }
    }
}

uint_fast32_t cumulate_freqs(uint_fast32_t freqs[], unsigned char *squashed) {
    if (squashed != NULL) {
        for (int i = 0; i < NUM_SYMBOLS; ++i) {
            squashed[i] = (unsigned char) freqs[i];
        }    
    }
    int thres1 = 0;
    int thres2 = 0;
    int thres3 = 0;
    int thres4 = 0;
    int s = 0;
    for (int i = 0; i <= NUM_SYMBOLS; ++i) {
        if (!thres1 && s >= 256) {
            thres1 = i;
        }
        if (!thres2 && s >= 512) {
            thres2 = i;
        }
        if (!thres3 && s >= 768) {
            thres3 = i;
        }
        if (!thres4 && s >= 1024) {
            thres4 = i;
        }
        int f = freqs[i];
        freqs[i] = s;
        s += f;
    }
    return (thres1 - 1) << 24 | (thres2 - 1) << 16 | (thres3 - 1) << 8 | (thres4 - 1);
}

void restore_cumulf(uint_fast32_t cumulf[NUM_SYMBOLS + 1], uint_fast32_t thresholds, unsigned char *freqs) {
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
    for (int i = thres4; i <= NUM_SYMBOLS; ++i) {
        cumulf[NUM_SYMBOLS] = FREQ_TOTAL;
    }
}

void normalise_freqs(uint_fast32_t counts[NUM_SYMBOLS]) {
    uint_fast32_t tmp[NUM_SYMBOLS] = {0};
    int nz = 0;
    int top = 0;    
    int s = 0;
    for (int i = 0, max = 0; i < NUM_SYMBOLS; ++i) {
        if (counts[i]) {
            if (counts[i] > max) {
                max = counts[i];
                top = i;
            }
            s += counts[i];
            nz++;
        }
    }
    if (s != FREQ_TOTAL) {
        int shift = 31 - FREQ_BITS;
        unsigned int t = FREQ_TOTAL << shift;
        if (s > FREQ_TOTAL) {
            t -= nz << (shift - 2);
        }
        unsigned int r = t / s;
        printf("Needs adj. %d (non-zero %d), using %d / %d\n", s, nz, r, 1 << 21);
        s = 0;
        for (int i = 0; i < NUM_SYMBOLS; ++i) {
            if (counts[i]) {
                unsigned int a = (counts[i] * r + (1 << (shift - 1))) >> shift;
                tmp[i] = (a > 0) ? a : 1;
                s += tmp[i];
            }
        }/*
        for (int i = NUM_SYMBOLS - 1; i >= 0 && s < FREQ_TOTAL; --i) {
            int c = tmp[i];
            int nearest =
                (c < 3) ? c : (c < 4) ? 4 : (c < 8) ? 8 : (c < 16) ? 16 :
                (c < 32) ? 32 : (c < 64) ? 64 : (c < 128) ? 128 : c;
            int delta = 4 + (nearest >> 4);
            int d = nearest - c;
            if (d > 0 && d < delta && s + d <= FREQ_TOTAL) {
                tmp[i] = nearest;
                s += d;
            }
        }*/
        printf("Got to %d\n", s);
        if (s != FREQ_TOTAL) {
            tmp[top] += (FREQ_TOTAL - s);
        }
    }
    for (int i = 0; i < NUM_SYMBOLS; ++i) {
        counts[i] = tmp[i];
    }
}

#define SZ 4096

int main() {
    uint_fast32_t counts[257] = {0};
    unsigned char buf[SZ];
    int r = fread(buf, 1, SZ, stdin);
	for (int i = 0; i < r; ++i) {
		++counts[buf[i]];
	}
    normalise_freqs(counts);
    
    uint_fast32_t ft_freqs[NUM_SYMBOLS + 1] = {
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
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0
    };
    cumulate_freqs(ft_freqs, NULL);
    unsigned char squashed[256];
    uint_fast32_t thresholds = cumulate_freqs(counts, squashed);
    uint_fast32_t num[2000];
    int wc = compress(squashed, 256, num, 100, ft_freqs);
    printf("Used %d x 32 bit words for frequency table.\n", wc);

    unsigned char o[256] = {0};
    uint_fast32_t ft[257] = {0};
    expand(o, 256, num, wc, ft_freqs);
    restore_cumulf(ft, thresholds, o);
    int bad = 0;
    for (int i = 0; i <= 256; ++i) {
        if (counts[i] != ft[i]) {
            ++bad;
            printf("Non-matching symbol at %d, was: %.2x but got: %.2x\n", i, counts[i], o[i]);
        }
    }
    printf("\nbad = %d\n", bad);
    
    wc = compress(buf, r, num, 2000, ft);
    printf("Used %d x 32 bit words for data.\n", wc);
    unsigned char result[SZ];
    expand(result, r, num, wc, ft);
    bad = 0;
    for (int i = 0; i < r; ++i) {
        if (result[i] != buf[i]) {
            ++bad;
        }
    }
    printf("\nbad = %d\n", bad);
}
