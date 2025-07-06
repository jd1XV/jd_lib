jd_ForceInline static void _jd_UTF8toUTF32(jd_UTFDecodedString* dec_str, jd_String input, b32 validate) {
    
    static const u8 length_table[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0
    };
    
    static const i32 masks[]  = {0x00, 0x7f, 0x1f, 0x0f, 0x07};
    static const u32  mins[] = {4194304, 0, 128, 2048, 65536};
    static const i64 mask_ascii = 0x8080808080808080ll;
    
    for (u32 i = 0; i < input.count;) {
        if (i + 8 < input.count) {
            u64 chunk = *(u64*)(input.mem + i);
            u8* chunk_bytes = (u8*)&chunk;
            if ((chunk & mask_ascii) == 0) {
                dec_str->utf32[dec_str->count++] = (u32)(chunk_bytes[0]);
                dec_str->utf32[dec_str->count++] = (u32)(chunk_bytes[1]);
                dec_str->utf32[dec_str->count++] = (u32)(chunk_bytes[2]);
                dec_str->utf32[dec_str->count++] = (u32)(chunk_bytes[3]);
                dec_str->utf32[dec_str->count++] = (u32)(chunk_bytes[4]);
                dec_str->utf32[dec_str->count++] = (u32)(chunk_bytes[5]);
                dec_str->utf32[dec_str->count++] = (u32)(chunk_bytes[6]);
                dec_str->utf32[dec_str->count++] = (u32)(chunk_bytes[7]);
                
                i += 8;
                continue;
            }
        }
        
        u32 length = length_table[input.mem[i] >> 3];
        
        if (i + length > input.count) {
            jd_LogError("Stream too short for UTF8 value", jd_Error_BadInput, jd_Error_Critical);
        }
        
        u32 codepoint = 0;
        
        switch (length) {
            case 0: { // continuation character, these will never appear at this branch in a valid utf8 string
                //       because we iterate i in all correct cases  
                jd_LogError("UTF8 continuation character found where it doesn't belong", jd_Error_BadInput, jd_Error_Common);
                return;
            } break;
            
            case 1: {
                dec_str->utf32[dec_str->count++] |= input.mem[i];
            } break;
            
            case 2: {
                codepoint |= (input.mem[i] & masks[2]) << 6;
                codepoint |= (input.mem[i + 1] & 0x3f) << 0;
                dec_str->utf32[dec_str->count++] |= codepoint;
            } break;
            
            case 3: {
                codepoint |= (input.mem[i] & masks[3]) << 12;
                codepoint |= (input.mem[i + 1] & 0x3f) << 6;
                codepoint |= (input.mem[i + 2] & 0x3f) << 0;
                
                dec_str->utf32[dec_str->count++] |= codepoint;
            } break;
            
            case 4: {
                codepoint |= (input.mem[i] & masks[4]) << 18;
                codepoint |= (input.mem[i + 1] & 0x3f) << 12;
                codepoint |= (input.mem[i + 2] & 0x3f) << 6;
                codepoint |= (input.mem[i + 3] & 0x3f) << 0;
                
                dec_str->utf32[dec_str->count++] |= codepoint;
            } break;
        }
        
        i += length;
    }
}

jd_UTFDecodedString jd_UnicodeDecodeUTF8String(jd_Arena* arena, jd_UnicodeTF tf, jd_String input, b32 validate) {
    jd_UTFDecodedString dec_str = {
        .tf = tf,
    };
    
    switch (tf) {
        default: {
            return dec_str;
        } break;
        
        case jd_UnicodeTF_UTF32: {
            dec_str.utf32 = jd_ArenaAlloc(arena, input.count * 4);
            _jd_UTF8toUTF32(&dec_str, input, validate);
        } break;
        
        case jd_UnicodeTF_UTF16: {
            
        } break;
    }
    
    return dec_str;
}

u32 jd_UnicodeDecodeUTF8Codepoint(jd_String string, u64* index) {
    static const u8 length_table[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0
    };
    static const i32 masks[]  = {0x00, 0x7f, 0x1f, 0x0f, 0x07};
    
    u64 i = *index;
    
    if (i >= string.count) {
        return 0;
    }
    
    u32 length = length_table[string.mem[i] >> 3];
    
    if (i + length > string.count) {
        jd_LogError("Stream too short for UTF8 value", jd_Error_BadInput, jd_Error_Critical);
        return 0;
    }
    
    u32 codepoint = 0;
    
    switch (length) {
        case 1: {
            codepoint |= string.mem[i];
        } break;
        
        case 2: {
            codepoint |= (string.mem[i] & masks[2]) << 6;
            codepoint |= (string.mem[i + 1] & 0x3f) << 0;
        } break;
        
        case 3: {
            codepoint |= (string.mem[i] & masks[3]) << 12;
            codepoint |= (string.mem[i + 1] & 0x3f) << 6;
            codepoint |= (string.mem[i + 2] & 0x3f) << 0;
        } break;
        
        case 4: {
            codepoint |= (string.mem[i] & masks[4]) << 18;
            codepoint |= (string.mem[i + 1] & 0x3f) << 12;
            codepoint |= (string.mem[i + 2] & 0x3f) << 6;
            codepoint |= (string.mem[i + 3] & 0x3f) << 0;
        } break;
    }
    
    *index += length;
    
    return codepoint;
    
}

// 0xxxxxxx
// 110xxxxx 10xxxxxx
// 1110xxxx 10xxxxxx 10xxxxxx
// 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

jd_String jd_UnicodeEncodeUTF32toUTF8(jd_Arena* arena, jd_UTFDecodedString input, b32 validate) {
    jd_String str = {
        .mem = jd_ArenaAlloc(arena, input.count * 4),
        .count = input.count * 4
    };
    
    u64 real_count = 0;
    
    for (u64 i = 0; i < input.count; i++) {
        u32 codepoint = input.utf32[i];
        
        if (codepoint <= (0x7F)) {
            str.mem[real_count] = codepoint;
            real_count++;
        }
        
        else if (codepoint < (1 << 11)) {
            str.mem[real_count] = 0xC0 | (codepoint >> 6);
            real_count++;
            str.mem[real_count] = 0x80 | (codepoint & 0x3F);
            real_count++;
        }
        
        else if (codepoint < (1 << 16)) {
            str.mem[real_count] = 0xE0 | (codepoint >> 12);
            real_count++;
            str.mem[real_count] = 0x80 | ((codepoint >> 6) & 0x3F);
            real_count++;
            str.mem[real_count] = 0x80 | (codepoint & 0x3F);
            real_count++;
        }
        
        else if (codepoint < (1 << 21)) {
            str.mem[real_count] = 0xF0 | (codepoint >> 18);
            real_count++;
            str.mem[real_count] = 0x80 | ((codepoint >> 12) & 0x3F);
            real_count++;
            str.mem[real_count] = 0x80 | ((codepoint >> 6) & 0x3F);
            real_count++;
            str.mem[real_count] = 0x80 | (codepoint & 0x3F);
            real_count++;
        }
    }
    
    str.count = real_count;
    return str;
}
