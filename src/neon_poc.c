// usage:  gcc -mfpu=neon -mfloat-abi=hard -std=c99 tst.c
// little-endian pi version
#include <arm_neon.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main() {

uint32x4_t mask = {3,3,3,3};

uint8x8_t gatherlo = {12, 8, 4, 0, 12, 8, 4, 0};

#define concat (1 | 1 << 10 | 1 << 20 | 1 << 30)
// sum lane codes in high byte
#define sum (1 | 1 << 8 | 1 << 16 | 1 << 24)
uint32x2_t Aggregators = {concat, sum};

uint32x4_t data = {1,256,100000, 1024};

uint32x4_t b = vshrq_n_u32(vclzq_u32(data), 3);

uint32x4_t lanecodes = veorq_u32(b, mask);

uint8x16_t lanebytes = vreinterpretq_u8_u32(lanecodes);
uint8x8x2_t halves = {vget_low_u8(lanebytes), vget_high_u8(lanebytes)};

// shuffle msbytes into two copies of an int
uint8x8_t lobytes = vtbl2_u8( halves, gatherlo);

uint32x2_t mulshift = vreinterpret_u32_u8(lobytes);

uint32x2_t codeAndLength = vmul_u32(mulshift, Aggregators);

uint32_t code = codeAndLength[0]>>24;
size_t length = 4+(codeAndLength[1]>>24) ;

for( int i =0; i < 4; i++)
	printf("lane code=%d\n", lanecodes[i] );

for( int i = 0; i < 8; i++)
	printf("lo = %x\n", lobytes[i]);

printf("code = %8x\n", code);
printf("length = %d\n", length);
}
