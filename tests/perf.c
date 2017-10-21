#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "streamvbyte.h"
//static inline void rdtsc(unsigned long long *destination) {
//  uint64_t t;
  //  __asm__ volatile(".byte 0x0f, 0x31" : "=A"(t));
//  *destination = 0;
//}

int main() {
  int N = 500000;
  uint32_t datain[N];
  uint8_t compressedbuffer[N * 5];
  uint32_t recovdata[N];

  for (int k = 0; k < N; ++k)
    datain[k] = rand() >> (31 & rand());

  size_t compsize = 0;

  //  uint64_t tsc, tsc2;
  //  rdtsc(&tsc);
  for (int i = 0; i < 2; i++)
    compsize = streamvbyte_encode(datain, N, compressedbuffer); // encoding

  //  rdtsc(&tsc2);
  //tsc2 -= tsc;
  //printf("cycles/quadword %llu\n", (4 * tsc2) / (N * 200));
  // here the result is stored in compressedbuffer using compsize bytes
  size_t compsize2 = streamvbyte_decode(compressedbuffer, recovdata,
                                        N); // decoding (fast)

  printf("compsize=%d compsize2 = %d\n", compsize, compsize2);
  //  assert(compsize == compsize2);

  int k;
  for (k = 0; k < N && datain[k] == recovdata[k]; k++) 
    ;
 
  if(k < N)
    printf("mismatch at %d before=%d after=%d\n", k, datain[k], recovdata[k]);

  assert( k >= N );

  //  free(datain);
  //free(compressedbuffer);
  //free(recovdata);
  printf("Compressed %d integers down to %d bytes.\n", N, (int)compsize);
  return 0;
}
