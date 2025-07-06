/* date = February 19th 2024 8:39 pm */

#ifndef JD_MATH_H
#define JD_MATH_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#endif

u64 jd_Pow_u64(u64 b, u64 e);
#define jd_Abs(x) (x < 0) ? x * -1 : x

jd_V4F jd_V4FMul4(jd_V4F a, jd_V4F b);

#ifdef JD_IMPLEMENTATION
#include "jd_math.c"
#endif // JD_IMPLEMENTATION

#endif //JD_MATH_H
