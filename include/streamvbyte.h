
#ifndef VARINTDECODE_H_
#define VARINTDECODE_H_
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
size_t streamvbyte_encode(uint32_t *in, uint32_t length, uint8_t *out);

// return the maximum number of compressed bytes given length input integers
static size_t streamvbyte_max_compressedbytes(uint32_t length) {
   // number of control bytes:
   size_t cb = (length + 3) / 4;
   // maximum number of control bytes:
   size_t db = (size_t) length * sizeof(uint32_t);
   return cb + db;
}


// Read "length" 32-bit integers in varint format from in, storing the result in out.
// Returns the number of bytes read.
// The caller is responsible for knowing how many integers ("length") are to be read:
// this information ought to be stored somehow.
// There is no alignment requirement on the "in" pointer.
// The out pointer should point to length * sizeof(uint32_t) bytes.
size_t streamvbyte_decode(const uint8_t *in, uint32_t *out, uint32_t length);

#if defined(__cplusplus)
};
#endif

#endif /* VARINTDECODE_H_ */
