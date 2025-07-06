/* date = February 20th 2024 1:05 am */

#ifndef JD_MEMORY_H
#define JD_MEMORY_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_sysinfo.h"
#include "jd_error.h"
#endif

typedef struct jd_Arena {
    u8* mem;
    u64 cap;
    u64 pos;
    u64 commit_pos;
    u64 _commit_page_size;
} jd_Arena;

typedef struct jd_ScratchArena {
    jd_Arena* arena;
    u64 pos;
} jd_ScratchArena;

jd_ExportFn void*     jd_HeapAlloc(u64 size);

jd_ExportFn jd_Arena* jd_ArenaCreate(u64 reserve, u64 commit_block_size);
jd_ExportFn void*     jd_ArenaAlloc(jd_Arena* arena, u64 size);
jd_ExportFn jd_View   jd_ArenaAllocView(jd_Arena* arena, u64 size);
jd_ExportFn void      jd_ArenaPopTo(jd_Arena* arena, u64 pos);
jd_ExportFn void      jd_ArenaPopBlock(jd_Arena* arena, void* start, u64 size);
jd_ExportFn void      jd_ArenaRelease(jd_Arena* arena);

jd_ExportFn jd_ScratchArena jd_ScratchArenaCreate(jd_Arena* arena);
jd_ExportFn void            jd_ScratchArenaRelease(jd_ScratchArena scratch);

jd_ExportFn void jd_ZeroMemory(void* dest, u64 size);
jd_ExportFn void jd_MemCpy(void* dest, const void* src, u64 size);
jd_ExportFn void jd_MemMove(void* dest, const void* src, u64 count);
jd_ExportFn void jd_MemSet(void* dest, u8 val, u64 size);
jd_ExportFn b32  jd_MemCmp(void* a, void* b, u64 size);

#ifdef JD_IMPLEMENTATION

#ifdef JD_WINDOWS
#include "win32_jd_memory.c"
#endif

#endif // JD_IMPLEMENTATION

#endif //JD_MEMORY_H
