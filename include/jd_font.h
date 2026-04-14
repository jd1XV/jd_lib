/* date = April 9th 2026 11:31 pm */
#ifndef JD_FONT_H
#define JD_FONT_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_memory.h"
#endif 

typedef u32 jd_FontHandle;

typedef struct jd_GlyphMetrics {
    jd_V2U offset;
    jd_V2U size;
    
    u32 ascender;
    u32 descender;
    u32 h_advance;
    u32 line_adv;
    u32 line_gap;
    
    u32 codepoint;
    
    struct jd_GlyphMetrics* hash_chain;
} jd_GlyphMetrics;


typedef struct jd_Font2 {
    jd_String id;
    jd_String path;
    u32 current_font_size;
    
    struct jd_GlyphMetrics* metrics_table;
    u64 metrics_table_count;
    
    jd_FontHandle handle;
} jd_Font2;

u8* jd_FontGetGlyphBitmap(jd_Font2 font, u32 codepoint);

#endif //JD_FONT_H
