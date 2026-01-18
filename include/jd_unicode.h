/* date = March 11th 2024 8:27 pm */

#ifndef JD_UNICODE_H
#define JD_UNICODE_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_string.h"
#endif

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
