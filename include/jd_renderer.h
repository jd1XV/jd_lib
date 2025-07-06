#ifndef JD_RENDERER_H
#define JD_RENDERER_H

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
#endif

typedef struct jd_Renderer jd_Renderer;

typedef struct jd_Rectangle {
    jd_V2F min;
    jd_V2F max;
} jd_Rectangle;

typedef struct jd_TypefaceUnicodeRange {
    u32 begin;
    u32 end;
} jd_TypefaceUnicodeRange;

static jd_ReadOnly jd_TypefaceUnicodeRange jd_unicode_range_basic_latin = {0, 0x7F};
static jd_ReadOnly jd_TypefaceUnicodeRange jd_unicode_range_bmp = {0, 0xFFFF};
static jd_ReadOnly jd_TypefaceUnicodeRange jd_unicode_range_all = {0, 0x10FFFF};

typedef enum jd_TextOrigin {
    jd_TextOrigin_TopLeft,
    jd_TextOrigin_BottomLeft,
    jd_TextOrigin_BottomRight,
    jd_TextOrigin_TopRight,
    jd_TextOrigin_Center,
    jd_TextOrigin_Count
} jd_TextOrigin;

#define jd_Max_Fonts 256

typedef struct jd_Typeface {
    jd_String id_str;
    u32 dpi;
    f32 face_size;
    f32 ascent;
    f32 descent;
    f32 line_gap;
    f32 line_adv;
    f32 max_adv;
    b8  fallback;
    jd_TypefaceUnicodeRange range;
    struct jd_Font* font;
} jd_Typeface;

typedef struct jd_2DTexture {
    jd_V2U res;
    u32    gl_index;
    u32    num_slots;
    u32    used_slots;
    b32    bound;
    
    jd_DArray* vertices;
    
    jd_Node(jd_2DTexture);
} jd_2DTexture;

typedef struct jd_Glyph {
    jd_V2F offset;
    jd_V2F size;
    f32 h_advance;
    
    u32 codepoint;
    jd_Typeface* face;
    
    u32 hash;
    
    struct jd_Glyph* next_with_same_hash;
    
    jd_2DTexture* texture;
    u32 layer_index;
} jd_Glyph;

typedef struct jd_Font {
    jd_String id;
    jd_Arena* arena;
    jd_Glyph* glyphs; // hash table
    u64       glyph_count;
    
    u32 texture_width;
    u32 texture_height;
    u8* white_bitmap;
    
    u64          max_faces;
    jd_Typeface* faces;
    u64          face_count;
    jd_Renderer* renderer;
} jd_Font;

typedef enum jd_TypeLayout {
    jd_TypeLayout_LeftAlign,
    jd_TypeLayout_RightAlign,
    jd_TypeLayout_Centered,
    jd_TypeLayout_Count
} jd_TypeLayout;

typedef struct jd_GLVertex {
    jd_V3F pos;
    jd_V3F tx;
    jd_V4F col;
    jd_V4F rect;
    f32 rounding;
    f32 softness;
    f32 thickness;
} jd_GLVertex;

typedef struct jd_RenderObjects {
    u32 vao;
    u32 vbo;
    u32 shader;
    u32 fbo;
    u32 tcb;
    u32 rbo;
    u32 fbo_tex;
} jd_RenderObjects;

#define jd_TexturePool_Max_Passes 16

#define jd_Renderer_Max_Texture_Passes 256

typedef struct jd_TexturePool {
    jd_2DTexture* textures; // hash table
    jd_2DTexture* free;
    jd_Arena* arena;
    u64 texture_count;
} jd_TexturePool;

typedef struct jd_Renderer {
    jd_Arena* arena;
    jd_Arena* frame_arena;
    
    jd_RenderObjects objects;
    
    f32 dpi_scaling;
    jd_V2F render_size;
    
    jd_2DTexture* render_chain;
    
    struct jd_PlatformWindow* current_window;
    
    u32 max_texture_units;
    u32 max_texture_depth;
} jd_Renderer;

jd_ExportFn jd_Renderer* jd_RendererGet();
void jd_RendererInit();
jd_ExportFn void jd_RendererBegin(jd_V2F render_size);
jd_ExportFn void jd_DrawString(jd_String font_id, jd_String str, jd_V2F window_pos, jd_TextOrigin baseline, jd_V4F color, f32 wrap_width);
jd_ExportFn void jd_CalcStringBoxUTF8(jd_String font_id, jd_String str, f32 wrap_width);
jd_ExportFn void jd_DrawStringWithBG(jd_String font_id, jd_String str, jd_V2F window_pos, jd_TextOrigin baseline, jd_V4F text_color, jd_V4F bg_color, f32 wrap_width, f32 box_rounding, f32 box_softness, f32 box_thickness);
jd_ExportFn void jd_DrawRect(jd_V2F window_pos, jd_V2F size, jd_V4F col, f32 rounding, f32 softness, f32 thickness);
jd_ExportFn void jd_RendererSetDPIScale(jd_Renderer* renderer, f32 scale);
jd_ExportFn void jd_RendererBegin(jd_V2F render_size);
jd_ExportFn void jd_RefreshFonts();
jd_ExportFn void jd_RendererDraw();
jd_ExportFn void jd_FontCreateEmpty(jd_String id, u64 max_memory, u64 max_faces);
jd_ExportFn void jd_FontAddTypefaceFromMemory(jd_String font_id, jd_File file, jd_TypefaceUnicodeRange* range, i32 point_size, f32 dpi);
#ifdef JD_IMPLEMENTATION
#include "jd_renderer.c"
#endif

#endif //JD_RENDERER_H
