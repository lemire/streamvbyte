#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#define extract(c,i) (3 & (c >> 2*i))

void decoder_permutation(uint8_t *table, uint8_t *lengths ) {
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

void encoder_permutation(uint8_t *table, uint8_t *lengths ) {
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


void print_permutation(uint8_t *table) {
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

int main() {
  uint8_t *table = (uint8_t *) malloc( sizeof(uint8_t[256][16]));
  uint8_t lengths[256];
  encoder_permutation(table, lengths);
  print_permutation(table);
}

