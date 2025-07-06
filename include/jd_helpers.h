/* date = February 19th 2024 7:14 pm */

#ifndef JD_HELPERS_H
#define JD_HELPERS_H

#ifndef JD_DEFS_H
#include "jd_defs.h"
#endif

#define KILOBYTES(x) (x * 1024)
#define MEGABYTES(x) (KILOBYTES(x) * 1024)
#define GIGABYTES(x) (MEGABYTES(x) * 1024ull)

#define IN_KB(x) ((f64)(x / 1024.0f))
#define IN_MB(x) ((f64)(IN_KB(x) / 1024.0f))
#define IN_GB(x) ((f64)(IN_MB(x) / 1024.0f))

#define jd_ArrayCount(x) (sizeof(x)/sizeof(*(x)))
#define jd_Min(a, b) ( ((a) < (b)) ? (a):(b) )
#define jd_Max(a, b) ( ((a) > (b)) ? (a):(b) )

#define jd_ClampToRange(val, min, max) (((val) < (min)) ? (min) : ((max) < (val)) ? (max) : (val))

#define jd_AlignUpPow2(x, p) (((x) + (p) - 1)&~((p) - 1))
#define jd_BitIsSet(field, val) (field & (1 << val))

#define jd_GetHiWordU64(x) (x >> 32)
#define jd_GetLoWordU64(x) (x & 0xFFFFFFFF)

#endif //JD_HELPERS_H
