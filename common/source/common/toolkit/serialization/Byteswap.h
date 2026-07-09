/*
 * Big- and little-endian byte swapping.
 */

#pragma once

#include "common/toolkit/UInt128.h"
#include <inttypes.h>

// byte order transformation macros for 16-, 32-, 64-, 128-bit types

inline uint16_t byteswap16(uint16_t u)
{
   uint16_t high = u >> 8;
   uint16_t low = u & 0xFF;

   return high | (low << 8);
}

inline uint32_t byteswap32(uint32_t u)
{
   uint32_t high = u >> 16;
   uint32_t low = u & 0xFFFF;

   return byteswap16(high) | (uint32_t(byteswap16(low) ) << 16);
}

inline uint64_t byteswap64(uint64_t u)
{
   uint64_t high = u >> 32;
   uint64_t low = u & 0xFFFFFFFF;

   return byteswap32(high) | (uint64_t(byteswap32(low) ) << 32);
}

inline uint128_t byteswap128(uint128_t u)
{
   return uint128::make(byteswap64(uint128::lower64(u)), byteswap64(uint128::upper64(u)));
}

#if BYTE_ORDER == BIG_ENDIAN // BIG_ENDIAN

   #define HOST_TO_LE_16(value)  (::byteswap16(value) )
   #define HOST_TO_LE_32(value)  (::byteswap32(value) )
   #define HOST_TO_LE_64(value)  (::byteswap64(value) )
   #define HOST_TO_LE_128(value) (::byteswap128(value) )
   #define HOST_TO_BE_16(value)  (value)
   #define HOST_TO_BE_32(value)  (value)
   #define HOST_TO_BE_64(value)  (value)
   #define HOST_TO_BE_128(value) (value)

#else // LITTLE_ENDIAN

   #define HOST_TO_LE_16(value)  (value)
   #define HOST_TO_LE_32(value)  (value)
   #define HOST_TO_LE_64(value)  (value)
   #define HOST_TO_LE_128(value) (value)
   #define HOST_TO_BE_16(value)  (::byteswap16(value) )
   #define HOST_TO_BE_32(value)  (::byteswap32(value) )
   #define HOST_TO_BE_64(value)  (::byteswap64(value) )
   #define HOST_TO_BE_128(value) (::byteswap128(value) )

#endif // BYTE_ORDER


#define LE_TO_HOST_16(value)    HOST_TO_LE_16(value)
#define LE_TO_HOST_32(value)    HOST_TO_LE_32(value)
#define LE_TO_HOST_64(value)    HOST_TO_LE_64(value)
#define LE_TO_HOST_128(value)   HOST_TO_LE_128(value)
#define BE_TO_HOST_16(value)    HOST_TO_BE_16(value)
#define BE_TO_HOST_32(value)    HOST_TO_BE_32(value)
#define BE_TO_HOST_64(value)    HOST_TO_BE_64(value)
#define BE_TO_HOST_128(value)   HOST_TO_BE_128(value)


