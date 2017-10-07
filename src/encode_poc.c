
#include <x86intrin.h>
#include <stdio.h>
#include "shuffle_tables.h"

// POC standalone test until code looks ok
// usage:  gcc -msse4.1 -o encode_poc encode_poc.c

void dump(__m128i x, char *msg) {
  uint32_t out[4];
  _mm_storeu_si128((__m128i *) out, x);

  printf("%s ", msg);
  for( int i=0; i<4; i++)
    printf("%d,", out[i]);
  printf("\n");

  printf("%s ", msg);
  for( int i=0; i<4; i++)
    printf("%08x,", out[i]);
  printf("\n");
}

typedef union M128 {
  char i8[16];
  uint32_t u32[4];
  __m128i i128;
} u128;

const u128 Ones = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

// bithack 3 byte lsb's shift/or into high byte via multiply
#define shifter (1|1<<9|1<<18)
const u128 Shifts = {.u32 = {shifter,shifter,shifter,shifter}};

// translate 3-bit maps into lane codes
const u128 LaneCodes   = {0,3,2,3,1,3,2,3,0,0,0,0,0,0,0,0};

const u128 CollectHi = {15,11,7,3,15,11,7,3,0,0,0,0,0,0,0,0};

// mul-shift 2-bit lane codes into high byte
#define concat (1|1<<10|1<<20|1<<30)
#define sum    (1|1<<8|1<<16|1<<24)
const u128 Aggregates = {.u32 = {concat, sum, 0, 0}};

size_t streamvbyte_encode(u128 *in, uint8_t *outData, uint8_t *outCode) {

  __m128i mins = _mm_min_epu8(in->i128, Ones.i128);
  __m128i bytemaps = _mm_mullo_epi32( mins, Shifts.i128);
  __m128i lanecodes = _mm_shuffle_epi8(LaneCodes.i128, bytemaps);

  // pshufb high bytes into lanes 0,1
  __m128i hibytes = _mm_shuffle_epi8(lanecodes, CollectHi.i128);

  u128 codeAndLength = {.i128 = _mm_mullo_epi32(hibytes, Aggregates.i128)};
  uint8_t code  = codeAndLength.i8[3];
  size_t length = codeAndLength.i8[7] + 4;

  // get shuffle[code]
  __m128i Shuf = *(__m128i *) &encodingShuffleTable[code];

  *(__m128i *) outData = _mm_shuffle_epi8(in->i128, Shuf);

  *outCode = code;
  return length;
}

int main() {
  uint32_t data[] = { 0xaa, 0xbbb, 0xcccccc, 0xdddddddd };
  uint8_t outData[16], outCode[1];
  int x;

  size_t len = streamvbyte_encode((u128 *) data, outData, outCode);
  printf("code 0x%02x\n", outCode[0]);
  printf("length %d\n", (int) len);
  for(int i =0; i < len; i++)
    printf(" %02x", outData[i] & 0xFF);
  printf("\n");
}
