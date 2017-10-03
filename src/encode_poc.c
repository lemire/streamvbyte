
#include <x86intrin.h>
#include <stdio.h>

// POC standalone test until code looks ok
// usage:  gcc -o encode_poc encode_poc.c

void dump(__m128i x, char *msg) {
  uint32_t out[4];
  _mm_storeu_si128((__m128i *) out, x);

  printf("%s ", msg);
  for( int i=0; i<4; i++)
    printf("%d,", out[i]);
  printf("\n");
}


int main() {
  uint32_t out[4];
  uint32_t dataPtr[] = {100,400,100000, 1000};

  // constants
  __m128i three = _mm_set_epi32(3,3,3,3);
  __m128i signs = _mm_set_epi32(1<<31,1<<31,1<<31,1<<31);

  // thresholds pre-adjusted for signed cmp
  #define u8  1<<31 | 1<<8
  #define u16 1<<31 | 1<<16
  #define u24 1<<31 | 1<<24

  // comparison threshold vectors
  __m128i b1 = _mm_set_epi32( u8, u8, u8, u8);
  __m128i b2 = _mm_set_epi32(u16,u16,u16,u16);
  __m128i b3 = _mm_set_epi32(u24,u24,u24,u24);

  // data loop can begin here:
  __m128i Data = _mm_loadu_si128((__m128i *) dataPtr);

  dump(Data, "Data");

  // adjust for signed cmp
  __m128i cmpData = _mm_xor_si128(Data, signs);

  // cmp to thresholds and add up result bitmasks (-1 or 0)
  __m128i r = 
    _mm_add_epi32(
		  _mm_add_epi32(
				_mm_cmpgt_epi32(b1, cmpData),
				_mm_cmpgt_epi32(b2, cmpData)),
		  _mm_cmpgt_epi32(b3, cmpData));

  // get 3 - #thresholds gt num
  r = _mm_add_epi32(r, three);

  // 2 bits of code are now in each lane
  dump(r, "Codes");

  // endianness note:  control byte uses high bits <=> dataPtr[4]

  // shove pairs of lanes together
  r = _mm_or_si128(r, _mm_srli_epi64(r, 30));

  // concat 4-bit chunks
  _mm_storeu_si128((__m128i *) out, r);
  uint32_t code = out[0] | (out[2] << 4);

  printf("code 0x%02x\n", code);

  // get shuffle[code], length[code] (2 instr)
  // todo: shuffle table (inverse of decode)
  // shuffle (1)
  // store bytes (1)
  // store code (1)
  // outptr += length (1)

}
