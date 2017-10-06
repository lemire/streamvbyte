#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#define extract(c,i) (3 & (c >> 2*i))

// produces the decoder permutation tables
// as well as the "length" tables. The length table
// is the same for encoding and decoding.
// table should point at 256*16 bytes
// length should point at 256 bytes
static void decoder_permutation(uint8_t *table, uint8_t *lengths) {
  uint8_t *p = table;
  for(int code = 0; code < 256; code++) {
    lengths[code] = 0;
    int byte = 0;
    for(int i = 0; i < 4; i++ ) {
      int c = extract(code, i);
      lengths[code] += c+1;
      int j;
      for( j = 0; j <= c; j++ )
        *p++ = byte++;
      for( ; j < 4; j++ )
        *p++ = -1;
    }
  }
}

// produces the encoder permutation tables
// as well as the "length" tables. The length table
// is the same for encoding and decoding.
// table should point at 256*16 bytes
// length should point at 256 bytes
static void encoder_permutation(uint8_t *table, uint8_t *lengths ) {
  uint8_t *p = table;
  for(int code = 0; code < 256; code++) {
    lengths[code] = 0;
    for(int i = 0; i < 4; i++ ) {
      int c = extract(code, i);
      lengths[code] += c+1;
      int j;
      for( j = 0; j <= c; j++ )
        *p++ = 4*i + j;
    }
    for( int i = lengths[code]; i < 16; i++ )
      *p++ = -1;
  }
}

// to be used after calling either  decoder_permutation or encoder_permutation
// table should point at 256*16 bytes
static void print_permutation(uint8_t *table) {
  for(int code = 0; code < 256; code++) {
    int x;
    printf(" {");
    for(int i = 0; i < 15; i++)
      printf(" %2d,", x = (int8_t) table[code*16 + i]);
    printf( " %2d", x = (int8_t) table[code*16 + 15]);
    printf(" },    // %d%d%d%d\n",
           extract(code,0)+1,
           extract(code,1)+1,
           extract(code,2)+1,
           extract(code,3)+1);
  }
}

// to be used after calling either  decoder_permutation or encoder_permutation
// length should point at 256 bytes
static void print_lengths(uint8_t *length) {
  printf("{");
  printf("\n");
  for(int code = 0; code < 256; ) {
    for(int k = 0; k < 16 ; k++) {
      printf(" %2d,", length[code]);
      code++;
    }
    printf("\n");
  }
  printf(" }");
}

int main() {
  uint8_t *encoder_table = (uint8_t *) malloc( sizeof(uint8_t[256][16]));
  uint8_t *decoder_table = (uint8_t *) malloc( sizeof(uint8_t[256][16]));
  uint8_t lengths[256];
  encoder_permutation(encoder_table, lengths);
  decoder_permutation(decoder_table, lengths);

  printf("static uint8_t lengthTable[256] =");
  print_lengths(lengths);
  printf(";\n\n");

  printf("// decoding:\n");
  printf("static uint8_t shuffleTable[256][16] = {\n");
  print_permutation(decoder_table);
  printf("};\n\n");

  printf("// encoding:\n");
  printf("static uint8_t encodingShuffleTable[256][16] = {\n");
  print_permutation(encoder_table);
  printf("};\n");
  return 0;

}
