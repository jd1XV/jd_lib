/* date = March 16th 2026 11:07 am */

#ifndef JD_RENDERER2_H
#define JD_RENDERER2_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_file.h"
#include "jd_disk.h"
#include "jd_string.h"
#include "jd_memory.h"
#include "jd_data_structures.h"
#include "jd_disk.h"
#include "jd_hash.h"
#include "jd_app.h"
#include "jd_font.h"
#endif

typedef struct jd_Texture {
    u32 index;
    jd_V3F uvw_min;
    jd_V3F uvw_max;
    
    jd_V2F uv_factor;
} jd_Texture;

typedef struct jd_TextureCache jd_TextureCache;
void jd_2DRendererBindWindow(struct jd_Window* window);
void jd_2DRendererBegin(struct jd_Window* window);
void jd_2DRendererDraw();
void jd_2DRendererInit();

jd_ExportFn u32 jd_TextureKey(jd_String s);

jd_ExportFn void jd_2DColorRect(jd_V2F position, jd_V2F size, jd_V4F color, f32 corner_rad, f32 softness, f32 thickness, jd_V4F clip);
jd_ExportFn void jd_2DGlyphRect(jd_Font2* font, jd_GlyphMetrics* metrics, u32 codepoint, u16 point_size, jd_V2F position, jd_V4F color, jd_V4F clip);
jd_ExportFn void jd_2DTextureRect(jd_Texture t, jd_V2F position, jd_V2F size, f32 corner_rad, f32 softness, f32 thickness, jd_V4F clip);
jd_ExportFn void jd_2DString(jd_Font2* font, jd_String string, u16 point_size, jd_V2F position, jd_V4F color, jd_V4F clip, f32 wrap, b32 wrap_on_newlines);

jd_ExportFn b32 jd_TextureCacheCheck(jd_TextureCache* cache, u32 key, jd_Texture* texture);
jd_ExportFn jd_Texture jd_TextureCacheInsert(jd_TextureCache* cache, b8 perm, u32 key, u8* bitmap, b32 monochrome, u32 width, u32 height, b32 fit);
jd_ExportFn jd_TextureCache* jd_TextureCacheCreate(jd_Arena* arena, u32 width, u32 height, u32 depth, u32 cell_width, u32 cell_height);


#endif //JD_RENDERER2_H
