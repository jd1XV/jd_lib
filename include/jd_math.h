/* date = February 19th 2024 8:39 pm */

#ifndef JD_MATH_H
#define JD_MATH_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#endif

u64 jd_Pow_u64(u64 b, u64 e);

jd_ExportFn u64    jd_I64Abs(i64 x);
jd_ExportFn f32    jd_F32Abs(f32 x);
jd_ExportFn jd_V4F jd_V4FMul4(jd_V4F a, jd_V4F b);
jd_ExportFn jd_V4F jd_V4FMulS(jd_V4F a, f32 b);
jd_ExportFn jd_V4F jd_V4FMul3S(jd_V4F a, f32 b);
jd_ExportFn jd_V4F jd_V4FColorLerp(jd_V4F a, jd_V4F b, f32 frac);

#ifdef JD_IMPLEMENTATION
#include "jd_math.c"
#endif // JD_IMPLEMENTATION

#endif //JD_MATH_H
