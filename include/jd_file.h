/* date = February 22nd 2024 11:32 pm */

#ifndef JD_FILE_H
#define JD_FILE_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_string.h"
#include "jd_memory.h"
#include "jd_error.h"
#endif

typedef jd_View jd_File;

jd_ForceInline b32 jd_FileReadU64(jd_File file, u64* index, u64* ptr);
jd_ForceInline b32 jd_FileReadU32(jd_File file, u64* index, u32* ptr);
jd_ForceInline b32 jd_FileReadU16(jd_File file, u64* index, u16* ptr);
jd_ForceInline b32 jd_FileReadU8(jd_File file, u64* index, u8* ptr);

jd_ForceInline b32 jd_FileReadI64(jd_File file, u64* index, i64* ptr);
jd_ForceInline b32 jd_FileReadI32(jd_File file, u64* index, i32* ptr);
jd_ForceInline b32 jd_FileReadI16(jd_File file, u64* index, i16* ptr);
jd_ForceInline b32 jd_FileReadI8(jd_File file, u64* index, i8* ptr);

jd_ForceInline b32 jd_FileReadB32(jd_File file, u64* index, b32* ptr);
jd_ForceInline b32 jd_FileReadB16(jd_File file, u64* index, b16* ptr);
jd_ForceInline b32 jd_FileReadB8(jd_File file, u64* index, b8* ptr);

jd_ForceInline b32 jd_FileReadF64(jd_File file, u64* index, f64* ptr);
jd_ForceInline b32 jd_FileReadF32(jd_File file, u64* index, f32* ptr);

jd_ForceInline b32 jd_FileReadC8(jd_File file, u64* index, c8* ptr);
jd_ForceInline b32 jd_FileReadString(jd_File file, u64* index, u64 count, jd_String* str);

#ifdef JD_IMPLEMENTATION
#include "jd_file.c"
#endif // JD_IMPLEMENTATION

#endif //JD_FILE_H
