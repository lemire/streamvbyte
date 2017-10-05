
#include <x86intrin.h>
#include <stdio.h>

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


int main() {
  uint8_t out[16];
  uint32_t dataPtr[] = {100,400,100000, 1000};

  // use min to set bytes to 1 or 0 
  uint8_t Ones[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

  // bithack shift/or into high byte via multiply
#define shifter (1+(1<<9)+(1<<18))
  uint32_t Shifts[4] = {shifter,shifter,shifter,shifter};

  // translate 8 possible shift/ored bitmaps into lane codes
  // 3 - trailing zero count
  uint8_t Codes[16] = {0,3,2,3,1,3,2,3,0,0,0,0,0,0,0,0};

  __m128i ones   = *(__m128i *) Ones;
  __m128i shifts = *(__m128i *) Shifts;
  __m128i codes  = *(__m128i *) Codes;

  // data loop can begin here:
  __m128i Data = _mm_loadu_si128((__m128i *) dataPtr);
  dump(Data, "Data");

  __m128i mins = _mm_min_epu8(Data, ones);

  __m128i bytemaps = _mm_mullo_epi32( mins, shifts);

  __m128i lanecodes = _mm_shuffle_epi8(codes, bytemaps);

  dump(lanecodes, "lane code in high byte");

  // endianness note:  control byte uses high bits <=> dataPtr[4]
  // shove pairs of lanes together into lower 32
  __m128i paired = _mm_or_si128(lanecodes, _mm_srli_epi64(lanecodes, 30));

  // concat 4-bit chunks from high bytes
  _mm_storeu_si128((__m128i *) out, paired);
  uint32_t code = out[3] | (out[11] << 4);  // todo:  check indices

  printf("code 0x%02x\n", code);

  // get shuffle[code], length[code] (2 instr)
  // todo: shuffle table (inverse of decode)
  // shuffle (1)
  // store bytes (1)
  // store code (1)
  // outptr += length (1)

}
