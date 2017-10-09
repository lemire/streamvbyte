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
#elif (defined(__GNUC__) || defined(__xlC__)) && (defined(__VEC__) || defined(__ALTIVEC__))
     /* XLC or GCC-compatible compiler, targeting PowerPC with VMX/VSX */
     #include <altivec.h>
#elif defined(__GNUC__) && defined(__SPE__)
     /* GCC-compatible compiler, targeting PowerPC with SPE */
     #include <spe.h>
#endif
#ifdef __AVX__

#include "shuffle_tables.h"

#endif 
static uint8_t _encode_data(uint32_t val, uint8_t *__restrict__ *dataPtrPtr) {
	uint8_t *dataPtr = *dataPtrPtr;
	uint8_t code;

	if (val < (1 << 8)) { // 1 byte
		*dataPtr = (uint8_t)(val);
		*dataPtrPtr += 1;
		code = 0;
	} else if (val < (1 << 16)) { // 2 bytes
		*(uint16_t *) dataPtr = (uint16_t)(val);
		*dataPtrPtr += 2;
		code = 1;
	} else if (val < (1 << 24)) { // 3 bytes
		*(uint16_t *) dataPtr = (uint16_t)(val);
		*(dataPtr + 2) = (uint8_t)(val >> 16);
		*dataPtrPtr += 3;
		code = 2;
	} else { // 4 bytes
		*(uint32_t *) dataPtr = val;
		*dataPtrPtr += 4;
		code = 3;
	}

	return code;
}

static uint8_t *svb_encode_scalar(const uint32_t *in,
		uint8_t *__restrict__ keyPtr, uint8_t *__restrict__ dataPtr,
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

typedef union M128 {
  char i8[16];
  uint32_t u32[4];
  __m128i i128;
} u128;

const u128 Ones = {.i8 = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}};

// bithack 3 byte lsb's shift/or into high byte via multiply
#define shifter (1|1<<9|1<<18)
const u128 Shifts = {.u32 = {shifter,shifter,shifter,shifter}};

// translate 3-bit maps into lane codes
const u128 LaneCodes   = {.i8 = {0,3,2,3,1,3,2,3,-1,-1,-1,-1,-1,-1,-1,-1}};

// gather high bytes from each lane, 2 copies
const u128 GatherHi = {.i8 = {15,11,7,3,15,11,7,3,-1,-1,-1,-1,-1,-1,-1,-1}};

// mul-shift magic numbers
// concatenate 2-bit lane codes into high byte
#define concat (1|1<<10|1<<20|1<<30)
// sum lane codes in high byte
#define sum    (1|1<<8|1<<16|1<<24)
const u128 Aggregators = {.u32 = {concat, sum, 0, 0}};

size_t streamvbyte_encode4(u128 *in, uint8_t *outData, uint8_t *outCode) {

  __m128i mins      = _mm_min_epu8(in->i128, Ones.i128);
  __m128i bytemaps  = _mm_mullo_epi32( mins, Shifts.i128);
  __m128i lanecodes = _mm_shuffle_epi8(LaneCodes.i128, bytemaps);
  __m128i hibytes   = _mm_shuffle_epi8(lanecodes, GatherHi.i128);

  u128 codeAndLength = {.i128 = _mm_mullo_epi32(hibytes, Aggregators.i128)};
  uint8_t code  = codeAndLength.i8[3];
  size_t length = codeAndLength.i8[7] + 4;

  __m128i Shuf = *(__m128i *) &encodingShuffleTable[code];
  __m128i outAligned = _mm_shuffle_epi8(in->i128, Shuf);

  _mm_storeu_si128 ((__m128i *) outData, outAligned);
  *outCode = code;
  return length;
}

static uint8_t *svb_encode_vector(const uint32_t *in,
				  uint8_t *__restrict__ keyPtr, uint8_t *__restrict__ dataPtr,
				  uint32_t count) {

  uint8_t * outData = dataPtr;
  uint8_t * outKey  = keyPtr;

  uint32_t count4 = count/4;

  for (uint32_t c = 0; c < count4 ; c++)
    outData += streamvbyte_encode4((u128 *) (in + 4*c), outData, outKey++);

  outData = svb_encode_scalar(in, outKey, outData, count - 4*count4);

  return outData;
}

#endif

// Encode an array of a given length read from in to bout in streamvbyte format.
// Returns the number of bytes written.
size_t streamvbyte_encode(uint32_t *in, uint32_t count, uint8_t *out) {
	uint8_t *keyPtr = out;
	uint32_t keyLen = (count + 3) / 4; // 2-bits rounded to full byte
	uint8_t *dataPtr = keyPtr + keyLen; // variable byte data after all keys
#ifdef __AVX__ 
	return svb_encode_vector(in, keyPtr, dataPtr, count) - out;
#else
	return svb_encode_scalar(in, keyPtr, dataPtr, count) - out;
#endif
}

#ifdef __AVX__ // though we do not require AVX per se, it is a macro that MSVC will issue

static inline __m128i _decode_avx(uint32_t key,
		const uint8_t *__restrict__ *dataPtrPtr) {
	uint8_t len = lengthTable[key];
	__m128i Data = _mm_loadu_si128((__m128i *) *dataPtrPtr);
	__m128i Shuf = *(__m128i *) &shuffleTable[key];

	Data = _mm_shuffle_epi8(Data, Shuf);
	*dataPtrPtr += len;
	return Data;
}

static inline void _write_avx(uint32_t *out, __m128i Vec) {
	_mm_storeu_si128((__m128i *) out, Vec);
}

#endif // __AVX__


static inline uint32_t _decode_data(const uint8_t **dataPtrPtr, uint8_t code) {
	const uint8_t *dataPtr = *dataPtrPtr;
	uint32_t val;

	if (code == 0) { // 1 byte
		val = (uint32_t) * dataPtr;
		dataPtr += 1;
	} else if (code == 1) { // 2 bytes
		val = (uint32_t) * (uint16_t *) dataPtr;
		dataPtr += 2;
	} else if (code == 2) { // 3 bytes
		val = (uint32_t) * (uint16_t *) dataPtr;
		val |= *(dataPtr + 2) << 16;
		dataPtr += 3;
	} else {                      // code == 3
		val = *(uint32_t *) dataPtr; // 4 bytes
		dataPtr += 4;
	}

	*dataPtrPtr = dataPtr;
	return val;
}
static const uint8_t *svb_decode_scalar(uint32_t *outPtr, const uint8_t *keyPtr,
		const uint8_t *dataPtr, uint32_t count) {
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
#ifdef __AVX__ // though we do not require AVX per se, it is a macro that MSVC will issue

const uint8_t *svb_decode_avx_simple(uint32_t *out,
		const uint8_t *__restrict__ keyPtr, const uint8_t *__restrict__ dataPtr,
		uint64_t count) {

	uint64_t keybytes = count / 4; // number of key bytes
	__m128i Data;
	if (keybytes >= 8) {

		int64_t Offset = -(int64_t) keybytes / 8 + 1;

		const uint64_t *keyPtr64 = (const uint64_t *) keyPtr - Offset;
		uint64_t nextkeys = keyPtr64[Offset];
		for (; Offset != 0; ++Offset) {
			uint64_t keys = nextkeys;
			nextkeys = keyPtr64[Offset + 1];

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
	uint64_t consumedkeys = keybytes - (keybytes & 7);
	return svb_decode_scalar(out, keyPtr + consumedkeys, dataPtr, count & 31);
}



#endif


// Read count 32-bit integers in maskedvbyte format from in, storing the result in out.  Returns the number of bytes read.
size_t streamvbyte_decode(const uint8_t* in, uint32_t* out, uint32_t count) {
	if (count == 0)
		return 0;
	const uint8_t *keyPtr = in;            // full list of keys is next
	uint32_t keyLen = ((count + 3) / 4); // 2-bits per key (rounded up)
	const uint8_t *dataPtr = keyPtr + keyLen;  // data starts at end of keys
#ifdef __AVX__
	return svb_decode_avx_simple(out, keyPtr, dataPtr, count) - in;
#else
	return svb_decode_scalar(out, keyPtr, dataPtr, count) - in;
#endif
} 
