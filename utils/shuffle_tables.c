#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define extract(c,i) (3 & (c >> 2*i))

typedef uint8_t (*code_len_function)(int c);

static uint8_t code_to_length(int c) {
  /* 0,1,2,3 codes-> 1,2,3,4 bytes */
  return c + 1;
}

static uint8_t code_to_length_0124(int c) {
  switch (c) {
  case 0: // 0 bytes
    return 0;
  case 1: // 1 byte
    return 1;
  case 2: // 2 bytes
    return 2;
  default: // 4 bytes
    return 4;
  }
}

//  Initializes the lengths tables. Meant to be called before
//  decoder_permutation/encoder_permutation functions.
static void lengths_init(uint8_t *lengths, code_len_function code_len) {
  for(int code = 0; code < 256; code++) {
    lengths[code] = 0;
    for(int i = 0; i < 4; i++ ) {
      int c = extract(code, i);
      lengths[code] += code_len(c);
    }
  }
}

// produces the decoder permutation tables
// as well as the "length" tables. The length table
// is the same for encoding and decoding.
// table should point at 256*16 bytes
// length should point at 256 bytes
static void decoder_permutation(uint8_t *table) {
  uint8_t *p = table;
  for(int code = 0; code < 256; code++) {
    int byte = 0;
    for(int i = 0; i < 4; i++ ) {
      int c = extract(code, i);
      int j;
      for( j = 0; j <= c; j++ )
        *p++ = byte++;
      for( ; j < 4; j++ )
        *p++ = -1;
    }
  }
}

// produces the encoder permutation tables
// table should point at 256*16 bytes
// length should point at 256 bytes initialized with init_lengths
static void encoder_permutation(uint8_t *table, uint8_t *lengths) {
  uint8_t *p = table;
  for(int code = 0; code < 256; code++) {
    for(int i = 0; i < 4; i++ ) {
      int c = extract(code, i);
      int j;
      for( j = 0; j <= c; j++ )
        *p++ = 4*i + j;
    }
    for( int i = lengths[code]; i < 16; i++ )
      *p++ = -1;
  }
}

// produces the decoder permutation tables
// table should point at 256*16 bytes
static void decoder_permutation_0124(uint8_t *table) {
  uint8_t *p = table;
  for(int code = 0; code < 256; code++) {
    int byte = 0;
    for(int i = 0; i < 4; i++ ) {
      int c = extract(code, i);
      if (c < 3) {
        // Here c stands for a number of bytes to copy, i.e. 0,1,2
        int j;
        for( j = 0; j < c; j++ )
          *p++ = byte++;
        for( ; j < 4; j++ )
          *p++ = -1;
      } else {
        // Otherwise always copy all 4 bytes
        for(int j = 0; j < 4; j++ )
          *p++ = byte++;
      }
    }
  }
}

// produces the encoder permutation tables
// table should point at 256*16 bytes
// length should point at 256 bytes initialized
// with lengths_init
static void encoder_permutation_0124(uint8_t *table, uint8_t *lengths) {
  uint8_t *p = table;
  for(int code = 0; code < 256; code++) {
    for(int i = 0; i < 4; i++ ) {
      int c = extract(code, i);
      if (c < 3) {
        for( int j = 1; j <= c; j++ )
          *p++ = 4*i + j - 1;
      } else {
        for( int j = 0; j <= c; j++ )
          *p++ = 4*i + j;
      }
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
           extract(code,0),
           extract(code,1),
           extract(code,2),
           extract(code,3));
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

int main(int argc, char **argv) {
  uint8_t *encoder_table = (uint8_t *) malloc( sizeof(uint8_t[256][16]));
  uint8_t *decoder_table = (uint8_t *) malloc( sizeof(uint8_t[256][16]));
  uint8_t lengths[256];

  if (argc == 2 && 0 == strcmp(argv[1], "0124")) {
    printf("// using 0,1,2,4 bytes per value\n");
    lengths_init(lengths, code_to_length_0124);
    encoder_permutation_0124(encoder_table, lengths);
    decoder_permutation_0124(decoder_table);
  } else if (argc == 2 && 0 == strcmp(argv[1], "1234")) {
    printf("// using 1,2,3,4 bytes per value\n");
    lengths_init(lengths, code_to_length);
    encoder_permutation(encoder_table, lengths);
    decoder_permutation(decoder_table);
  } else {
    fprintf(stderr, "Usage: shuffle_tables <0124|1234> > src/streamvbyte_shuffle_tables.h\n");
    exit(EXIT_FAILURE);
  }

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
