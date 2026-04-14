/* date = March 16th 2026 11:07 am */

#ifndef JD_RENDERER2_H
#define JD_RENDERER2_H

typedef struct jd_Texture {
    u32 index;
    jd_V3F uvw_min;
    jd_V3F uvw_max;
} jd_Texture;

typedef struct jd_TextureCache jd_TextureCache;
void jd_2DRendererBindWindow(struct jd_Window* window);
void jd_2DRendererBegin(struct jd_Window* window);
void jd_2DRendererDraw();
void jd_2DRendererInit();

jd_ExportFn u64 jd_TextureKey(jd_String s);
jd_ExportFn u64 jd_GlyphTextureKey(u16 dpi, u16 point_size, u32 codepoint);

jd_ExportFn void jd_2DColorRect(jd_V2F position, jd_V2F size, jd_V4F color, f32 corner_rad, f32 softness, f32 thickness, jd_V4F clip);
jd_ExportFn void jd_2DTextureRect(jd_Texture t, jd_V2F position, jd_V2F size, f32 corner_rad, f32 softness, f32 thickness, jd_V4F clip);

jd_ExportFn b32 jd_TextureCacheCheck(jd_TextureCache* cache, u64 key);
jd_ExportFn jd_Texture jd_TextureCacheInsert(jd_TextureCache* cache, b8 perm, u64 key, u8* bitmap, b32 monochrome, u32 width, u32 height, b32 fit);
jd_ExportFn jd_TextureCache* jd_TextureCacheCreate(jd_Arena* arena, u32 width, u32 height, u32 depth, u32 cell_width, u32 cell_height);


#endif //JD_RENDERER2_H
