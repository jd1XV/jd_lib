void jd_ZeroMemory(void* dest, u64 size) {
    ZeroMemory(dest, size);
}

void jd_MemCpy(void* dest, const void* src, u64 size) {
    CopyMemory(dest, src, size);
}

void jd_MemMove(void* dest, const void* src, u64 size) {
    MoveMemory(dest, src, size);
}

void* jd_HeapAlloc(u64 size) {
    u8* ptr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    if (!ptr) {
        jd_LogError("Could not get heap memory!", jd_Error_OutOfMemory, jd_Error_Fatal);
    }
    
    return ptr;
}

void jd_HeapFree(void* ptr) {
    HeapFree(GetProcessHeap(), 0, ptr);
}

inline u8* _jd_Internal_ArenaReserve(u64 reserve, u64 commit_block_size) {
    u8* ptr = 0;
    
    ptr = VirtualAlloc(0, reserve, MEM_RESERVE, PAGE_READWRITE);
    VirtualAlloc(ptr, commit_block_size, MEM_COMMIT, PAGE_READWRITE);
    return ptr;
}

inline void _jd_Internal_ArenaCommit(jd_Arena* arena, u64 size) {
    VirtualAlloc(arena->mem + arena->commit_pos, size, MEM_COMMIT, PAGE_READWRITE); 
}

inline void _jd_Internal_ArenaDecommit(jd_Arena* arena, u64 pos, u64 size) {
    VirtualFree(arena->mem + pos, size, MEM_DECOMMIT);
}

jd_Arena* jd_ArenaCreate(u64 capacity, u64 commit_page_size) {
    if (capacity == 0) capacity = GIGABYTES(1);
    capacity = jd_Max(capacity, KILOBYTES(64));
    commit_page_size = jd_Max(commit_page_size, KILOBYTES(4));
    
    jd_Arena* arena = (jd_Arena*)_jd_Internal_ArenaReserve(capacity, commit_page_size);
    
    arena->cap = capacity;
    arena->pos = sizeof(jd_Arena);
    arena->_commit_page_size = commit_page_size;
    arena->commit_pos = commit_page_size; // we are assuming we are always going to be using memory pretty shortly after getting it. Plus it's 8MB and it's 2023.
    
    arena->mem = (u8*)arena;
    
    return arena;
}

void* jd_ArenaAlloc(jd_Arena* arena, u64 size) {
    void* result = 0;
    
    if (arena->pos + size > arena->cap) {
        jd_LogError("Arena has reached its capacity!", jd_Error_OutOfMemory, jd_Error_Fatal);
    }
    
    result = arena->mem + arena->pos;
    arena->pos += size;
    
    if (arena->pos > arena->commit_pos) {
        u64 pos_aligned = jd_AlignUpPow2(arena->pos, arena->_commit_page_size);
        u64 next_commit_pos = jd_Min(pos_aligned, arena->cap);
        u64 commit_size = next_commit_pos - arena->commit_pos;
        
        _jd_Internal_ArenaCommit(arena, commit_size);
        arena->commit_pos = next_commit_pos;
    }
    
    return result;
}

jd_View jd_ArenaAllocView(jd_Arena* arena, u64 size) {
    jd_View view = {
        .mem = jd_ArenaAlloc(arena, size),
        .size = size
    };
    
    return view;
}

void jd_ArenaPopTo(jd_Arena* arena, u64 pos) {
    arena->pos = sizeof(jd_Arena) + pos;
    u64 pos_aligned = jd_AlignUpPow2(arena->pos, arena->_commit_page_size);
    i64 diff = pos_aligned - arena->pos;
    if (diff > 0)
        jd_ZeroMemory(arena->mem + arena->pos, pos_aligned - arena->pos);
    if (arena->commit_pos > pos_aligned && arena->commit_pos > arena->_commit_page_size) {
        u64 diff = arena->commit_pos - pos_aligned;
        _jd_Internal_ArenaDecommit(arena, pos_aligned, diff);
        arena->commit_pos = pos_aligned;
    }
}

void jd_ArenaRelease(jd_Arena* arena) {
    VirtualFree(arena, 0, MEM_RELEASE);
}

jd_ScratchArena jd_ScratchArenaCreate(jd_Arena* arena) {
    if (!arena) {
        jd_LogError("Cannot create scratch arena on null arena.", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_ScratchArena scratch = {
        .arena = arena,
        .pos = arena->pos - sizeof(jd_Arena)
    };
    
    return scratch;
}

void jd_ScratchArenaRelease(jd_ScratchArena scratch) {
    if (!scratch.arena || scratch.arena->pos < scratch.pos) {
        jd_LogError("Arena has been modified since the creation of the scratch. Ensure you're not sharing this arena between threads.", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_ArenaPopTo(scratch.arena, scratch.pos);
}

void jd_MemSet(void* dest, const u8 val, u64 size) {
    jd_CPUFlags flags = jd_SysInfoGetCPUFlags();
    u64 index = 0;
    
    u8* _dest = dest;
    
    if (jd_CPUFlagIsSet(flags, jd_CPUFlags_SupportsAVX)) {
        __m256i val32 = _mm256_set1_epi8(val);
        for (; index + 32 <= size; index += 32) {
            _mm256_storeu_si256((__m256i*)(_dest + index), val32);
        }
        
    }
    
    if (index == size) return;
    
    if (jd_CPUFlagIsSet(flags, jd_CPUFlags_SupportsSSE2)) {
        __m128i val16 = _mm_set1_epi8(val);
        for (; index + 16 <= size; index += 16) {
            _mm_storeu_si128((__m128i*)(_dest + index), val16);
        }
    }
    
    if (index == size) return;
    
    for (; index < size; index++) {
        _dest[index] = val;
    }
}

b32 jd_MemCmp(void* a_, void* b_, u64 size) {
    jd_CPUFlags flags = jd_SysInfoGetCPUFlags();
    u64 index = 0;
    
    u8* a = a_;
    u8* b = b_;
    
    if (jd_CPUFlagIsSet(flags, jd_CPUFlags_SupportsAVX)) {
        for (; index + 32 <= size; index += 32) {
            __m256i _a = _mm256_loadu_si256((__m256i*)(a + index));
            __m256i _b = _mm256_loadu_si256((__m256i*)(b + index));
            __m256i res = _mm256_cmpeq_epi8(_a, _b);
            i32 mask = _mm256_movemask_epi8(res);
            
            if (mask != 0xFFFFFFFF) {
                return false;
            }
        }
        
    }
    
    if (index == size) return true;
    
    if (jd_CPUFlagIsSet(flags, jd_CPUFlags_SupportsSSE2)) {
        for (; index + 16 <= size; index += 16) {
            __m128i _a = _mm_loadu_si128((__m128i*)(a + index));
            __m128i _b = _mm_loadu_si128((__m128i*)(b + index));
            __m128i res = _mm_cmpeq_epi8(_a, _b);
            i32 mask = _mm_movemask_epi8(res);
            
            if (mask != 0xFFFF) {
                return false;
            }
        }
    }
    
    if (index == size) return true;
    
    for (; index < size; index++) {
        if (*(a + index) != *(b + index)) return false;
    }
    
    return true;
}


#define jd_BitScanMSB32(in, mask) _BitScanReverse(in, mask)
#define jd_BitScanLSB32(in, mask) _BitScanForward(in, mask)

#define jd_BitScanMSB64(in, mask) _BitScanReverse64(in, mask)
#define jd_BitScanLSB64(in, mask) _BitScanForward64(in, mask)
