// Note: This is basically MurmurHash3, just with a fixed input size.
jd_ForceInline u32 _jd_RotateLeft32(u32 x, u8 r) {
    return (x << r) | (x >> (32 - r));
}

jd_ForceInline u32 _jd_FinalMix32(u32 hash) {
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;
    
    return hash;
}

u32 jd_HashU32toU32(u32 in, u32 seed) {
    u32 hash = seed;
    
    u32 c1 = 0xcc9e2d51;
    u32 c2 = 0x1b873593;
    
    u32 k1 = in;
    if (k1 >> 8 == 0) k1 |= (k1 << 8);
    if (k1 >> 16 == 0) k1 |= (k1 << 16);
    if (k1 >> 24 == 0) k1 |= (k1 << 24);
    
    k1 *= c1;
    k1 = _jd_RotateLeft32(k1, 15);
    k1 *= c2;
    
    hash ^= k1;
    hash = _jd_RotateLeft32(hash, 13); 
    hash = hash * 5 + 0xe6546b64;
    
    hash ^= 4 * (in >> 3);
    
    hash = _jd_FinalMix32(hash);
    return hash;
}

u32 jd_HashStrToU32(jd_String input_str, u32 seed) {
    u32 num_blocks = input_str.count / 4;
    u32 hash = seed;
    
    u32 c1 = 0xcc9e2d51;
    u32 c2 = 0x1b873593;
    
    for (u32 i = 0; i < num_blocks; i++) {
        u32 k1 = *(u32*)(input_str.mem + (i * 4));
        
        k1 *= c1;
        k1 = _jd_RotateLeft32(k1, 15);
        k1 *= c2;
        
        hash ^= k1;
        hash = _jd_RotateLeft32(hash, 13); 
        hash = hash * 5 + 0xe6546b64;
    }
    
    u32 k1 = 0;
    
    u32 remaining = input_str.count - (num_blocks * 4);
    u64 remaining_idx = input_str.count - remaining;
    switch ((remaining) & 3) {
        case 3: k1 ^= input_str.mem[remaining_idx + 2] << 16;
        case 2: k1 ^= input_str.mem[remaining_idx + 1] << 8;
        case 1: k1 ^= input_str.mem[remaining_idx];
        k1 *= c1; 
        k1 = _jd_RotateLeft32(k1, 15); 
        k1 *= c2; 
        hash ^= k1;
    }
    
    hash ^= input_str.count;
    hash = _jd_FinalMix32(hash);
    
    return hash;
}

u32 jd_HashU64toU32(u64 val, u32 seed) {
    jd_String val_as_string = {
        .mem = (u8*)&val,
        .count = sizeof(val)
    };
    
    return jd_HashStrToU32(val_as_string, seed);
}
