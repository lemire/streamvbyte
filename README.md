streamvbyte
===========
[![Build Status](https://travis-ci.org/lemire/streamvbyte.png)](https://travis-ci.org/lemire/streamvbyte)

StreamVByte is a new integer compression technique that applies SIMD instructions (vectorization) to
Google's Group Varint approach. The net result is faster than other byte-oriented compression
techniques.

The approach is patent-free, the code is available under the Apache License.


It includes fast differential coding.

It assumes a recent Intel processor (e.g., haswell or better) or an ARM processor with NEON instructions (which is almost all of them).

The code should build using most standard-compliant C99 compilers. The provided makefile
expects a Linux-like system.

This library is used by [UpscaleDB](https://github.com/cruppstahl/upscaledb), Redis' [RediSearch](https://github.com/RedisLabsModules/RediSearch) and by the  [Trinity Information Retrieval framework](https://github.com/phaistos-networks/Trinity).



Usage with Makefile:

      make
      ./unit

Usage with CMake:

The cmake build system also offers a `libstreamvbyte_static.a` in addition to
`libstreamvbyte.so`.

`-DCMAKE_INSTALL_PREFIX:PATH=/path/to/install` is optional.
Defaults to /usr/local{include,lib}

By default, the project builds with ``-march=native`` (except on MSVC), use ``-DSTREAMVBYTE_DISABLE_NATIVE=ON`` to disable.

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX:PATH=/path/to/install \
make install

# run the tests like:
ctest -V

```

See example.c for an example.

Short code sample:
```C
// suppose that datain is an array of uint32_t integers
size_t compsize = streamvbyte_encode(datain, N, compressedbuffer); // encoding
// here the result is stored in compressedbuffer using compsize bytes
streamvbyte_decode(compressedbuffer, recovdata, N); // decoding (fast)
```

If the values are sorted, then it might be preferable to use differential coding:
```C
// suppose that datain is an array of uint32_t integers
size_t compsize = streamvbyte_delta_encode(datain, N, compressedbuffer,0); // encoding
// here the result is stored in compressedbuffer using compsize bytes
streamvbyte_delta_decode(compressedbuffer, recovdata, N,0); // decoding (fast)
```
You have to know how many integers were coded when you decompress. You can store this
information along with the compressed stream.


Signed integers
-----------------

We do not directly support signed integers, but you can use fast functions to convert signed integers to unsigned integers.

```C

#include "streamvbyte_zigzag.h"

zigzag_encode(mysignedints, myunsignedints, number); // mysignedints => myunsignedints

zigzag_decode(myunsignedints, mysignedints, number); // myunsignedints => mysignedints
```

Installation
----------------

You can install the library (as a dynamic library) on your machine if you have root access:

      sudo make install

To uninstall, simply type:

      sudo make uninstall

It is recommended that you try ``make dyntest`` before proceeding.

Benchmarking
-----------------

You can try to benchmark the speed in this manner:

      make perf
      ./perf

Make sure to run ``make test`` before, as a sanity test.

Technical posts
---------------

* [Trinity Updates and integer codes benchmarks](https://medium.com/@markpapadakis/trinity-updates-and-integer-codes-benchmarks-6a4fa2eb3fd1) by Mark Papadakis
* [Stream VByte: breaking new speed records for integer compression](https://lemire.me/blog/2017/09/27/stream-vbyte-breaking-new-speed-records-for-integer-compression/) by Daniel Lemire


Alternative encoding
-------------------------------

By default, Stream VByte uses 1, 2, 3 or 4 bytes per integer.
In the case where you expect many of your integers to be zero, you might try
the ``streamvbyte_encode_0124`` and ``streamvbyte_decode_0124`` which use
0, 1, 2, or 4 bytes per integer.


Stream VByte in other languages
--------------------------------

* There is a [Rust version](https://bitbucket.org/marshallpierce/stream-vbyte-rust) by Marshall Pierce.
* There is a [Go version](https://github.com/nelz9999/stream-vbyte-go) by Nelz.

Format Specification
---------------------

We specify the format as follows.

We do not store how many integers (``count``) are compressed
in the compressed data per se. If you want to store
the data stream (e.g., to disk), you need to add this
information. It is intentionally left out because, in
applications, it is often the case that there are better
ways to store this count.

There are two streams:

- The data starts with an array of "control bytes". There
   are (count + 3) / 4 of them.
- Following the array of control bytes, there are data bytes.

We can interpret the control bytes as a sequence of 2-bit words.
The first 2-bit word is made of the least significant 2 bits
in the first byte, and so forth. There are four 2-bit words
written in each byte.

Starting from the first 2-bit word, we have corresponding
sequence in the data bytes, written in sequence from the beginning:
 - When the 2-bit word is 00, there is a single data byte.
 - When the 2-bit words is 01, there are two data bytes.
 - When the 2-bit words is 10, there are three data bytes.
 - When the 2-bit words is 11, there are four data bytes.

The data bytes are stored using a little-endian encoding.


Consider the following example:

```
control bytes: [0x40 0x55 ... ]
data bytes: [0x00 0x64 0xc8 0x2c 0x01 0x90  0x01 0xf4 0x01 0x58 0x02 0xbc 0x02 ...]
```

The first control byte is 0x40 or the four 2-bit words : ``00 00 00 01``.
The second control byte is 0x55 or the four 2-bit words : ``01 01 01 01``.
Thus the first three values are given by the first three bytes:
``0x00, 0x64, 0xc8`` (or 0, 100, 200 in base 10). The five next values are stored
using two bytes each: ``0x2c 0x01, 0x90  0x01, 0xf4 0x01, 0x58 0x02, 0xbc 0x02``.
As little endian integers, these are to be interpreted as 300, 400, 500, 600, 700.

Thus, to recap, the sequence of integers (0,100,200,300,400,500,600,700) gets encoded as the 15 bytes  ``0x40 0x55 0x00 0x64 0xc8 0x2c 0x01 0x90  0x01 0xf4 0x01 0x58 0x02 0xbc 0x02``.

If the ``count``is not divisible by four, then we include a final partial group where we use zero 2-bit corresponding to no data byte.

Reference
---------

* Daniel Lemire, Nathan Kurz, Christoph Rupp, [Stream VByte: Faster Byte-Oriented Integer Compression](https://arxiv.org/abs/1709.08990), Information Processing Letters 130, 2018.

See also
--------
* SIMDCompressionAndIntersection: A C++ library to compress and intersect sorted lists of integers using SIMD instructions https://github.com/lemire/SIMDCompressionAndIntersection
* The FastPFOR C++ library : Fast integer compression https://github.com/lemire/FastPFor
* High-performance dictionary coding https://github.com/lemire/dictionary
* LittleIntPacker: C library to pack and unpack short arrays of integers as fast as possible https://github.com/lemire/LittleIntPacker
* The SIMDComp library: A simple C library for compressing lists of integers using binary packing https://github.com/lemire/simdcomp
* MaskedVByte: Fast decoder for VByte-compressed integers https://github.com/lemire/MaskedVByte
* CSharpFastPFOR: A C#  integer compression library  https://github.com/Genbox/CSharpFastPFOR
* JavaFastPFOR: A java integer compression library https://github.com/lemire/JavaFastPFOR
* Encoding: Integer Compression Libraries for Go https://github.com/zhenjl/encoding
* FrameOfReference is a C++ library dedicated to frame-of-reference (FOR) compression: https://github.com/lemire/FrameOfReference
* libvbyte: A fast implementation for varbyte 32bit/64bit integer compression https://github.com/cruppstahl/libvbyte
* TurboPFor is a C library that offers lots of interesting optimizations. Well worth checking! (GPL license) https://github.com/powturbo/TurboPFor
* Oroch is a C++ library that offers a usable API (MIT license) https://github.com/ademakov/Oroch
