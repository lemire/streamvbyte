#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>

#include "streamvbyte.h"

static inline uint64_t rdtsc(void) {
  uint64_t t = 0;
#ifdef __AVX__
    __asm__ volatile(".byte 0x0f, 0x31" : "=A"(t));
#endif
  return t;
}

typedef struct {
  struct rusage usg;
  uint64_t tsc;
} timepoint;

void gettime(timepoint *t) {
  t->tsc = rdtsc();
  getrusage(RUSAGE_SELF, &(t->usg));
}

void print_duration(timepoint *begin, timepoint *end, int NTotal, char *label) {
  float t = (end->usg.ru_utime.tv_sec - begin->usg.ru_utime.tv_sec) +
   (end->usg.ru_utime.tv_usec - begin->usg.ru_utime.tv_usec)/1000000.0;
 printf("%s time = %.4f  %.0f uints/sec\n", label, t, NTotal/t); 

 uint64_t cycles = end->tsc - begin->tsc;
 printf("%s cycles/quadword %0.2f\n", label, (4.0 * cycles) / NTotal);
}

int main() {
  int N = 500000;
  int NTrials = 200;
  uint32_t datain[N];
  uint8_t compressedbuffer[N * 5];
  uint32_t recovdata[N];

  for (int k = 0; k < N; ++k)
    datain[k] = rand() >> (31 & rand());

  size_t compsize = 0;

  timepoint encode_start, encode_end;
  gettime(&encode_start);

  for (int i = 0; i < NTrials; i++)
    compsize = streamvbyte_encode(datain, N, compressedbuffer); // encoding

  gettime(&encode_end);
  print_duration( &encode_start, &encode_end, N*NTrials, "encoding");

  size_t compsize2;

  timepoint decode_start, decode_end;
  gettime(&decode_start);

  for (int i = 0; i < NTrials; i++)
    compsize2 = streamvbyte_decode(compressedbuffer, recovdata,
                                        N); // decoding (fast)

  gettime(&decode_end);
  print_duration( &decode_start, &decode_end, N*NTrials, "decoding");

  printf("compsize=%zu compsize2 = %zu\n", compsize, compsize2);
  //  assert(compsize == compsize2);

  int k;
  for (k = 0; k < N && datain[k] == recovdata[k]; k++) 
    ;
 
  if(k < N)
    printf("mismatch at %d before=%d after=%d\n", k, datain[k], recovdata[k]);

  assert( k >= N );

  printf("Compressed %d integers down to %d bytes.\n", N, (int)compsize);
  return 0;
}
