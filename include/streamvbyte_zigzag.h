
#ifndef INCLUDE_STREAMVBYTE_ZIGZAG_H_
#define INCLUDE_STREAMVBYTE_ZIGZAG_H_
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>// please use a C99-compatible compiler
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif


/**
 * Convert N signed integers to N unsigned integers, using zigzag
 * encoding.
 */
void zigzag_encode(const int32_t * in, uint32_t * out, size_t N);

/**
 * Convert N unsigned integers to N signed integers, using zigzag
 * encoding.
 */
void zigzag_decode(const uint32_t * in, int32_t * out, size_t N);


#if defined(__cplusplus)
};
#endif

#endif /* INCLUDE_STREAMVBYTE_ZIGZAG_H_ */
