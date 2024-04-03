#ifndef _OS_H
#define _OS_H

// Subset of stdint.h for VS2008
#if defined(_MSC_VER) && (_MSC_VER < 1600) && (!defined(_STDINT)) && (!defined(_STDINT_H))
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
// Macros used in printf format string
#define PRId64 "lld"
#define PRIu64 "llu"
#else
#include <stdint.h>
#include <inttypes.h>
#endif

#endif