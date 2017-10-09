#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "streamvbyte.h"

int main() {
  int N = 5000;
  uint32_t *datain = malloc(N * sizeof(uint32_t));
  uint8_t *compressedbuffer = malloc(N * sizeof(uint32_t));
  uint32_t *recovdata = malloc(N * sizeof(uint32_t));
  for (int k = 0; k < N; ++k)
    datain[k] = k * 100;
  size_t compsize = streamvbyte_encode(datain, N, compressedbuffer); // encoding
  const char *filename = "data.bin";
  printf("I will write the data to %s \n", filename);
  FILE *f = fopen(filename, "w");
  size_t bw = fwrite(compressedbuffer, 1, compsize, f);
  fclose(f);
  if (bw != compsize) {
    printf("Tried to write %zu bytes, wrote %zu \n", compsize, bw);
  }

  f = fopen(filename, "r");
  for (size_t k = 0; k < N * sizeof(uint32_t); ++k)
    compressedbuffer[k] = 0;
  size_t br = fread(compressedbuffer, 1, compsize, f);
  if (br != compsize) {
    printf("Tried to read %zu bytes, wrote %zu \n", compsize, br);
  }
  // here the result is stored in compressedbuffer using compsize bytes
  size_t compsize2 = streamvbyte_decode(compressedbuffer, recovdata,
                                        N); // decoding (fast)
  assert(compsize == compsize2);
  free(datain);
  free(compressedbuffer);
  free(recovdata);
  printf("Compressed %d integers down to %d bytes.\n", N, (int)compsize);
  return 0;
}
