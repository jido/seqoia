/*

Adapted from qoiconv.c:
Copyright (c) 2021, Dominic Szablewski - https://phoboslab.org
SPDX-License-Identifier: MIT


Command line tool to convert between png <> sqoa <> qoi > jpg format

Requires:
    -"stb_image.h" (https://github.com/nothings/stb/blob/master/stb_image.h)
    -"stb_image_write.h" (https://github.com/nothings/stb/blob/master/stb_image_write.h)
    -"tiny_jpeg.h" (https://github.com/serge-rgb/TinyJPEG/blob/master/tiny_jpeg.h)
    -"seqoia.h" (https://github.com/jido/seqoia/blob/sqoa-format/seqoia.h)

Compile with: 
    gcc sqoaconv.c -std=c99 -O3 -o sqoaconv

*/


#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

#define SQOA_IMPLEMENTATION
#include "seqoia.h"

#define STR_ENDS_WITH(S, E) (strcmp(S + strlen(S) - (sizeof(E)-1), E) == 0)

int main(int argc, char **argv) {
    if (argc < 3) {
        puts("Usage: sqoaconv <infile> <outfile>");
        puts("Examples:");
        puts("  sqoaconv input.png output.sqoa");
        puts("  sqoaconv input.qoi output.png");
        puts("  sqoaconv input.sqoa output.jpg");
        exit(1);
    }

    void *pixels = NULL;
    int w, h, channels;
    if (STR_ENDS_WITH(argv[1], ".png")) {
        if(!stbi_info(argv[1], &w, &h, &channels)) {
            printf("Couldn't read header %s\n", argv[1]);
            exit(1);
        }

        // Force all odd encodings to be RGBA
        if(channels != 3) {
            channels = 4;
        }

        pixels = (void *)stbi_load(argv[1], &w, &h, NULL, channels);
    }
    else if (STR_ENDS_WITH(argv[1], ".sqoa") || STR_ENDS_WITH(argv[1], ".qoi")) {
        sqoa_desc desc;
        pixels = sqoa_read(argv[1], &desc, 0);
        channels = desc.channels;
        w = desc.width;
        h = desc.height;
    }

    if (pixels == NULL) {
        printf("Couldn't load/decode %s\n", argv[1]);
        exit(1);
    }

    int encoded = 0;
    if (STR_ENDS_WITH(argv[2], ".png")) {
        encoded = stbi_write_png(argv[2], w, h, channels, pixels, 0);
    }
    else if (STR_ENDS_WITH(argv[2], ".jpg")) {
        encoded = tje_encode_to_file_at_quality(argv[2], 2, w, h, channels, pixels);
    }
    else if (STR_ENDS_WITH(argv[2], ".sqoa") || STR_ENDS_WITH(argv[2], ".qoi")) {
        encoded = sqoa_write(argv[2], pixels, &(sqoa_desc){
            .width = w,
            .height = h, 
            .channels = channels,
            .colorspace = SQOA_SRGB,
            .qoi_compat = STR_ENDS_WITH(argv[2], ".qoi")
        });
    }

    if (!encoded) {
        printf("Couldn't write/encode %s\n", argv[2]);
        exit(1);
    }

    free(pixels);
    return 0;
}
