#include "streamvbyte.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>

#include <string.h>

void punt(long long n, char *s) {
  int i = 127;
  int sign = 0;
  if (n < 0) {
    sign = 1;
    n = -n;
  }
  s[i--] = '\0'; // null terminated
  int digits = 0;
  do {
    s[i--] = n % 10 + '0';
    digits++;
    n /= 10;
    if (((digits % 3) == 0) && (n != 0))
      s[i--] = ',';

  } while (n);
  if (sign)
    s[i--] = '-';
  memmove(s, s + i + 1, 127 - i);
}

int main() {
  int N = 500000;
  int NTrials = 100;
  struct rusage before;
  struct rusage after;
  float t;
  char s[128];
  char s1[128];
  char s2[128];

  uint32_t datain[N];
  uint8_t compressedbuffer[N * 5];
  uint32_t recovdata[N];

  for (int k = 0; k < N; ++k)
    datain[k] = rand() >> (31 & rand());

  size_t compsize = 0;

  getrusage(RUSAGE_SELF, &before);

  for (int i = 0; i < NTrials; i++)
    compsize = streamvbyte_encode(datain, N, compressedbuffer);

  getrusage(RUSAGE_SELF, &after);

  t = (after.ru_utime.tv_usec - before.ru_utime.tv_usec) / 1000000.0;
  punt((long long)round(N * NTrials / t), s);
  printf("encoding time = %f s,   %s uints/sec\n", t, s);

  size_t compsize2;
  getrusage(RUSAGE_SELF, &before);
  for (int i = 0; i < NTrials; i++)
    compsize2 = streamvbyte_decode(compressedbuffer, recovdata, N);
  getrusage(RUSAGE_SELF, &after);
  t = (after.ru_utime.tv_usec - before.ru_utime.tv_usec) / 1000000.0;
  punt((long long)round(N * NTrials / t), s);
  printf("decoding time = %f s,   %s uints/sec\n", t, s);
  if (compsize != compsize2)
    printf("compsize=%zu compsize2 = %zu\n", compsize, compsize2);

  int k;
  for (k = 0; k < N && datain[k] == recovdata[k]; k++)
    ;

  if (k < N)
    printf("mismatch at %d before=%d after=%d\n", k, datain[k], recovdata[k]);

  assert(k >= N);
  punt(N * sizeof(uint32_t), s1);
  punt(compsize, s2);
  printf("Compressed %s bytes down to %s bytes.\n", s1, s2);
  return 0;
}
