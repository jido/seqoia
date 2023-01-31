# SQOA - Losslessly squash image file sizes, fast

## Why?

Seqoia is a derivative of QOI. Its decoder is designed to be compatible with QOI,
however the encoder is not compatible. SQOA images are about 0.7% smaller than QOI.

Compared to stb_image and stb_image_write Seqoia offers 20x-50x faster encoding,
3x-4x faster decoding and 20% better compression. It's also stupidly simple and
fits in about 400 lines of C.


## Example Usage

- [sqoaconv.c](https://github.com/jido/seqoia/blob/master/sqoaconv.c)
converts between png <> sqoa
 - [sqoabench.c](https://github.com/jido/seqoia/blob/master/sqoabench.c)
a simple wrapper to benchmark stbi, libpng and sqoa


## MIME Type, File Extension

The recommended MIME type for SQOA images is `image/sqoa`.
The recommended file extension for SQOA images is `.sqoa`


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

![QOI Logo](https://qoiformat.org/qoi-logo.svg)

### QOI - The “Quite OK Image Format” for fast, lossless image compression

Single-file MIT licensed library for C/C++

See [qoi.h](https://github.com/phoboslab/qoi/blob/master/qoi.h) for
the documentation and format specification.

More info at https://qoiformat.org
