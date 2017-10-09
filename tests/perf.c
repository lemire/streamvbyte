#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "streamvbyte.h"
static inline void rdtsc(unsigned long long *destination) {
  uint64_t t;
  __asm__ volatile(".byte 0x0f, 0x31" : "=A"(t));
  *destination = t;
}

int main() {
  int N = 500000;
  uint32_t *datain = malloc(N * sizeof(uint32_t));
  uint8_t *compressedbuffer = malloc(N * sizeof(uint32_t));
  uint32_t *recovdata = malloc(N * sizeof(uint32_t));
  for (int k = 0; k < N; ++k)
    datain[k] = rand() >> (31 & rand());

  size_t compsize = 0;
  uint64_t tsc, tsc2;
  rdtsc(&tsc);
  for (int i = 0; i < 200; i++)
    compsize = streamvbyte_encode(datain, N, compressedbuffer); // encoding

  rdtsc(&tsc2);
  tsc2 -= tsc;
  printf("cycles/quadword %llu\n", (4 * tsc2) / (N * 200));
  // here the result is stored in compressedbuffer using compsize bytes
  size_t compsize2 = streamvbyte_decode(compressedbuffer, recovdata,
                                        N); // decoding (fast)
  assert(compsize == compsize2);

  for (int k = 0; k < N; k++)
    assert(datain[k] == recovdata[k]);

  free(datain);
  free(compressedbuffer);
  free(recovdata);
  printf("Compressed %d integers down to %d bytes.\n", N, (int)compsize);
  return 0;
}
