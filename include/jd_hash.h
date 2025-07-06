/* date = April 21st 2024 10:25 pm */

#ifndef JD_HASH_H
#define JD_HASH_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_string.h"
#endif

u32 jd_HashU32toU32(u32 in, u32 seed);
u32 jd_HashStrToU32(jd_String input_str, u32 seed);
u32 jd_HashU64toU32(u64 val, u32 seed);

#ifdef JD_IMPLEMENTATION
#include "jd_hash.c"
#endif

#endif //JD_HASH_H
