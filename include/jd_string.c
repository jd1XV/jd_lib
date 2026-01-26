#define STB_SPRINTF_DECORATE(name) stb_##name
#define STB_SPRINTF_IMPLEMENTATION
#include "dep/stb/stb_sprintf.h"

jd_String jd_StrLit(c8* c_str) {
    jd_String str = {0};
    str.mem = c_str;
    while (*c_str != 0) {
        c_str++;
        str.count++;
    }
    
    return str;
}

jd_String jd_StringPush(jd_Arena* arena, jd_String str) {
    jd_String string = {
        .mem = jd_ArenaAlloc(arena, str.count + 1), // + 1 so our strings will be null-terminated by default
        .count = str.count
    };
    
    jd_MemCpy(string.mem, str.mem, str.count);
    return string;
}

jd_String jd_StringPushF(jd_Arena* arena, jd_String fmt_string, ...) {
    va_list list = {0};
    va_start(list, fmt_string);
    i32 length = stb_vsnprintf(0, 0, fmt_string.mem, list);
    jd_String string = {0};
    string.count = length;
    string.mem = jd_ArenaAlloc(arena, string.count + 1);
    stb_vsnprintf(string.mem, string.count + 1, fmt_string.mem, list);
    va_end(list);
    return string;
}

jd_String jd_StringPushVAList(jd_Arena* arena, jd_String fmt_string, va_list list) {
    i32 length = stb_vsnprintf(0, 0, fmt_string.mem, list);
    jd_String string = {0};
    string.count = length;
    string.mem = jd_ArenaAlloc(arena, string.count + 1);
    stb_vsnprintf(string.mem, string.count + 1, fmt_string.mem, list);
    return string;
}

jd_String jd_StringGetPrefix(jd_String str, jd_String pattern) {
    jd_String s = str;
    for (u64 i = 0; i + pattern.count < str.count; i++) {
        if (jd_MemCmp(&str.mem[i], pattern.mem, pattern.count)) {
            s.count = i;
            return s;
        }
    }
    
    return s;
}

jd_String jd_StringGetPostfix(jd_String str, jd_String pattern) {
    jd_String s = str;
    for (u64 i = 0; i + pattern.count < str.count; i++) {
        if (jd_MemCmp(&str.mem[i], pattern.mem, pattern.count)) {
            s.mem = &str.mem[i];
            s.count = str.count - i;
            return s;
        }
    }
    
    return s;
}

b32 jd_StringMatch(jd_String a, jd_String b) {
    if (a.count != b.count) return false;
    return jd_MemCmp(a.mem, b.mem, a.count);
}

b32 jd_StringContainsSubstring(jd_String string, jd_String substring) {
    if (substring.count > string.count) return false;
    
    for (u64 i = 0; i < string.count; i++) {
        if (i + substring.count > string.count) {
            return false;
        }
        
        b32 cmp = jd_MemCmp(string.mem + i, substring.mem, substring.count);
        if (cmp) return true;
    }
    
    return false;
}

b32 jd_StringBeginsWith(jd_String string, jd_String substring) {
    if (substring.count > string.count) return false;
    
    for (u32 i = 0; i < substring.count; i++) {
        if (string.mem[i] != substring.mem[i]) {
            return false;
        }
    }
    
    return true;
}

jd_ForceInline jd_DString* jd_DStringCreate(u64 capacity) {
    jd_Arena* arena = jd_ArenaCreate(capacity, 0);
    jd_DString* d_string = jd_ArenaAlloc(arena, sizeof(*d_string));
    d_string->arena = arena;
    d_string->mem = arena->mem + arena->pos;
    d_string->count = 0;
    return d_string;
}

void jd_DStringClear(jd_DString* d_string) {
    jd_ArenaPopTo(d_string->arena, 0 + sizeof(*d_string));
    d_string->count = 0;
}

void jd_DStringAppend(jd_DString* d_string, jd_String app) {
    u8* ptr = jd_ArenaAlloc(d_string->arena, app.count);
    d_string->count += app.count;
    jd_MemCpy(ptr, app.mem, app.count);
}

void jd_DStringAppendF(jd_DString* d_string, jd_String app_fmt, ...) {
    va_list list = {0};
    va_start(list, app_fmt);
    jd_String str = jd_StringPushVAList(d_string->arena, app_fmt, list);
    d_string->count += str.count;
    d_string->arena->pos -= 1;  // little hack to cancel out stb_vsnprintf 0-termination
    va_end(list);
}

void jd_DStringAppendC8(jd_DString* d_string, c8 app) {
    c8* ptr = jd_ArenaAlloc(d_string->arena, sizeof(app));
    d_string->mem[d_string->count] = app;
    d_string->count++;
}

void jd_DStringRelease(jd_DString* d_string) {
    jd_ArenaRelease(d_string->arena);
}

static const c8 _jd_digit_table[36] =  {
    '0', '1',' 2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
};

void jd_DStringAppendU8(jd_DString* d_string, u8 num, u32 radix) {
    u64 places = 0; 
    for (u64 quot = num; quot > 0; quot /= radix) {
        places++;
    }
    
    u8* ptr = jd_ArenaAlloc(d_string->arena, places);
    d_string->count += places;
    
    for (u64 i = 0; i < places; i++) {
        if (i + 1 == places) {
            ptr[i] = _jd_digit_table[num];
            break;
        }
        
        u64 sub = jd_Pow_u64(radix, (places - i) - 1);
        u64 digit = num / sub;
        ptr[i] = _jd_digit_table[digit];
        num -= sub * digit;
    }
}

void jd_DStringAppendU32(jd_DString* d_string, u32 num, u32 radix) {
    u64 places = 0; 
    for (u64 quot = num; quot > 0; quot /= radix) {
        places++;
    }
    
    u8* ptr = jd_ArenaAlloc(d_string->arena, places);
    d_string->count += places;
    
    for (u64 i = 0; i < places; i++) {
        if (i + 1 == places) {
            ptr[i] = _jd_digit_table[num];
            break;
        }
        
        u64 sub = jd_Pow_u64(radix, (places - i) - 1);
        u64 digit = num / sub;
        ptr[i] = _jd_digit_table[digit];
        num -= sub * digit;
    }
}

void jd_DStringAppendI32(jd_DString* d_string, i32 num, u32 radix) {
    
}

void jd_DStringAppendU64(jd_DString* d_string, u64 num, u32 radix) {
    
}

void jd_DStringAppendI64(jd_DString* d_string, i64 num, u32 radix) {
    
}

void jd_DStringAppendBin(jd_DString* d_string, u64 size, void* ptr) {
    u8* dst = jd_ArenaAlloc(d_string->arena, size);
    jd_MemCpy(dst, ptr, size);
}