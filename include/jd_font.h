/* date = April 9th 2026 11:31 pm */
#ifndef JD_FONT_H
#define JD_FONT_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_memory.h"
#endif 

typedef u32 jd_FontHandle;

typedef struct jd_GlyphMetrics {
    struct jd_Font* font;
    u32 codepoint;
    f32 h_advance;
    b32 loaded;
    
    f32 offset_x;
    f32 offset_y;
    
    struct jd_GlyphMetrics* hash_chain;
} jd_GlyphMetrics;

typedef struct jd_Font {
    jd_String id;
    jd_String path;
    u32 current_font_size;
    
    f32 ascent;
    f32 descent;
    f32 line_gap;
    f32 line_adv;
    
    f32 layout_line_height;
    
    struct jd_GlyphMetrics* metrics_table;
    u64 metrics_table_count;
    
    jd_FontHandle handle;
} jd_Font;

jd_ExportFn jd_Font jd_FontAdd(jd_String path);
jd_GlyphMetrics* jd_FontGetGlyphMetrics(jd_Font* font, u32 codepoint);
jd_ExportFn jd_Bitmap jd_FontGetGlyphBitmap(jd_Arena* arena, jd_Font* font, u32 codepoint, u32 size);
jd_V2F jd_FontGetTextLayoutExtent(jd_Font* font, u16 point_size, jd_String string, f32 wrap, b32 wrap_on_newlines);
jd_V2F jd_FontGetPenPositionForIndex(jd_Font* font, u16 point_size, jd_String string, u64 index, f32 wrap, b32 wrap_on_newlines);

jd_ExportFn jd_Font* jd_OSBaseFont();
jd_ExportFn jd_Font* jd_OSSymbolFont();

#endif //JD_FONT_H
