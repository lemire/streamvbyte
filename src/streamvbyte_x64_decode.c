#include "streamvbyte_isadetection.h"
#ifdef STREAMVBYTE_X64
static inline __m128i _decode_avx(uint32_t key,
                                  const uint8_t *__restrict__ *dataPtrPtr) {
  uint8_t len;
  __m128i Data = _mm_loadu_si128((__m128i *)*dataPtrPtr);
  uint8_t *pshuf = (uint8_t *) &shuffleTable[key];
  __m128i Shuf = *(__m128i *)pshuf;
#ifdef AVOIDLENGTHLOOKUP
  // this avoids the dependency on lengthTable,
  // see https://github.com/lemire/streamvbyte/issues/12
  len = pshuf[12 + (key >> 6)] + 1;
#else
  len = lengthTable[key];
#endif
  Data = _mm_shuffle_epi8(Data, Shuf);
  *dataPtrPtr += len;
  return Data;
}

static inline void _write_avx(uint32_t *out, __m128i Vec) {
  _mm_storeu_si128((__m128i *)out, Vec);
}




const uint8_t *svb_decode_avx_simple(uint32_t *out,
                                     const uint8_t *__restrict__ keyPtr,
                                     const uint8_t *__restrict__ dataPtr,
                                     uint64_t count) {

  uint64_t keybytes = count / 4; // number of key bytes
  __m128i Data;
  if (keybytes >= 8) {

    int64_t Offset = -(int64_t)keybytes / 8 + 1;

    const uint64_t *keyPtr64 = (const uint64_t *)keyPtr - Offset;
    uint64_t nextkeys;
    memcpy(&nextkeys, keyPtr64 + Offset, sizeof(nextkeys));
    for (; Offset != 0; ++Offset) {
      uint64_t keys = nextkeys;
      memcpy(&nextkeys, keyPtr64 + Offset + 1, sizeof(nextkeys));

      Data = _decode_avx((keys & 0xFF), &dataPtr);
      _write_avx(out, Data);
      Data = _decode_avx((keys & 0xFF00) >> 8, &dataPtr);
      _write_avx(out + 4, Data);

      keys >>= 16;
      Data = _decode_avx((keys & 0xFF), &dataPtr);
      _write_avx(out + 8, Data);
      Data = _decode_avx((keys & 0xFF00) >> 8, &dataPtr);
      _write_avx(out + 12, Data);

      keys >>= 16;
      Data = _decode_avx((keys & 0xFF), &dataPtr);
      _write_avx(out + 16, Data);
      Data = _decode_avx((keys & 0xFF00) >> 8, &dataPtr);
      _write_avx(out + 20, Data);

      keys >>= 16;
      Data = _decode_avx((keys & 0xFF), &dataPtr);
      _write_avx(out + 24, Data);
      Data = _decode_avx((keys & 0xFF00) >> 8, &dataPtr);
      _write_avx(out + 28, Data);

      out += 32;
    }
    {
      uint64_t keys = nextkeys;

      Data = _decode_avx((keys & 0xFF), &dataPtr);
      _write_avx(out, Data);
      Data = _decode_avx((keys & 0xFF00) >> 8, &dataPtr);
      _write_avx(out + 4, Data);

      keys >>= 16;
      Data = _decode_avx((keys & 0xFF), &dataPtr);
      _write_avx(out + 8, Data);
      Data = _decode_avx((keys & 0xFF00) >> 8, &dataPtr);
      _write_avx(out + 12, Data);

      keys >>= 16;
      Data = _decode_avx((keys & 0xFF), &dataPtr);
      _write_avx(out + 16, Data);
      Data = _decode_avx((keys & 0xFF00) >> 8, &dataPtr);
      _write_avx(out + 20, Data);

      keys >>= 16;
      Data = _decode_avx((keys & 0xFF), &dataPtr);
      _write_avx(out + 24, Data);
      Data = _decode_avx((keys & 0xFF00) >> 8, &dataPtr);
      _write_avx(out + 28, Data);

      out += 32;
    }
  }

  return dataPtr;
}
#endif