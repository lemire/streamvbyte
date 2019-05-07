#include "streamvbyte.h"
#include "streamvbyte_zigzag.h"
#include "streamvbytedelta.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool isLittleEndian() {
  int x = 1;
  char *c = (char *)&x;
  return (*c == 1);
}

// return -1 in case of failure
int zigzagtests() {
    size_t N = 4096;
    int32_t *datain = malloc(N * sizeof(int32_t));
    for(size_t i = 0; i < N; i++)
      datain[i] = rand() - rand();
    uint32_t *dataout = malloc(N * sizeof(uint32_t));
    int32_t *databack = malloc(N * sizeof(int32_t));
    zigzag_encode(datain, dataout, N);
    zigzag_decode(dataout, databack, N);
    int isok = 1;
    for(size_t i = 0; i < N; i++) {
      if(datain[i] != databack[i]) {
        printf("bug\n");
        isok = -1;
      }
    }
    free(databack);
    free(dataout);
    free(datain);
    return isok;
}

// return -1 in case of failure
int basictests() {
  int N = 4096;
  uint32_t *datain = malloc(N * sizeof(uint32_t));
  // on purpose we mess with the alignment of compressedbufferorig
  uint8_t *compressedbufferorig =
      malloc(streamvbyte_max_compressedbytes(N) + sizeof(uint32_t));
  uint8_t *compressedbuffer = compressedbufferorig + (sizeof(uint32_t) - 1);
  uint32_t *recovdata = malloc(N * sizeof(uint32_t));

  for (int length = 0; length <= N;) {
    for (uint32_t gap = 1; gap <= 387420489; gap *= 3) {
      for (int k = 0; k < length; ++k)
        datain[k] = gap - 1 + (rand() % 8); // sometimes start with zero

      // Default encoding: 1,2,3,4 bytes per value
      size_t compsize = streamvbyte_encode(datain, length, compressedbuffer);
      size_t usedbytes = streamvbyte_decode(compressedbuffer, recovdata, length);
      if (compsize != usedbytes) {
        printf("[streamvbyte_decode] code is buggy length=%d gap=%d: compsize=%d != "
               "usedbytes=%d \n",
               (int)length, (int)gap, (int)compsize, (int)usedbytes);
        return -1;
      }

      for (int k = 0; k < length; ++k) {
        if (recovdata[k] != datain[k]) {
          printf("[streamvbyte_decode] code is buggy gap=%d\n", (int)gap);
          return -1;
        }
      }

      // Alternative encoding: 0,1,2,4 bytes per value
      compsize = streamvbyte_encode_0124(datain, length, compressedbuffer);
      usedbytes = streamvbyte_decode_0124(compressedbuffer, recovdata, length);
      if (compsize != usedbytes) {
        printf("[streamvbyte_decode_0124] code is buggy length=%d gap=%d: compsize=%d != "
               "usedbytes=%d \n",
               (int)length, (int)gap, (int)compsize, (int)usedbytes);
        return -1;
      }

      for (int k = 0; k < length; ++k) {
        if (recovdata[k] != datain[k]) {
          printf("[streamvbyte_decode_0124] code is buggy gap=%d\n", (int)gap);
          return -1;
        }
      }
    }

    // Delta-encoded functions
    for (size_t gap = 1; gap <= 531441; gap *= 3) {
      for (int k = 0; k < length; ++k)
        datain[k] = gap * k;
      size_t compsize =
          streamvbyte_delta_encode(datain, length, compressedbuffer, 0);
      size_t usedbytes =
          streamvbyte_delta_decode(compressedbuffer, recovdata, length, 0);
      if (compsize != usedbytes) {
        printf("[streamvbyte_delta_decode] code is buggy gap=%d, size "
               "mismatch %d %d \n",
               (int)gap, (int)compsize, (int)usedbytes);
        return -1;
      }
      for (int k = 0; k < length; ++k) {
        if (recovdata[k] != datain[k]) {
          printf("[streamvbyte_delta_decode] code is buggy gap=%d\n",
                 (int)gap);
          return -1;
        }
      }
    }

    if (length < 128)
      ++length;
    else {
      length *= 2;
    }
  }
  free(datain);
  free(compressedbufferorig);
  free(recovdata);
  return 0;
}
// return -1 in case of failure
int aqrittests() {
  uint8_t in[16];
  uint8_t compressedbuffer[32];
  uint8_t recovdata[16];

  memset(compressedbuffer, 0, 32);
  memset(recovdata, 0, 16);

  for (int i = 0; i < 0x10000; i++) {
    in[0] = (uint8_t)((i >> 0) & 1);
    in[1] = (uint8_t)((i >> 1) & 1);
    in[2] = (uint8_t)((i >> 2) & 1);
    in[3] = (uint8_t)((i >> 3) & 1);
    in[4] = (uint8_t)((i >> 4) & 1);
    in[5] = (uint8_t)((i >> 5) & 1);
    in[6] = (uint8_t)((i >> 6) & 1);
    in[7] = (uint8_t)((i >> 7) & 1);
    in[8] = (uint8_t)((i >> 8) & 1);
    in[9] = (uint8_t)((i >> 9) & 1);
    in[10] = (uint8_t)((i >> 10) & 1);
    in[11] = (uint8_t)((i >> 11) & 1);
    in[12] = (uint8_t)((i >> 12) & 1);
    in[13] = (uint8_t)((i >> 13) & 1);
    in[14] = (uint8_t)((i >> 14) & 1);
    in[15] = (uint8_t)((i >> 15) & 1);
    const int length = 4;

    size_t compsize = streamvbyte_encode((uint32_t *)in, length, compressedbuffer);
    size_t usedbytes = streamvbyte_decode(compressedbuffer, (uint32_t *)recovdata, length);

    if (compsize != usedbytes) {
      printf("[streamvbyte_decode] code is buggy");
      return -1;
    }
    for (size_t k = 0; k < length * sizeof(uint32_t); ++k) {
      if (recovdata[k] != in[k]) {
        printf("[streamvbyte_decode] code is buggy");
        return -1;
      }
    }

    compsize = streamvbyte_encode_0124((uint32_t *)in, length, compressedbuffer);
    usedbytes = streamvbyte_decode_0124(compressedbuffer, (uint32_t *)recovdata, length);

    if (compsize != usedbytes) {
      printf("[streamvbyte_decode_0124] code is buggy");
      return -1;
    }
    for (size_t k = 0; k < length * sizeof(uint32_t); ++k) {
      if (recovdata[k] != in[k]) {
        printf("[streamvbyte_decode_0124] code is buggy");
        return -1;
      }
    }

  }
  return 0;
}

int main() {
  if(zigzagtests() == -1)
    return -1;
  if (basictests() == -1)
    return -1;
  if (aqrittests() == -1)
    return -1;
  printf("Code looks good.\n");
  if (isLittleEndian()) {
    printf("And you have a little endian architecture.\n");
  } else {
    printf("And you have a big endian architecture.\n");
    printf("Warning: produced compressed bytes may not be interoperable with "
           "little endian systems.\n");
  }
#ifdef __AVX__
  printf("Code was vectorized (x64).\n");
#elif defined(__ARM_NEON__)
  printf("Code was vectorized (ARM NEON).\n");
#else
  printf("Warning: you tested non-vectorized code.\n");
#endif
  return 0;
}
