#include "streamvbytedelta.h"
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

#include <string.h> // for memcpy

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

static const uint8_t *svb_decode_scalar_d1_init(uint32_t *outPtr,
                                         const uint8_t *keyPtr,
                                         const uint8_t *dataPtr, uint32_t count,
                                         uint32_t prev) {
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
    val += prev;
    *outPtr++ = val;
    prev = val;
    shift += 2;
  }

  return dataPtr; // pointer to first unused byte after end
}

#ifdef STREAMVBYTE_X64
#include "streamvbytedelta_x64_decode.c"
#endif








size_t streamvbyte_delta_decode(const uint8_t *in, uint32_t *out,
                                uint32_t count, uint32_t prev) {
  uint32_t keyLen = ((count + 3) / 4); // 2-bits per key (rounded up)
  const uint8_t *keyPtr = in;
  const uint8_t *dataPtr = keyPtr + keyLen; // data starts at end of keys
#ifdef STREAMVBYTE_X64
  if(streamvbyte_ssse3()) {
    return svb_decode_avx_d1_init(out, keyPtr, dataPtr, count, prev) - in;
  }
#else
  return svb_decode_scalar_d1_init(out, keyPtr, dataPtr, count, prev) - in;
#endif
}
