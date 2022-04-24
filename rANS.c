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
            uint_fast64_t segment = carry + number[i] * factor;
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
#define NUM_SYMBOLS 256

int compress(unsigned char *bytes, size_t len, uint_fast32_t cumuf[], uint_fast32_t result[], int size) {
    int nseg = 1;
    int p = len - 1;
    result[0] = cumuf[p];
    for (; p >= 0; --p) {
        unsigned char b = bytes[p];
        uint_fast32_t f = cumuf[b + 1] - cumuf[b];
        uint_fast32_t rest;
        int shift = (f == 2) ? 1 : (f == 4) ? 2 : (f == 8) ? 3 : (f == 16) ? 4 :
            (f == 32) ? 5 : (f == 64) ? 6 : (f == 128) ? 7 : (f == 256) ? 8 : 0;
        if (shift) {
            nseg = long_shr(shift, result, nseg, &rest);
        }
        else {
            nseg = long_div(f, result, nseg, &rest);
        }
        nseg = long_shl(FREQ_BITS, result, nseg);
        
        result[0] |= cumuf[b] + rest;
        if (nseg == 0) {
            nseg = 1;
        }
    }
    return nseg;
}

void expand(unsigned char *bytes, size_t len, uint_fast32_t cumuf[], uint_fast32_t source[], int nseg) {
    unsigned char syms[1 << FREQ_BITS];
    for (int b = 0; b < NUM_SYMBOLS; ++b) {
        uint_fast32_t start = cumuf[b], end = cumuf[b + 1];
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
            uint_fast32_t f = cumuf[b + 1] - cumuf[b];
            int shift = (f == 2) ? 1 : (f == 4) ? 2 : (f == 8) ? 3 : (f == 16) ? 4 :
                (f == 32) ? 5 : (f == 64) ? 6 : (f == 128) ? 7 : (f == 256) ? 8 : 0;
            if (shift) {
                nseg = long_shl(shift, source, nseg);
            }
            else {
                nseg = long_mul(f, source, nseg);
            }
            source[0] += rest - cumuf[b];
            if (nseg == 0 && rest != cumuf[b]) {
                nseg = 1;
            }
        }
    }
}

void cumulate(uint_fast32_t freqs[]) {
    int s = 0;
    for (int i = 0; i <= NUM_SYMBOLS; ++i) {
        int f = freqs[i];
        freqs[i] = s;
        s += f;
    }
}

int main() {
    unsigned char counts[256] = {0};
    unsigned char buf[1024];
    int r = fread(buf, 1, 1024, stdin);
    while (r < 1024)
    {
    	r = fread(buf + r, 1, 1024 - r, stdin);
    }
	for (int i = 0; i < 1024; ++i) {
		++counts[buf[i]];
	}
    
    uint_fast32_t ft_freqs[NUM_SYMBOLS + 1] = {
        256, 256, 128, 64, 32, 16, 8, 4, 4, 4, 2, 2, 2, 2, 2, 2, 1,
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
    cumulate(ft_freqs);
    uint_fast32_t result[100];
    int wc = compress(counts, 256, ft_freqs, result, 100);
    printf("Used %d x 32 bit words.\n", wc);
    unsigned char o[256] = {0};
    expand(o, 256, ft_freqs, result, wc);
    int bad = 0;
    for (int i = 0; i < 256; ++i) {
        if (counts[i] != o[i]) {
            ++bad;
            //printf("Non-matching symbol at %d, was: %.2x but got: %.2x\n", i, counts[i], o[i]);
        }
    }
    printf("\nbad = %d\n", bad);
}
