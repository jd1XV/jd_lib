/* date = March 11th 2024 8:27 pm */

#ifndef JD_UNICODE_H
#define JD_UNICODE_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_string.h"
#endif

// TODO: Consider merging with jd_string.h/.c

typedef enum jd_UnicodeTF {
    jd_UnicodeTF_UTF8,
    jd_UnicodeTF_UTF16,
    jd_UnicodeTF_UTF32,
    jd_UnicodeTF_Count
} jd_UnicodeTF;

typedef struct jd_UTFDecodedString {
    jd_UnicodeTF tf;
    
    union {
        u8*  utf8;
        u16* utf16;
        u32* utf32;
    };
    
    u64 count;
} jd_UTFDecodedString;

typedef struct jd_String32 {
    u32* mem;
    u64  count;
} jd_String32;

typedef struct jd_String16 {
    u16* mem;
    u64  count;
} jd_String16;

jd_ExportFn u32 jd_Codepoint8to32(jd_String string8, u64* index);
jd_ExportFn jd_String16 jd_UTF8ToUTF16(jd_Arena* arena, jd_String string);
jd_ExportFn jd_String32 jd_UTF8ToUTF32(jd_Arena* arena, jd_String string);
jd_ExportFn jd_String   jd_UTF16toUTF8(jd_Arena* arena, jd_String16 string);
jd_ExportFn jd_String   jd_UTF32toUTF8(jd_Arena* arena, jd_String32 string);

// 0xxxxxxx
// 110xxxxx 10xxxxxx
// 1110xxxx 10xxxxxx 10xxxxxx
// 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

#define _jd_PackU64_u8(a,b,c,d, e,f,g,h) \
(a << 56) + (b << 48) + (c << 40) + (d << 32) + (e << 24) + (f << 16) + (g << 8) + h


jd_UTFDecodedString jd_UnicodeDecodeUTF8String(jd_Arena* arena, jd_UnicodeTF tf, jd_String input, b32 validate);
jd_String jd_UnicodeEncodeUTF32toUTF8(jd_Arena* arena, jd_UTFDecodedString input, b32 validate);
jd_ForceInline u32 jd_UnicodeDecodeUTF8Codepoint(jd_String string, u64 index);
u32 jd_UnicodeDecodeUTF16Codepoint(u16* buf, u32 count);
u32 jd_UnicodeUTF16toUTF8(u16 wide[2], u32 wide_count, c8* buffer, u32 buf_size);
u32 jd_UnicodeUTF32toUTF8(u32 codepoint, c8* buffer, u32 buf_size);
void jd_UnicodeSeekDeltaUTF8(jd_String string, u64* index, i64 delta);

#ifdef JD_IMPLEMENTATION
#include "jd_unicode.c"
#endif

#endif //JD_UNICODE_H
