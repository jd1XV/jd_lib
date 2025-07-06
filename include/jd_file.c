b32 jd_FileReadU64(jd_File file, u64* index, u64* ptr){
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(u64*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadU32(jd_File file, u64* index, u32* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(u32*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadU16(jd_File file, u64* index, u16* ptr){
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(u16*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadU8(jd_File file, u64* index, u8* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(u8*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadI64(jd_File file, u64* index, i64* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(i64*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadI32(jd_File file, u64* index, i32* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(i32*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadI16(jd_File file, u64* index, i16* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(i16*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadI8(jd_File file, u64* index, i8* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(i8*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadB32(jd_File file, u64* index, b32* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(b32*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadB16(jd_File file, u64* index, b16* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(b16*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadB8(jd_File file, u64* index, b8* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(b8*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadF64(jd_File file, u64* index, f64* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(f64*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadF32(jd_File file, u64* index, f32* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(f32*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadC8(jd_File file, u64* index, c8* ptr){ 
    if (*index + sizeof(*ptr) <= file.size) {
        *ptr = *(c8*)(file.mem + *index);
        *index += sizeof(*ptr);
        return true;
    } 
    return false;
}

b32 jd_FileReadString(jd_File file, u64* index, u64 count, jd_String* str) {
    if (*index + count <= file.size) {
        str->mem = file.mem + *index;
        str->count = count;
        *index += count;
        return true;
    }
    
    return false;
}
