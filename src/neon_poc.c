// usage:  gcc -mfpu=neon -mfloat-abi=hard -std=c99 tst.c
// little-endian pi version
#include <arm_neon.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "streamvbyte_shuffle_tables.h"

int main() {

uint32x4_t mask = {3,3,3,3};

uint8x8_t gatherlo = {12, 8, 4, 0, 12, 8, 4, 0};

#define concat (1 | 1 << 10 | 1 << 20 | 1 << 30)
// sum lane codes in high byte
#define sum (1 | 1 << 8 | 1 << 16 | 1 << 24)
uint32x2_t Aggregators = {concat, sum};

uint32x4_t data = {1, 0xFF, 0xdeadbeef, 0xab };


// lane code is 3 ^ (clz(data)/8)
uint32x4_t b = vshrq_n_u32(vclzq_u32(data), 3);
uint32x4_t lanecodes = veorq_u32(b, mask);

// nops
uint8x16_t lanebytes = vreinterpretq_u8_u32(lanecodes);
uint8x8x2_t twohalves = {vget_low_u8(lanebytes), vget_high_u8(lanebytes)};

// shuffle msbytes into two copies of an int
uint8x8_t lobytes = vtbl2_u8( twohalves, gatherlo);

uint32x2_t mulshift = vreinterpret_u32_u8(lobytes);

uint32x2_t codeAndLength = vmul_u32(mulshift, Aggregators);

uint32_t code = codeAndLength[0]>>24;
size_t length = 4+(codeAndLength[1]>>24) ;

// shuffle in 8-byte chunks
uint8x16_t databytes = vreinterpretq_u8_u32(data);
uint8x8x2_t datahalves = {vget_low_u8(databytes), vget_high_u8(databytes)};

uint8x8_t first8 = *(uint8x8_t *) &encodingShuffleTable[code];
uint8x8_t out = vtbl2_u8(datahalves, first8);

if( length > 8 ) {
	uint8x8_t last8 = *(uint8x8_t *) (8 + (void*)&encodingShuffleTable[code]);
	uint8x8_t out2 = vtbl2_u8(datahalves, last8);
}
for( int i = 0; i < 8; i++)
	printf("%2x ", out[i]);
printf("\n");
// shuffle(first 8 of mask)
// if( length > 8 )
//  shuffle(2nd half of mask)

for( int i =0; i < 4; i++)
	printf("lane code=%d\n", lanecodes[i] );

for( int i = 0; i < 8; i++)
	printf("lo = %x\n", lobytes[i]);

printf("code = %8x\n", code);
printf("length = %d\n", length);
}
