/* date = February 22nd 2024 11:46 pm */

#ifndef JD_STRING_H
#define JD_STRING_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_memory.h"
#endif 

typedef jd_View jd_String;

typedef struct jd_DString {
    jd_Arena* arena;
    u8* mem;
    u64 count;
} jd_DString;

jd_ExportFn jd_String jd_StrLit(c8* c_str);
#define jd_StrConst(c_str) {c_str, sizeof(c_str) - 1}
jd_ExportFn jd_String jd_StringPush(jd_Arena* arena, jd_String str);
jd_ExportFn jd_String jd_StringPushF(jd_Arena* arena, jd_String fmt_string, ...);
jd_ExportFn jd_String jd_StringPushVAList(jd_Arena* arena, jd_String fmt_string, va_list list);

jd_ExportFn jd_String jd_StringGetPrefix(jd_String str, jd_String pattern);
jd_ExportFn jd_String jd_StringGetPostfix(jd_String str, jd_String pattern);

jd_ExportFn b32 jd_StringMatch(jd_String a, jd_String b);
jd_ExportFn b32 jd_StringContainsSubstring(jd_String string, jd_String substring);
jd_ExportFn b32 jd_StringBeginsWith(jd_String string, jd_String substring);

#define jd_DStringGet(x) (jd_String){x->mem, x->count}

jd_ExportFn jd_DString* jd_DStringCreate(u64 capacity);
jd_ExportFn void jd_DStringRelease(jd_DString* d_string);

jd_ExportFn void jd_DStringClear(jd_DString* d_string);
jd_ExportFn void jd_DStringAppend(jd_DString* d_string, jd_String app);
jd_ExportFn void jd_DStringAppendF(jd_DString* d_string, jd_String app_fmt, ...);
jd_ExportFn void jd_DStringAppendC8(jd_DString* d_string, c8 app);
jd_ExportFn void jd_DStringAppendU8(jd_DString* d_string, u8 num, u32 radix);
jd_ExportFn void jd_DStringAppendU32(jd_DString* d_string, u32 num, u32 radix);
jd_ExportFn void jd_DStringAppendI32(jd_DString* d_string, i32 num, u32 radix);
jd_ExportFn void jd_DStringAppendU64(jd_DString* d_string, u64 num, u32 radix);
jd_ExportFn void jd_DStringAppendI64(jd_DString* d_string, i64 num, u32 radix);
jd_ExportFn void jd_DStringAppendBin(jd_DString* d_string, u64 size, void* ptr);

#ifdef JD_IMPLEMENTATION
#include "jd_string.c"
#endif

#endif //JD_STRING_H
