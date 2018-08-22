#include "streamvbyte.h"

#if defined(_MSC_VER)
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
/* GCC-compatible compiler, targeting x86/x86-64 */
#include <x86intrin.h>
#endif

#ifdef __AVX__
#include "streamvbyte_shuffle_tables_0124.h"
#endif

#include <string.h> // for memcpy


static uint8_t _encode_data(uint32_t val, uint8_t *__restrict__ *dataPtrPtr) {
  uint8_t *dataPtr = *dataPtrPtr;
  uint8_t code;

  if (val == 0) { // 0 bytes
    code = 0;
  } else if (val < (1 << 8)) { // 1 bytes
    *dataPtr = (uint8_t)(val);
    *dataPtrPtr += 1;
    code = 1;
  } else if (val < (1 << 16)) { // 2 bytes
    memcpy(dataPtr, &val, 2);   // assumes little endian
    *dataPtrPtr += 2;
    code = 2;
  } else { // 4 bytes
    memcpy(dataPtr, &val, sizeof(uint32_t));
    *dataPtrPtr += sizeof(uint32_t);
    code = 3;
  }
  return code;
}

static uint8_t *svb_encode_scalar(const uint32_t *in,
                                  uint8_t *__restrict__ keyPtr,
                                  uint8_t *__restrict__ dataPtr,
                                  uint32_t count) {
  if (count == 0)
    return dataPtr; // exit immediately if no data

  uint8_t shift = 0; // cycles 0, 2, 4, 6, 0, 2, 4, 6, ...
  uint8_t key = 0;
  for (uint32_t c = 0; c < count; c++) {
    if (shift == 8) {
      shift = 0;
      *keyPtr++ = key;
      key = 0;
    }
    uint32_t val = in[c];
    uint8_t code = _encode_data(val, &dataPtr);
    key |= code << shift;
    shift += 2;
  }

  *keyPtr = key;  // write last key (no increment needed)
  return dataPtr; // pointer to first unused data byte
}

#ifdef __AVX__

static size_t streamvbyte_encode4(__m128i in, uint8_t *outData, uint8_t *outCode) {
  const __m128i Ones = _mm_set1_epi32(0x01010101);
  const __m128i GatherBits = _mm_set1_epi32(0x08040102);
  const __m128i CodeTable = _mm_set_epi32(0x03030303, 0x03030303, 0x03030303, 0x02020100);
  const __m128i GatherBytes = _mm_set_epi32(0, 0, 0x0D090501, 0x0D090501);
  const __m128i Aggregators = _mm_set_epi32(0, 0, 0x01010101, 0x10400104);

  __m128i m0, m1;
  m0 = _mm_min_epu8(in, Ones); // set byte to 1 if it is not zero
  m0 = _mm_madd_epi16(m0, GatherBits); // gather bits 8,16,24 to bits 8,9,10
  m1 = _mm_shuffle_epi8(CodeTable, m0); // translate to a 2-bit encoded symbol
  m1 = _mm_shuffle_epi8(m1, GatherBytes); // gather bytes holding symbols; 2 copies
  m1 = _mm_madd_epi16(m1, Aggregators); // sum dword_1, pack dword_0

  size_t code = (size_t)_mm_extract_epi8(m1, 1);
  size_t length = lengthTable[code];

  __m128i* shuf = (__m128i*)(((uint8_t*)encodingShuffleTable) + code * 16);
  __m128i out = _mm_shuffle_epi8(in, _mm_loadu_si128(shuf)); // todo: aligned access

  _mm_storeu_si128((__m128i *)outData, out);
  *outCode = (uint8_t)code;
  return length;
}

static size_t streamvbyte_encode_quad(const uint32_t *in, uint8_t *outData, uint8_t *outKey) {
  __m128i vin = _mm_loadu_si128((__m128i *) in );
  return streamvbyte_encode4(vin, outData, outKey);
}

#endif

size_t streamvbyte_encode_0124(const uint32_t *in, uint32_t count, uint8_t *out) {
  uint8_t *keyPtr = out;
  uint32_t keyLen = (count + 3) / 4;  // 2-bits rounded to full byte
  uint8_t *dataPtr = keyPtr + keyLen; // variable byte data after all keys
#ifdef __AVX__

  uint32_t count_quads = count / 4;
  count -= 4 * count_quads;
  for (uint32_t c = 0; c < count_quads; c++) {
    dataPtr += streamvbyte_encode_quad(in, dataPtr, keyPtr);
    keyPtr++;
    in += 4;
  }

#endif
  return svb_encode_scalar(in, keyPtr, dataPtr, count) - out;

}

#ifdef __AVX__ // though we do not require AVX per se, it is a macro that MSVC
               // will issue

static inline __m128i _decode_avx(uint32_t key,
                                  const uint8_t *__restrict__ *dataPtrPtr) {
  uint8_t len;
  __m128i Data = _mm_loadu_si128((__m128i *)*dataPtrPtr);
  uint8_t *pshuf = (char *) &shuffleTable[key];
  __m128i Shuf = *(__m128i *)pshuf;
  len = lengthTable[key];
  Data = _mm_shuffle_epi8(Data, Shuf);
  *dataPtrPtr += len;
  return Data;
}

static inline void _write_avx(uint32_t *out, __m128i Vec) {
  _mm_storeu_si128((__m128i *)out, Vec);
}

#endif // __AVX__

static inline uint32_t _decode_data(const uint8_t **dataPtrPtr, uint8_t code) {
  const uint8_t *dataPtr = *dataPtrPtr;
  uint32_t val;

  if (code == 0) { // 0 byte
    val = 0;
  } else if (code == 1) { // 1 bytes
    val = (uint32_t)*dataPtr;
    dataPtr += 1;
  } else if (code == 2) { // 2 bytes
    val = 0;
    memcpy(&val, dataPtr, 2); // assumes little endian
    dataPtr += 2;
  } else { // code == 3, 4 bytes
    memcpy(&val, dataPtr, 4);
    dataPtr += 4;
  }

  *dataPtrPtr = dataPtr;
  return val;
}

static const uint8_t *svb_decode_scalar(uint32_t *outPtr, const uint8_t *keyPtr,
                                        const uint8_t *dataPtr,
                                        uint32_t count) {
  if (count == 0)
    return dataPtr; // no reads or writes if no data

  uint8_t shift = 0;
  uint32_t key = *keyPtr++;
  for (uint32_t c = 0; c < count; c++) {
    if (shift == 8) {
      shift = 0;
      key = *keyPtr++;
    }
    uint32_t val = _decode_data(&dataPtr, (key >> shift) & 0x3);
    *outPtr++ = val;
    shift += 2;
  }

  return dataPtr; // pointer to first unused byte after end
}

#ifdef __AVX__ // though we do not require AVX per se, it is a macro that MSVC
               // will issue

static const uint8_t *svb_decode_avx_simple(uint32_t *out,
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

// Read count 32-bit integers in maskedvbyte format from in, storing the result
// in out.  Returns the number of bytes read.
size_t streamvbyte_decode_0124(const uint8_t *in, uint32_t *out, uint32_t count) {
  if (count == 0)
    return 0;


  const uint8_t *keyPtr = in;               // full list of keys is next
  uint32_t keyLen = ((count + 3) / 4);      // 2-bits per key (rounded up)
  const uint8_t *dataPtr = keyPtr + keyLen; // data starts at end of keys

#ifdef __AVX__
  dataPtr = svb_decode_avx_simple(out, keyPtr, dataPtr, count);
  out += count & ~ 31;
  keyPtr += (count/4) & ~ 7;
  count &= 31;
#endif

  return svb_decode_scalar(out, keyPtr, dataPtr, count) - in;

}
