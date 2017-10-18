// usage:  gcc -mfpu=neon -mfloat-abi=hard -std=c99 tst.c
// little-endian pi version
#include <arm_neon.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "streamvbyte_shuffle_tables.h"

int main() {

uint32_t maskval = 3;
uint32x4_t mask = vld1q_dup_u32(&maskval);

uint8_t pgatherlo[] = {12, 8, 4, 0, 12, 8, 4, 0};
uint8x8_t gatherlo = vld1_u8(pgatherlo);

#define concat (1 | 1 << 10 | 1 << 20 | 1 << 30)
// sum lane codes in high byte
#define sum (1 | 1 << 8 | 1 << 16 | 1 << 24)
uint32_t pAggregators[2] = {concat, sum};
uint32x2_t Aggregators = vld1_u32(pAggregators);

uint32_t pdata[4] = {0xabcdef, 0xfeeddad, 0xdeadbeef, 0xab };
uint32x4_t data = vld1q_u32(pdata);

// lane code is 3 ^ (clz(data)/8)
uint32x4_t clzbytes = vshrq_n_u32(vclzq_u32(data), 3);
uint32x4_t lanecodes = veorq_u32(clzbytes, mask);

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

uint8_t * pShuffle = (uint8_t *) &encodingShuffleTable[code];

uint8x8_t first8 = vld1_u8(pShuffle);

uint8_t out[16];
vst1_u8(out, vtbl2_u8(datahalves, first8));

if( length > 8 ) {
	uint8x8_t last8 = vld1_u8(pShuffle + 8);
	vst1_u8(out + 8, vtbl2_u8(datahalves, last8));
}

// decode
uint32_t dec[4];

// compressed data up to 16 bytes
uint8x16_t compressed = vld1q_u8(out);
uint8x8x2_t codehalves = {vget_low_u8(compressed), vget_high_u8(compressed)};

uint8_t *pShuf = (uint8_t *) &shuffleTable[code];

vst1_u8((uint8_t *) dec, vtbl2_u8(codehalves, vld1_u8(pShuf)));
vst1_u8((uint8_t *) (dec+2), vtbl2_u8(codehalves, vld1_u8(pShuf + 8)));

for( int i = 0; i < 4; i++ )
	printf("%x\n", dec[i]);

for( int i = 0; i < length; i++)
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
