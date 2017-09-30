streamvbyte
===========
[![Build Status](https://travis-ci.org/lemire/streamvbyte.png)](https://travis-ci.org/lemire/streamvbyte)

StreamVByte is a new integer compression technique that applies SIMD instructions (vectorization) to
Google's Group Varint approach. The net result is faster than other byte-oriented compression
techniques.

The approach is patent-free, the code is available under the Apache License.


It includes fast differential coding.

It assumes a recent Intel processor (e.g., haswell or better) .

The code should build using most standard-compliant C99 compilers. The provided makefile
expects a Linux-like system.


Usage:

      make
      ./unit

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

Technical posts
---------------

* [Trinity Updates and integer codes benchmarks](https://medium.com/@markpapadakis/trinity-updates-and-integer-codes-benchmarks-6a4fa2eb3fd1) by Mark Papadakis
* [Stream VByte: breaking new speed records for integer compression](https://lemire.me/blog/2017/09/27/stream-vbyte-breaking-new-speed-records-for-integer-compression/) by Daniel Lemire


Stream VByte in other languages
--------------------------------

* There is a [Rust version](https://bitbucket.org/marshallpierce/stream-vbyte-rust) by Marshall Pierce.

Reference
---------

* Daniel Lemire, Nathan Kurz, Christoph Rupp, [Stream VByte: Faster Byte-Oriented Integer Compression](https://arxiv.org/abs/1709.08990), Information Processing Letters (to appear)

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

