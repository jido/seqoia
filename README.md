# SQOA - Losslessly squash image file sizes, fast

## Why?

Seqoia is a derivative of QOI. It can read and write QOI files in addition to
SQOA files. SQOA images are about 0.7% smaller than QOI and they are amenable 
to further reduction using a generic compression tool.

Compared to stb_image and stb_image_write Seqoia offers 20x-50x faster encoding,
3x-4x faster decoding and 20% better compression. It's also stupidly simple and
fits in about 400 lines of C.


## Example Usage

- [sqoaconv.c](https://github.com/jido/seqoia/blob/sqoa-format/sqoaconv.c)
converts between png <> sqoa <> qoi
 - [sqoabench.c](https://github.com/jido/seqoia/blob/sqoa-format/sqoabench.c)
a simple wrapper to benchmark stbi, libpng, qoi and sqoa


## MIME Type, File Extension

The recommended MIME type for SQOA images is `image/sqoa`.
The recommended file extension for SQOA images is `.sqoa`


## Benchmark results

Tested on Apple MacBook Air M1

Compiler command:

```
clang -o sqoabench -I/opt/homebrew/include -L/opt/homebrew/lib -lpng -std=gnu99 -O3 sqoabench.c
```

Benchmark command:

```
./sqoabench 10 ../qoi/images --onlytotals
```

**Results:**

> [bench10.txt](https://github.com/jido/seqoia/blob/sqoa-format/bench10.txt)

_Seqoia_ compresses better than _QOI_ on synthetic images like icons.

### Why you should compress your SQOA files

Total size of PNG files in the "images" folder:

```
 1144723797
```

Total size of SQOA files before compression:

```
 1342694532
```

Command:

```
gzip -r sqoaimages
```

Total size of SQOA files after compression:

```
 1060163477
```

The results are smaller than the original PNG folder, all while using a fraction of the power/time to generate them!

For comparison, the total size of STBI-generated PNG files is 1637952729 (STBI is much slower).

## Limitations

The SQOA file format allows for huge images with up to 18 exa-pixels. A streaming 
en-/decoder can handle these with minimal RAM requirements, assuming there is 
enough storage space.

This particular implementation of Seqoia however is limited to images with a 
maximum size of 400 million pixels. It will safely refuse to en-/decode anything
larger than that. This is not a streaming en-/decoder. It loads the whole image
file into RAM before doing any work and is not extensively optimized for 
performance (but it's still very fast).


## Original Project

### QOI - The “Quite OK Image Format” for fast, lossless image compression

Single-file MIT licensed library for C/C++

See [qoi.h](https://github.com/phoboslab/qoi/blob/master/qoi.h) for
the documentation and format specification.

More info at https://qoiformat.org

![QOI Logo](https://qoiformat.org/qoi-logo.svg)
