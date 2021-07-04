
#ifndef INCLUDE_STREAMVBYTE_H_
#define INCLUDE_STREAMVBYTE_H_
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>// please use a C99-compatible compiler
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

// Encode an array of a given length read from in to bout in varint format.
// Returns the number of bytes written.
// The number of values being stored (length) is not encoded in the compressed stream,
// the caller is responsible for keeping a record of this length.
// The pointer "in" should point to "length" values of size uint32_t
// there is no alignment requirement on the out pointer
// For safety, the out pointer should point to at least streamvbyte_max_compressedbyte(length)
// bytes.
// Uses 1,2,3 or 4 bytes per value + the decoding keys.
size_t streamvbyte_encode(const uint32_t *in, uint32_t length, uint8_t *out);

// same as streamvbyte_encode but 0,1,2 or 4 bytes per value (plus decoding keys) instead of using 1,2,3 or 4
// bytes. This might be useful when there's a lot of zeroes in the input array.
size_t streamvbyte_encode_0124(const uint32_t *in, uint32_t length, uint8_t *out);

// return the maximum number of compressed bytes given length input integers
// in the worst case we overestimate data bytes required by four, see below
// for a function you can run upfront over your data to compute allocations
static inline size_t streamvbyte_max_compressedbytes(const uint32_t length) {
   // number of control bytes:
   size_t cb = (length + 3) / 4;
   // maximum number of control bytes:
   size_t db = (size_t) length * sizeof(uint32_t);
   return cb + db;
}

// return the exact number of compressed bytes given length input integers
// runtime in O(n) wrt. in; use streamvbyte_max_compressedbyte if you
// care about speed more than potentially over-allocating memory
static inline size_t streamvbyte_compressedbytes(const uint32_t *in, uint32_t length) {
   // number of control bytes:
   size_t cb = (length + 3) / 4;
   // maximum number of control bytes:
   size_t db = 0;
   for (uint32_t c = 0; c < length; c++) {
      uint32_t val = in[c];

      if (val < (1 << 8)) db += 1;
      else if (val < (1 << 16)) db += 2;
      else if (val < (1 << 24)) db += 3;
      else db += 4;
   }
   return cb + db;
}


// Read "length" 32-bit integers in varint format from in, storing the result in out.
// Returns the number of bytes read.
// The caller is responsible for knowing how many integers ("length") are to be read:
// this information ought to be stored somehow.
// There is no alignment requirement on the "in" pointer.
// The out pointer should point to length * sizeof(uint32_t) bytes.
size_t streamvbyte_decode(const uint8_t *in, uint32_t *out, uint32_t length);

// Same as streamvbyte_decode but is meant to be used for streams encoded with
// streamvbyte_encode_0124.
size_t streamvbyte_decode_0124(const uint8_t *in, uint32_t *out, uint32_t length);

#if defined(__cplusplus)
};
#endif

#endif /* INCLUDE_STREAMVBYTE_H_ */
