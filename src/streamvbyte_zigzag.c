#include "streamvbyte_zigzag.h"

static inline
uint32_t _zigzag_encode_32 (int32_t val) {
	return (val + val) ^ (val >> 31);
}

void zigzag_encode(const int32_t * in, uint32_t * out, size_t N) {
    for(size_t i = 0; i < N; i++)
      out[i] = _zigzag_encode_32(in[i]);
}


static inline
int32_t _zigzag_decode_32 (uint32_t val) {
	return (val >> 1) ^ -(val & 1);
}


void zigzag_decode(const uint32_t * in, int32_t * out, size_t N) {
    for(size_t i = 0; i < N; i++)
      out[i] = _zigzag_decode_32(in[i]);
}
