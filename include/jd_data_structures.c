jd_DArray* jd_DArrayCreate(u64 max, u64 stride) {
    if (stride <= 0) 
        return 0; // err
    u64 a_cap = max * stride;
    jd_Arena* arena = jd_ArenaCreate(jd_Max(max * stride, KILOBYTES(64)), jd_Max(stride, KILOBYTES(4)));
    if (!arena) // err 
        return 0;
    
    jd_DArray* arr = jd_ArenaAlloc(arena, sizeof(jd_DArray));
    arr->view.mem = arena->mem + arena->pos;
    arr->arena = arena;
    arr->stride = stride;
    return arr;
}

jd_ForceInline void* jd_DArrayGetIndex(jd_DArray* d_array, u64 index) {
    if (index >= d_array->count) return 0;
    return (void*)((u8*)(d_array->view.mem + (d_array->stride * index)));
}

jd_ForceInline void* jd_DArrayGetBack(jd_DArray* d_array) {
    return jd_DArrayGetIndex(d_array, d_array->count - 1);
}

jd_ForceInline void* jd_DArrayPushBack(jd_DArray* d_array, void* data) { 
    void* ptr = jd_ArenaAlloc(d_array->arena, d_array->stride);
    if (!ptr) return 0;
    
    if (data) {
        jd_MemCpy(ptr, data, d_array->stride);
    }
    
    d_array->count++;
    d_array->view.size += d_array->stride;
    return ptr;
}


jd_ForceInline void* jd_DArrayPushBackV(jd_DArray* d_array, u64 count, void* data) {
    void* ptr = jd_ArenaAlloc(d_array->arena, (d_array->stride * count));
    if (!ptr) return 0;
    
    if (data) {
        jd_MemCpy(ptr, data, (d_array->stride * count));
    }
    
    d_array->count += count;
    d_array->view.size += (d_array->stride * count);
    return ptr;
}


jd_ForceInline void* jd_DArrayPushAtIndex(jd_DArray* d_array, u64 index, void* data) { 
    u64 space_in_front = (d_array->arena->pos - (d_array->stride * index));
    u8* space = jd_ArenaAlloc(d_array->arena, d_array->stride);
    u8* insert = jd_DArrayGetIndex(d_array, index);
    u8* next = insert + d_array->stride;
    jd_MemMove(next, insert, space_in_front);
    if (data) jd_MemCpy(insert, data, d_array->stride);
    d_array->count++;
    d_array->view.size += d_array->stride;
    return insert;
}

jd_ForceInline void* jd_DArrayPushAtIndexV(jd_DArray* d_array, u64 index, u64 count, void* data) {
    u64 space_in_front = (d_array->arena->pos - (d_array->stride * index));
    u8* space = jd_ArenaAlloc(d_array->arena, d_array->stride * count);
    u8* insert = jd_DArrayGetIndex(d_array, index);
    u8* next = insert + (d_array->stride * count);
    jd_MemMove(next, insert, space_in_front);
    if (data) jd_MemCpy(insert, data, (d_array->stride * count));
    d_array->count += count;
    d_array->view.size += (d_array->stride) * count;
    return insert;
}

jd_ForceInline b32 jd_DArrayPopIndex(jd_DArray* d_array, u64 index) { 
    if (d_array->count == 0) return false;
    if (index > d_array->count - 1) return false;
    
    u8* ptr = jd_DArrayGetIndex(d_array, index);
    if (index < d_array->count - 1) {
        jd_MemMove(ptr, ptr + d_array->stride, d_array->stride * (d_array->count - (index + 1)));
    }
    
    jd_DArrayClearToIndex(d_array, (d_array->count - 1));
    d_array->view.size -= d_array->stride;
    return true;
}

jd_ForceInline b32 jd_DArrayPopBack(jd_DArray* d_array) { 
    jd_DArrayPopIndex(d_array, d_array->count - 1);
    return true;
}

jd_ForceInline b32 jd_DArrayPopFront(jd_DArray* d_array) { 
    jd_DArrayPopIndex(d_array, 0);
    return true;
}

jd_ForceInline b32 jd_DArrayClear(jd_DArray* d_array) { 
    jd_DArrayClearToIndex(d_array, 0);
    return true;
}

jd_ForceInline b32 jd_DArrayClearNoDecommit(jd_DArray* d_array) {
    jd_DArrayClearToIndexNoDecommit(d_array, 0);
    return true;
}

jd_ForceInline b32 jd_DArrayClearToIndex(jd_DArray* d_array, u64 index) { 
    jd_ArenaPopTo(d_array->arena, sizeof(jd_DArray) + (d_array->stride * index));
    d_array->count = index;
    d_array->view.size = index * d_array->stride;
    return true;
}

jd_ForceInline b32 jd_DArrayClearToIndexNoDecommit(jd_DArray* d_array, u64 index) {
    d_array->count = index;
    d_array->view.size = index * d_array->stride;
    d_array->arena->pos = sizeof(jd_Arena) + sizeof(jd_DArray) + (index * d_array->stride);
    return true;
}

void jd_DArrayRelease(jd_DArray* d_array) { 
    jd_ArenaRelease(d_array->arena);
}

jd_DFile* jd_DFileCreate(u64 max_size) {
    jd_Arena* arena = jd_ArenaCreate(max_size, 0);
    
    jd_DFile* file = jd_ArenaAlloc(arena, sizeof(*file));
    file->arena = arena;
    file->view = (jd_View){arena->mem + arena->pos, 0};
    
    return file;
}

void jd_DFileAppendSized(jd_DFile* df, u64 size, void* ptr) {
    u8* dst = jd_ArenaAlloc(df->arena, size);
    jd_MemCpy(dst, ptr, size);
    df->view.size += size;
}

void jd_DFileRelease(jd_DFile* df) {
    jd_ArenaRelease(df->arena);
}


