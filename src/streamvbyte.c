#include "streamvbyte.h"

#if defined(_MSC_VER)
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
/* GCC-compatible compiler, targeting x86/x86-64 */
#include <x86intrin.h>
#elif defined(__GNUC__) && defined(__ARM_NEON__)
/* GCC-compatible compiler, targeting ARM with NEON */
#include <arm_neon.h>
#elif defined(__GNUC__) && defined(__IWMMXT__)
/* GCC-compatible compiler, targeting ARM with WMMX */
#include <mmintrin.h>
#elif (defined(__GNUC__) || defined(__xlC__)) &&                               \
    (defined(__VEC__) || defined(__ALTIVEC__))
/* XLC or GCC-compatible compiler, targeting PowerPC with VMX/VSX */
#include <altivec.h>
#elif defined(__GNUC__) && defined(__SPE__)
/* GCC-compatible compiler, targeting PowerPC with SPE */
#include <spe.h>
#endif
#ifdef __AVX__

#include "streamvbyte_shuffle_tables.h"

#endif
#include <string.h> // for memcpy


static uint8_t _encode_data(uint32_t val, uint8_t *__restrict__ *dataPtrPtr) {
  uint8_t *dataPtr = *dataPtrPtr;
  uint8_t code;

  if (val < (1 << 8)) { // 1 byte
    *dataPtr = (uint8_t)(val);
    *dataPtrPtr += 1;
    code = 0;
  } else if (val < (1 << 16)) { // 2 bytes
    memcpy(dataPtr, &val, 2);   // assumes little endian
    *dataPtrPtr += 2;
    code = 1;
  } else if (val < (1 << 24)) { // 3 bytes
    memcpy(dataPtr, &val, 3);   // assumes little endian
    *dataPtrPtr += 3;
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

#ifdef __ARM_NEON__

#include "streamvbyte_shuffle_tables.h"
static const uint8_t pgatherlo[] = {12, 8, 4, 0, 12, 8, 4, 0}; // apparently only used in streamvbyte_encode4
#define concat (1 | 1 << 10 | 1 << 20 | 1 << 30)
#define sum (1 | 1 << 8 | 1 << 16 | 1 << 24)
static const  uint32_t pAggregators[2] = {concat, sum}; // apparently only used in streamvbyte_encode4

static inline size_t streamvbyte_encode4(uint32x4_t data, uint8_t *__restrict__ outData, uint8_t *__restrict__ outCode) {

  const uint8x8_t gatherlo = vld1_u8(pgatherlo);
  const uint32x2_t Aggregators = vld1_u32(pAggregators);

  // lane code is 3 - (saturating sub) (clz(data)/8)
  uint32x4_t clzbytes = vshrq_n_u32(vclzq_u32(data), 3);
  uint32x4_t lanecodes = vqsubq_u32(vdupq_n_u32(3), clzbytes);

  // nops
  uint8x16_t lanebytes = vreinterpretq_u8_u32(lanecodes);
#ifdef __aarch64__
  uint8x8_t lobytes = vqtbl1_u8( lanebytes, gatherlo );
#else
  uint8x8x2_t twohalves = {{vget_low_u8(lanebytes), vget_high_u8(lanebytes)}};

  // shuffle lsbytes into two copies of an int
  uint8x8_t lobytes = vtbl2_u8(twohalves, gatherlo);
#endif

  uint32x2_t mulshift = vreinterpret_u32_u8(lobytes);

  uint32_t codeAndLength[2];
  vst1_u32(codeAndLength, vmul_u32(mulshift, Aggregators));

  uint32_t code = codeAndLength[0] >> 24;
  size_t length = 4 + (codeAndLength[1] >> 24);

  // shuffle in 8-byte chunks
  uint8x16_t databytes = vreinterpretq_u8_u32(data);
  uint8x16_t encodingShuffle = vld1q_u8((uint8_t *) &encodingShuffleTable[code]);
#ifdef __aarch64__
  vst1q_u8(outData, vqtbl1q_u8(databytes, encodingShuffle));
#else
  uint8x8x2_t datahalves = {{vget_low_u8(databytes), vget_high_u8(databytes)}};
  vst1_u8(outData, vtbl2_u8(datahalves, vget_low_u8(encodingShuffle)));
  vst1_u8(outData + 8, vtbl2_u8(datahalves, vget_high_u8(encodingShuffle)));
#endif

  *outCode = (uint8_t) code;
  return length;
}

static inline size_t streamvbyte_encode_quad(const uint32_t *__restrict__ in, uint8_t *__restrict__ outData, uint8_t *__restrict__ outCode) {
  uint32x4_t inq = vld1q_u32(in);

  return streamvbyte_encode4(inq, outData, outCode);
}

#ifdef __aarch64__
typedef uint8x16_t decode_t;
#else
typedef uint8x8x2_t decode_t;
#endif
static inline decode_t  _decode_neon(const uint8_t key,
					const uint8_t * restrict *dataPtrPtr) {

  uint8_t len;
  uint8_t *pshuf = &shuffleTable[key];
  uint8x16_t decodingShuffle = vld1q_u8(pshuf);

  uint8x16_t compressed = vld1q_u8(*dataPtrPtr);
#ifdef AVOIDLENGTHLOOKUP
  // this avoids the dependency on lengthTable,
  // see https://github.com/lemire/streamvbyte/issues/12
  len = pshuf[12 + (key >> 6)] + 1;
#else
  len = lengthTable[key];
#endif
#ifdef __aarch64__
  uint8x16_t data = vqtbl1q_u8(compressed, decodingShuffle);
#else
  uint8x8x2_t codehalves = {{vget_low_u8(compressed), vget_high_u8(compressed)}};

  uint8x8x2_t data = {{vtbl2_u8(codehalves, vget_low_u8(decodingShuffle)),
		       vtbl2_u8(codehalves, vget_high_u8(decodingShuffle))}};
#endif
  *dataPtrPtr += len;
  return data;
}

static void streamvbyte_decode_quad( const uint8_t * restrict *dataPtrPtr, uint8_t key, uint32_t * restrict out ) {
  decode_t data =_decode_neon( key, dataPtrPtr );
#ifdef __aarch64__
  vst1q_u8((uint8_t *) out, data);
#else
  vst1_u8((uint8_t *) out, data.val[0]);
  vst1_u8((uint8_t *) (out + 2), data.val[1]);
#endif
}

static const uint8_t *svb_decode_vector(uint32_t *out, const uint8_t *keyPtr, const uint8_t *dataPtr, uint32_t count) {
  for(uint32_t i = 0; i < count/4; i++)
    streamvbyte_decode_quad( &dataPtr, keyPtr[i], out + 4*i );

  return dataPtr;
}

#endif

#ifdef __AVX__

size_t streamvbyte_encode4(__m128i in, uint8_t *outData, uint8_t *outCode) {
  const __m128i Ones = _mm_set1_epi32(0x01010101);
  const __m128i GatherBits = _mm_set1_epi32(0x02040001);
  const __m128i CodeTable = _mm_set_epi32(0, 0, 0x03030303, 0x02020100);
  const __m128i GatherBytes = _mm_set_epi32(0, 0, 0x0D090501, 0x0D090501);
  const __m128i Aggregators = _mm_set_epi32(0, 0, 0x01010101, 0x10400104);

  __m128i m0, m1;
  m0 = _mm_min_epu8(in, Ones); // set byte to 1 if it is not zero
  m0 = _mm_madd_epi16(m0, GatherBits); // gather bits 8,16,24 to bits 8,9,10
  m1 = _mm_shuffle_epi8(CodeTable, m0); // translate to a 2-bit encoded symbol
  m1 = _mm_shuffle_epi8(m1, GatherBytes); // gather bytes holding symbols; 2 copies
  m1 = _mm_madd_epi16(m1, Aggregators); // sum dword_1, pack dword_0

  // extract data length and decode key
  size_t code = (size_t)_mm_extract_epi8(m1, 1);
  size_t length = 4 + (size_t)_mm_extract_epi8(m1, 5);

  __m128i* shuf = (__m128i*)(((uint8_t*)encodingShuffleTable) + code * 16);
  __m128i out = _mm_shuffle_epi8(in, _mm_loadu_si128(shuf)); // todo: aligned access

  _mm_storeu_si128((__m128i *)outData, out);
  *outCode = (uint8_t)code;
  return length;
}

size_t streamvbyte_encode_quad(const uint32_t *in, uint8_t *outData, uint8_t *outKey) {
    __m128i vin = _mm_loadu_si128((__m128i *) in );
    return streamvbyte_encode4(vin, outData, outKey);
}

#endif

// Encode an array of a given length read from in to bout in streamvbyte format.
// Returns the number of bytes written.
size_t streamvbyte_encode(const uint32_t *in, uint32_t count, uint8_t *out) {
  uint8_t *keyPtr = out;
  uint32_t keyLen = (count + 3) / 4;  // 2-bits rounded to full byte
  uint8_t *dataPtr = keyPtr + keyLen; // variable byte data after all keys

#if defined(__AVX__) || defined(__ARM_NEON__)

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
  uint8_t *pshuf = &shuffleTable[key];
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

#endif // __AVX__

static inline uint32_t _decode_data(const uint8_t **dataPtrPtr, uint8_t code) {
  const uint8_t *dataPtr = *dataPtrPtr;
  uint32_t val;

  if (code == 0) { // 1 byte
    val = (uint32_t)*dataPtr;
    dataPtr += 1;
  } else if (code == 1) { // 2 bytes
    val = 0;
    memcpy(&val, dataPtr, 2); // assumes little endian
    dataPtr += 2;
  } else if (code == 2) { // 3 bytes
    val = 0;
    memcpy(&val, dataPtr, 3); // assumes little endian
    dataPtr += 3;
  } else { // code == 3
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

// Read count 32-bit integers in maskedvbyte format from in, storing the result
// in out.  Returns the number of bytes read.
size_t streamvbyte_decode(const uint8_t *in, uint32_t *out, uint32_t count) {
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
#elif defined(__ARM_NEON__)
  dataPtr = svb_decode_vector(out, keyPtr, dataPtr, count);
  out += count - (count & 3);
  keyPtr += count/4;
  count &= 3;
#endif

  return svb_decode_scalar(out, keyPtr, dataPtr, count) - in;

}
