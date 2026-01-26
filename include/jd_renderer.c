
#include "dep/glad/glad_wgl.h"
#include "dep/glad/glad.h"

#include <ft2build.h>
#include FT_FREETYPE_H

static jd_Renderer* jd_global_renderer = 0;


jd_String vs_string = jd_StrConst("#version 430\n"
                                  "layout (location = 0) in vec3 vert_xyz;\n"
                                  "layout (location = 1) in vec3 vert_uvw;\n"
                                  "layout (location = 2) in vec4 vert_col;\n"
                                  "layout (location = 3) in vec4 vert_rect;\n"
                                  "layout (location = 4) in float rounding;\n"
                                  "layout (location = 5) in float softness;\n"
                                  "layout (location = 6) in float thickness;\n"
                                  "\n"
                                  "out vec3 fs_xyz;\n"
                                  "out vec3 fs_uvw;\n"
                                  "out vec4 fs_col;\n"
                                  "out vec4 fs_rect;\n"
                                  "out float fs_rounding;\n"
                                  "out float fs_softness;\n"
                                  "out float fs_thickness;\n"
                                  "\n"
                                  "uniform vec2 screen_conv;\n"
                                  "\n"
                                  "void main(){\n"
                                  "    gl_Position.x = (vert_xyz.x * screen_conv.x) - 1;\n"
                                  "    gl_Position.y = 1 - (vert_xyz.y * screen_conv.y);\n"
                                  "    gl_Position.z = vert_xyz.z;\n"
                                  "    gl_Position.w = 1;\n"
                                  "    fs_uvw = vert_uvw;\n"
                                  "    fs_col = vert_col;\n"
                                  "    fs_xyz = vert_xyz;\n"
                                  "    fs_rect = vert_rect;\n"
                                  "    fs_rounding = rounding;\n"
                                  "    fs_softness = softness;\n"
                                  "    fs_thickness = thickness;\n"
                                  "}"
                                  );

jd_String fs_string = jd_StrConst("#version 430\n"
                                  "in vec3 fs_xyz;\n"
                                  "in vec3 fs_uvw;\n"
                                  "in vec4 fs_col;\n"
                                  "in vec4 fs_rect;\n"
                                  "in float fs_rounding;\n"
                                  "in float fs_softness;\n"
                                  "in float fs_thickness;\n"
                                  "\n"
                                  "out vec4 out_col; \n"
                                  "uniform sampler2DArray tex;\n"
                                  "\n"
                                  "// Implementation lifted from Ryan Fleury's UI tutorial\n"
                                  "float RoundingSDF(vec2 sample_pos, vec2 rect_center, vec2 rect_half_size, float r) {\n"
                                  "    vec2 d2 = (abs(rect_center - sample_pos) - rect_half_size + vec2(r, r));\n"
                                  "    return min(max(d2.x, d2.y), 0.0) + length(max(d2, 0.0)) - r;\n"
                                  "}\n"
                                  "\n"
                                  "void main() {\n"
                                  "    vec4 color = texture(tex, fs_uvw);\n"
                                  "    \n"
                                  "    float softness = fs_softness;\n"
                                  "    vec2 softness_padding = vec2(max(0, softness * 2 - 1), max(0, softness * 2 - 1));\n"
                                  "    \n"
                                  "    vec2 p0 = vec2(fs_rect.x, fs_rect.z);\n"
                                  "    vec2 p1 = vec2(fs_rect.y, fs_rect.w);\n"
                                  "    \n"
                                  "    vec2 rect_center = vec2(p1 + p0) / 2;\n"
                                  "    vec2 rect_half_size = vec2(p1 - p0) / 2;\n"
                                  "    \n"
                                  "    float dist = RoundingSDF(fs_xyz.xy, rect_center, rect_half_size - softness_padding, fs_rounding);\n"
                                  "    \n"
                                  "    float border_factor = 1.0f;\n"
                                  "    \n"
                                  "    if (fs_thickness != 0) {\n"
                                  "        vec2 interior_half_size = rect_half_size - vec2(fs_thickness);\n"
                                  "        \n"
                                  "        float interior_radius_reduce = min(interior_half_size.x / rect_half_size.x,\n"
                                  "                                           interior_half_size.y / rect_half_size.y);\n"
                                  "        float interior_corner_radius = (fs_rounding * interior_radius_reduce * interior_radius_reduce);\n"
                                  "        \n"
                                  "        float inside_d = RoundingSDF(fs_xyz.xy, rect_center, interior_half_size - softness_padding, interior_corner_radius);\n"
                                  "        \n"
                                  "        // map distance => factor\n"
                                  "        float inside_f = smoothstep(0, 2 * softness, inside_d);\n"
                                  "        border_factor = inside_f;\n"
                                  "    }\n"
                                  "    \n"
                                  "    // map distance => a blend factor\n"
                                  "    float sdf_factor = 1.0f - smoothstep(0, 2 * softness, dist);\n"
                                  "    \n"
                                  "    out_col = fs_col * color * sdf_factor * border_factor;\n"
                                  "}"
                                  );


jd_ForceInline jd_Renderer* jd_RendererGet() {
    return jd_global_renderer;
}

void jd_ShaderCreate(jd_Renderer* renderer) {
    u32 id = 0;
    id = glCreateProgram();
    
    u32 vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_string.mem, NULL);
    glCompileShader(vs);
    
    i32 success = 0;
    c8 info_log[1024] = {0};
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vs, 1024, NULL, info_log);
        jd_DebugPrint(jd_StrLit(info_log));
        jd_LogError("Vertex shader failed to compile!", jd_Error_BadInput, jd_Error_Critical);
        glDeleteShader(vs);
        glDeleteProgram(id);
        return;
    }
    
    u32 fs = glCreateShader(GL_FRAGMENT_SHADER);
    
    glShaderSource(fs, 1, &fs_string.mem, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    
    if (!success) {
        glGetShaderInfoLog(fs, 1024, NULL, info_log);
        jd_DebugPrint(jd_StrLit(info_log));
        jd_LogError("Fragment Shader failed to compile", jd_Error_BadInput, jd_Error_Critical);
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(id);
        return;
    }
    
    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    renderer->objects.shader = id;
}

#define jd_Render_Font_Texture_Height 128
#define jd_Render_Font_Texture_Width 64
#define jd_Render_Font_Texture_Depth 256

#define jd_Default_Face_Point_Size 12

typedef struct _jd_FT_Instance {
    FT_Library library;
    b32 init;
} _jd_FT_Instance;

static _jd_FT_Instance _jd_ft_global_instance = {0};

#define _jd_GlyphHashTableLimit KILOBYTES(64)

jd_ForceInline u32 jd_GlyphHashGetIndexForCodepoint(u32 codepoint) {
    if (codepoint <= 128) 
        return codepoint;
    
    u32 hash = jd_HashU32toU32(codepoint, 5700877729);
    return hash & _jd_GlyphHashTableLimit - 1;
}

jd_ForceInline jd_Glyph* jd_FontGetGlyph(jd_Font* font, u32 codepoint) {
    u32 hash = jd_GlyphHashGetIndexForCodepoint(codepoint);
    jd_Glyph* glyph = &font->glyphs[hash];
    while (glyph && glyph->codepoint != codepoint) {
        glyph = glyph->next_with_same_hash;
    }
    
    return glyph;
}

jd_ForceInline jd_Glyph* jd_FontInsertGlyph(jd_Font* font, u32 codepoint) {
    u32 hash = jd_GlyphHashGetIndexForCodepoint(codepoint);
    jd_Glyph* glyph = &font->glyphs[hash];
    
    if (glyph->codepoint == 0) {
        glyph->codepoint = codepoint;
        return glyph;
    }
    
    while (glyph->next_with_same_hash != 0)
        glyph = glyph->next_with_same_hash;
    
    glyph->next_with_same_hash = jd_ArenaAlloc(font->arena, sizeof(jd_Glyph));
    glyph = glyph->next_with_same_hash;
    glyph->codepoint = codepoint;
    return glyph;
}

#define jd_Texture_Table_Size 1024

static jd_DArray* jd_internal_font_bank = 0;
static jd_RWLock* jd_internal_font_lock = 0;

static jd_TexturePool* jd_internal_texture_pool = 0;
static jd_RWLock*     jd_internal_texture_pool_lock = 0;
static jd_2DTexture*  jd_internal_rectangle_texture = 0;

typedef enum jd_2DTextureMode {
    jd_2DTextureMode_Image,
    jd_2DTextureMode_Font,
    jd_2DTextureMode_Count
} jd_2DTextureMode;

jd_ForceInline jd_2DTexture* jd_TextureGet(u32 gl_index) {
    u32 hash = jd_HashU32toU32(gl_index, 5700877729);
    u32 slot = hash % jd_Texture_Table_Size;
    jd_RWLockGet(jd_internal_texture_pool_lock, jd_RWLock_Read);
    jd_2DTexture* tex = &jd_internal_texture_pool->textures[slot];
    while (tex && tex->gl_index != gl_index) {
        tex = tex->next;
    }
    jd_RWLockRelease(jd_internal_texture_pool_lock, jd_RWLock_Read);
    return tex;
}

jd_ForceInline jd_2DTexture* jd_TextureInsert(u32 gl_index) {
    u32 hash = jd_HashU32toU32(gl_index, 5700877729);
    u32 slot = hash % jd_Texture_Table_Size;
    
    jd_2DTexture* tex = &jd_internal_texture_pool->textures[slot];
    if (tex->gl_index == 0) {
        tex->gl_index = gl_index;
    }
    
    while (tex->gl_index != gl_index && tex->next) {
        tex = tex->next;
    }
    
    if (tex->gl_index != gl_index) {
        if (jd_internal_texture_pool->free) {
            tex->next = jd_internal_texture_pool->free;
            jd_internal_texture_pool->free = jd_internal_texture_pool->free->next;
            jd_DLinksClear(tex->next);
        } else {
            tex->next = jd_ArenaAlloc(jd_internal_texture_pool->arena, sizeof(jd_2DTexture));
            tex = tex->next;
        }
    }
    
    tex->vertices = jd_DArrayCreate(MEGABYTES(1), sizeof(jd_GLVertex));
    
    return tex;
}

jd_2DTexture* jd_TexturePoolAddTexture(jd_V2U size, u32 depth, jd_2DTextureMode mode) {
    u32 gl_index = 0;
    glGenTextures(1, &gl_index);
    
    if (!jd_internal_texture_pool) {
        // first texture
        jd_internal_texture_pool = jd_HeapAlloc(sizeof(*jd_internal_texture_pool));
        jd_internal_texture_pool_lock = jd_HeapAlloc(sizeof(*jd_internal_texture_pool_lock));
        jd_internal_texture_pool->arena = jd_ArenaCreate(0, 0);
        jd_RWLockInitialize(jd_internal_texture_pool_lock);
        
        jd_internal_texture_pool->textures = jd_ArenaAlloc(jd_internal_texture_pool->arena, sizeof(jd_2DTexture) * jd_Texture_Table_Size);
    }
    
    jd_RWLockGet(jd_internal_texture_pool_lock, jd_RWLock_Write);
    
    jd_2DTexture* tex = jd_TextureInsert(gl_index);
    if (!tex) {
        jd_LogError("Could not get a slot for the texture.", jd_Error_OutOfMemory, jd_Error_Fatal);
    }
    
    tex->gl_index = gl_index;
    tex->num_slots = depth;
    tex->res = size;
    
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex->gl_index);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, size.x, size.y, depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 3);
    
    if (mode == jd_2DTextureMode_Font) {
        i32 swizzle_mask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
        glTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);
    }
    
    if (jd_internal_rectangle_texture == 0) {
        jd_internal_rectangle_texture = tex;
        u8* white_bitmap = jd_HeapAlloc(sizeof(u32) * size.x * size.y);
        jd_MemSet(white_bitmap, 0xFF, sizeof(u32) * size.x * size.y);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, size.x, size.y, 1, GL_RGBA, GL_UNSIGNED_BYTE, white_bitmap);
        tex->used_slots++;
    }
    
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    jd_RWLockRelease(jd_internal_texture_pool_lock, jd_RWLock_Write);
    
    return tex;
}

void jd_FontCreateEmpty(jd_String id, u64 max_memory, u64 max_faces) {
    if (!jd_internal_font_bank) {
        jd_internal_font_bank = jd_DArrayCreate(jd_Max_Fonts, sizeof(jd_Font));
        jd_internal_font_lock = jd_HeapAlloc(sizeof(*jd_internal_font_lock));
        jd_RWLockInitialize(jd_internal_font_lock);
    }
    
    if (max_faces == 0) max_faces = 256;
    
    jd_RWLockGet(jd_internal_font_lock, jd_RWLock_Write);
    
    jd_Font* font = jd_DArrayPushBack(jd_internal_font_bank, 0); 
    
    font->arena = jd_ArenaCreate(max_memory, 0);
    font->id    = jd_StringPush(font->arena, id);
    font->max_faces = max_faces;
    font->faces = jd_ArenaAlloc(font->arena, sizeof(jd_Typeface) * max_faces);
    font->glyphs = jd_ArenaAlloc(font->arena, sizeof(jd_Glyph) * _jd_GlyphHashTableLimit);
    
    jd_RWLockRelease(jd_internal_font_lock, jd_RWLock_Write);
}

jd_Font* jd_FontGetByID(jd_String font_id) {
    jd_Font* font = 0;
    jd_RWLockGet(jd_internal_font_lock, jd_RWLock_Read);
    
    for (u64 i = 0; i < jd_internal_font_bank->count; i++) {
        jd_Font* f = jd_DArrayGetIndex(jd_internal_font_bank, i);
        if (jd_StringMatch(f->id, font_id)) {
            font = f;
            jd_RWLockRelease(jd_internal_font_lock, jd_RWLock_Read);
            return font;
        }
    }
    
    jd_RWLockRelease(jd_internal_font_lock, jd_RWLock_Read);
    
    if (!font) {
        jd_LogError("Could not find font matching provided ID", jd_Error_APIMisuse, jd_Error_Critical);
    }
    
    return font;
}

void jd_FontAddTypefaceFromMemory(jd_String font_id, jd_File file, jd_TypefaceUnicodeRange* range, i32 point_size, f32 dpi) {
    jd_Font* font = jd_FontGetByID(font_id);
    if (!font) {
        return;
    }
    
    if (file.size == 0) {
        jd_LogError("File is zero-sized! Did it load correctly?", jd_Error_MissingResource, jd_Error_Common);
        return;
    }
    
    if (point_size == 0) {
        jd_LogError("Typeface point size must be larger than 0!", jd_Error_APIMisuse, jd_Error_Common);
        return;
    }
    
    if (!range)
        range = &jd_unicode_range_basic_latin;
    
    if (point_size == 0)
        point_size = jd_Default_Face_Point_Size;
    
    if (!_jd_ft_global_instance.init) {
        i32 error = FT_Init_FreeType(&_jd_ft_global_instance.library);
        if (error) {
            jd_LogError("Could not initialize FreeType library.", jd_Error_OutOfMemory, jd_Error_Fatal);
            return;
        }
        
        _jd_ft_global_instance.init = true;
    }
    
    if (font->face_count >= font->max_faces) {
        jd_LogError("Max faces exceeded for font", jd_Error_APIMisuse, jd_Error_Fatal);
        return;
    }
    
    jd_Typeface* face = &font->faces[font->face_count];
    font->face_count++;
    
    FT_Face ft_face = {0};
    i32 error = FT_New_Memory_Face(_jd_ft_global_instance.library, file.mem, file.size, 0, &ft_face);
    if (error) {
        jd_LogError("Could not load font file with FreeType.", jd_Error_BadInput, jd_Error_Critical);
        return;
    }
    
    if (ft_face->num_glyphs == 0) {
        jd_LogError("Font does not contain any glyphs!", jd_Error_BadInput, jd_Error_Common);
        return;
    }
    
    error = FT_Set_Char_Size(ft_face,
                             0,
                             point_size * 64,
                             dpi,
                             dpi);
    
    face->ascent = ft_face->size->metrics.ascender / 64;
    face->descent = ft_face->size->metrics.descender / 64;
    face->max_adv = ft_face->size->metrics.max_advance / 64;
    face->line_adv = face->ascent - face->descent;
    face->range = *range;
    face->face_size = point_size;
    face->dpi = dpi;
    
    u64 texture_height = face->line_adv + (face->line_adv / 2);
    u64 texture_width  = face->max_adv + (face->max_adv / 2);
    u64 texture_depth  = jd_Min(ft_face->num_glyphs, jd_Min(range->end, jd_RendererGet()->max_texture_depth));
    
    jd_V2U size = {texture_width, texture_height};
    
    jd_2DTexture* texture = jd_TexturePoolAddTexture(size, texture_depth, jd_2DTextureMode_Font);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture->gl_index);
    
    for (u32 i = 0; i < range->end; i++) {
        u32 glyph_index = FT_Get_Char_Index(ft_face, i);
        if (glyph_index && (FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT) == 0)) {
            if (FT_Render_Glyph(ft_face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
                jd_LogError("Glyph was present but couldn't be rasterized!", jd_Error_BadInput, jd_Error_Common);
                continue;
            }
            
            jd_Glyph* glyph = jd_FontInsertGlyph(font, i);
            
            glyph->size.w = ft_face->glyph->bitmap.width;
            glyph->size.h = ft_face->glyph->bitmap.rows;
            glyph->offset.x = ft_face->glyph->bitmap_left;
            glyph->offset.y = -ft_face->glyph->bitmap_top;
            glyph->h_advance = ft_face->glyph->metrics.horiAdvance / 64;
            glyph->texture = texture;
            glyph->face = face;
            font->glyph_count++;
            
            u8* bitmap = ft_face->glyph->bitmap.buffer;
            if (bitmap) {
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, texture->used_slots, glyph->size.w, glyph->size.h, 1, GL_RED, GL_UNSIGNED_BYTE, bitmap);
                
                glyph->layer_index = texture->used_slots++;
                
                if (texture->used_slots == texture->num_slots) {
                    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
                    texture = jd_TexturePoolAddTexture(size, texture_depth, jd_2DTextureMode_Font);
                    glBindTexture(GL_TEXTURE_2D_ARRAY, texture->gl_index);
                }
                
            }
        }
    }
    
    FT_Done_Face(ft_face);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
}

jd_ForceInline void jd_TextureQueue2(jd_2DTexture* texture) {
    jd_Renderer* renderer = jd_RendererGet();
    if (renderer->draw_group_chain_tail && renderer->draw_group_chain_tail->tex == texture) return;
    jd_DrawGroup* new_group = jd_ArenaAlloc(renderer->frame_arena, sizeof(jd_DrawGroup));
    new_group->tex = texture;
    new_group->index = renderer->vertices->count;
    
    if (!renderer->draw_group_chain_head) {
        renderer->draw_group_chain_head = new_group;
        renderer->draw_group_chain_tail = new_group;
    }
    
    else {
        jd_DLinkNext(renderer->draw_group_chain_tail, new_group);
        renderer->draw_group_chain_tail = new_group;
    }
    
    texture->bound = true;
}

jd_ForceInline void jd_TextureQueue(jd_2DTexture* texture) {
    jd_TextureQueue2(texture);
#if 0
    if (texture->bound) return;
    jd_Renderer* renderer = jd_RendererGet();
    if (!renderer->render_chain) 
        renderer->render_chain = texture;
    else
        jd_DLinkNext(renderer->render_chain, texture);
    
    texture->bound = true;
#endif
}

jd_RectF32 jd_RectClip(jd_RectF32 rect, jd_RectF32 clip) {
    jd_RectF32 clipped = {
        .min = {jd_Max(rect.min.x, clip.min.x), jd_Max(rect.min.y, clip.min.y)},
        .max = {jd_Min(rect.max.x, clip.max.x), jd_Min(rect.max.y, clip.max.y)}
    };
    
    return clipped;
}

b32 jd_Internal_DrawGlyph(jd_Font* font, jd_V3F window_pos, jd_V4F col, jd_Glyph* g, f32* advance_out, jd_RectF32 clip_rectangle) {
    jd_Renderer* renderer = jd_RendererGet();
    f32 dpi_scaling = ((f32)jd_WindowGetDPI(renderer->current_window) / (f32)g->face->dpi);
    f32 x = window_pos.x;
    f32 y = window_pos.y;
    f32 z = window_pos.z;
    
    jd_RectF32 rect = {0};
    rect.min.x = x + g->offset.x * dpi_scaling;
    rect.min.y = y + g->offset.y * dpi_scaling;
    rect.max.x = rect.min.x + (g->size.x * dpi_scaling);
    rect.max.y = rect.min.y + (g->size.y * dpi_scaling);
    
    jd_RectF32 clipped_rect = jd_RectClip(rect, clip_rectangle);
    
    if (clipped_rect.max.x < clipped_rect.min.x ||
        clipped_rect.max.y < clipped_rect.min.y) {
        return false;
    }
    
    f32 clip_from_left = clip_rectangle.min.x - rect.min.x;
    f32 clip_from_right = rect.max.x - clip_rectangle.max.x;
    f32 clip_from_top  = clip_rectangle.min.y - rect.min.y;
    f32 clip_from_bottom = rect.max.y - clip_rectangle.max.y;
    
    /*
Let's imagine:

Clip from left: 0.0f
Clip from right: 0.0f
Clip from top: 10.f
Clip from bottom: 0.0f

g->size = 100
dpi_scale = 1.0

*/
    
    clip_from_left = (clip_from_left > 0.0f) ? clip_from_left : 0.0f;
    clip_from_right = (clip_from_right > 0.0f) ? clip_from_right : 0.0f;
    clip_from_top = (clip_from_top > 0.0f) ? clip_from_top : 0.0f;
    clip_from_bottom = (clip_from_bottom > 0.0f) ? clip_from_bottom : 0.0f;
    
    clip_from_left /= dpi_scaling;
    clip_from_right /= dpi_scaling;
    clip_from_top /= dpi_scaling;
    clip_from_bottom /= dpi_scaling;
    
    f32 uv_factor_x = 1.0f / (f32)g->texture->res.x;
    f32 uv_factor_y = 1.0f / (f32)g->texture->res.y;
    
    f32 g_tx_max_x = (f32)g->size.x * uv_factor_x;
    f32 g_tx_max_y = (f32)g->size.y * uv_factor_y;
    
    jd_V3F txmin = { 
        0.0f + ((clip_from_left * uv_factor_x)),
        0.0f + ((clip_from_top * uv_factor_y)),
        g->layer_index
    };
    
    jd_V3F txmax = {
        g_tx_max_x - (clip_from_right * uv_factor_x),
        g_tx_max_y - (clip_from_bottom * uv_factor_y),
        g->layer_index
    };
    
    rect = clipped_rect;
    
    jd_V4F inst_rect = {
        .x0 = rect.min.x, 
        .y0 = rect.min.y,
        .x1 = rect.max.x,
        .y1 = rect.max.y
    };
    
    jd_GLVertex top_right = {
        .pos = { rect.max.x, rect.max.y, z },
        .tx = txmax,
        .col = col,
        .rect = inst_rect
    };
    
    jd_GLVertex bottom_right = {
        .pos = { rect.max.x, rect.min.y, z },
        .tx = {txmax.u, txmin.v, txmax.w},
        .col = col,
        .rect = inst_rect
    };
    
    jd_GLVertex bottom_left = {
        .pos = { rect.min.x, rect.min.y, z },
        .tx = txmin,
        .col = col,
        .rect = inst_rect
    };
    
    jd_GLVertex top_left = {
        .pos = { rect.min.x, rect.max.y, z },
        .tx = {txmin.u, txmax.v, txmax.w},
        .col = col,
        .rect = inst_rect
    };
    
    
    jd_DArray* vertices = renderer->vertices;
    jd_TextureQueue(g->texture);
    jd_DArrayPushBack(vertices, &top_right);
    jd_DArrayPushBack(vertices, &bottom_right);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &top_left);
    jd_DArrayPushBack(vertices, &top_right);
    
    
    *advance_out += g->h_advance * dpi_scaling;
    
    return true;
}



jd_V2F jd_CalcStringBoxMax(jd_String font_id, jd_V2F starting_pos, jd_UTFDecodedString utf32_string, f32 wrap_width) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_Font* font = jd_FontGetByID(font_id);
    jd_Glyph* fallback = jd_FontGetGlyph(font, '?');
    
    f32 dpi_scaling = ((f32)jd_WindowGetDPI(renderer->current_window) / (f32)font->faces[0].dpi);
    
    jd_V2F window_bb = renderer->current_window->size;
    
    f32 line_adv = font->faces[0].line_adv;
    
    jd_V2F pos = starting_pos;
    jd_V2F max = {starting_pos.x, starting_pos.y + (line_adv * dpi_scaling)};
    
    b8 wrap = (wrap_width > 0.0f);
    
    if (!wrap) {
        for (u64 i = 0; i < utf32_string.count && max.x < window_bb.x; i++) {
            if (utf32_string.utf32[i] == 0x0a) {
                max.y += font->faces[0].line_adv * dpi_scaling;
                pos.x = starting_pos.x;
                continue;
            }
            
            if (utf32_string.utf32[i] == 0x0d) {
                continue;
            }
            
            if (utf32_string.utf32[i] == 0) {
                continue;
            }
            
            jd_Glyph* g = jd_FontGetGlyph(font, utf32_string.utf32[i]);
            if (!g || g->codepoint == 0) {
                g = fallback;
            }
            
            max.x += g->h_advance * dpi_scaling;
        }
    }  else {
        for (u64 i = 0; i < utf32_string.count && max.y < window_bb.y; i++) {
            if (utf32_string.utf32[i] == 0x0a) {
                max.y += font->faces[0].line_adv * dpi_scaling;
                pos.x = starting_pos.x;
                continue;
            }
            
            if (utf32_string.utf32[i] == 0x0d) {
                continue;
            }
            
            if (utf32_string.utf32[i] == 0) {
                continue;
            }
            
            jd_Glyph* g = jd_FontGetGlyph(font, utf32_string.utf32[i]);
            if (!g || g->codepoint == 0) {
                g = fallback;
            }
            
            f32 adv = g->h_advance;
            
            if (pos.x + (adv * dpi_scaling) > wrap_width) {
                max.y += line_adv * dpi_scaling;
                pos.x = starting_pos.x;
            } else {
                pos.x += adv * dpi_scaling;
                max.x = jd_Min(window_bb.x, jd_Max(pos.x, max.x));
            }
            
        }
    }
    
    return max;
}

jd_V2F jd_CalcStringBoxMaxUTF8(jd_String font_id, jd_String str, f32 wrap_width) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_ScratchArena scratch = jd_ScratchArenaCreate(renderer->frame_arena);
    jd_UTFDecodedString utf32 = jd_UnicodeDecodeUTF8String(scratch.arena, jd_UnicodeTF_UTF32, str, false);
    jd_V2F bm = jd_CalcStringBoxMax(font_id, (jd_V2F){0.0f, 0.0f}, utf32, wrap_width);
    jd_ScratchArenaRelease(scratch);
    return bm;
}

jd_V2F jd_CalcStringSize(jd_String font_id, jd_UTFDecodedString utf32_string, f32 wrap_width) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_Font* font = jd_FontGetByID(font_id);
    jd_Glyph* fallback = jd_FontGetGlyph(font, '?');
    
    f32 dpi_scaling = ((f32)jd_WindowGetDPI(renderer->current_window) / (f32)font->faces[0].dpi);
    
    f32 line_adv = font->faces[0].line_adv;
    
    jd_V2F pos = jd_V2F(0.0, 0.0);
    jd_V2F size = jd_V2F(0.0, 0.0);
    
    b8 wrap = (wrap_width > 0.0f);
    
    if (!wrap) {
        for (u64 i = 0; i < utf32_string.count; i++) {
            if (utf32_string.utf32[i] == 0x0a) {
                continue;
            }
            
            if (utf32_string.utf32[i] == 0x0d) {
                size.y += font->faces[0].line_adv * dpi_scaling;
                pos.x = 0.0f;
                continue;
            }
            
            if (utf32_string.utf32[i] == 0) {
                continue;
            }
            
            jd_Glyph* g = jd_FontGetGlyph(font, utf32_string.utf32[i]);
            if (!g || g->codepoint == 0) {
                g = fallback;
            }
            
            size.x += g->h_advance * dpi_scaling;
            size.y = (g->face->ascent + g->face->descent) * dpi_scaling;
        }
    }  else {
        for (u64 i = 0; i < utf32_string.count; i++) {
            if (utf32_string.utf32[i] == 0x0a) {
                size.y += font->faces[0].line_adv * dpi_scaling;
                pos.x = 0.0f;
                continue;
            }
            
            if (utf32_string.utf32[i] == 0x0d) {
                
            }
            
            if (utf32_string.utf32[i] == 0) {
                continue;
            }
            
            jd_Glyph* g = jd_FontGetGlyph(font, utf32_string.utf32[i]);
            if (!g || g->codepoint == 0) {
                g = fallback;
            }
            
            f32 adv = g->h_advance;
            
            if (pos.x + (adv * dpi_scaling) > wrap_width) {
                pos.y += line_adv * dpi_scaling;
                pos.x = 0.0f;
                size.y = pos.y * dpi_scaling;
            } else {
                pos.x += adv * dpi_scaling;
                size.x = jd_Max(pos.x, size.x);
                size.y = jd_Max((g->face->ascent + g->face->descent) * dpi_scaling, size.y);
            }
        }
    }
    
    return size;
}

u64 jd_CalcCursorIndex(jd_String font_id, jd_UTFDecodedString utf32_string, f32 wrap_width, jd_V2F desired_pos) {
    u64 index = 0;
    
    jd_Renderer* renderer = jd_RendererGet();
    jd_Font* font = jd_FontGetByID(font_id);
    jd_Glyph* fallback = jd_FontGetGlyph(font, '?');
    
    jd_V2F pos = {0};
    
    f32 dpi_scaling = ((f32)jd_WindowGetDPI(renderer->current_window) / (f32)fallback->face->dpi);
    
    f32 line_adv = fallback->face->line_adv;
    
    b8 wrap = (wrap_width > 0.0f);
    
    u64 i = 0;
    
    b8 y_match_found = 0;
    
    if (!wrap) {
        for (; i < utf32_string.count; i++) {
            if (utf32_string.utf32[i] == 0x0a) {
                pos.x = 0.0f;
                pos.y += fallback->face->line_adv * dpi_scaling;
                continue;
            }
            
            if (utf32_string.utf32[i] == 0x0d) {
                continue;
            }
            
            if (utf32_string.utf32[i] == 0) {
                continue;
            }
            
            jd_Glyph* g = jd_FontGetGlyph(font, utf32_string.utf32[i]);
            if (!g || g->codepoint == 0) {
                g = fallback;
            }
            
            pos.x += g->h_advance * dpi_scaling;
            
            if (pos.x >= desired_pos.x) {
                break;
            }
        }
    }  else {
        for (; i < utf32_string.count; i++) {
            jd_Glyph* g = jd_FontGetGlyph(font, utf32_string.utf32[i]);
            if (!g || g->codepoint == 0) {
                g = fallback;
            }
            
            f32 off_x = desired_pos.x - pos.x;
            f32 off_y = desired_pos.y - pos.y;
            
            f32 off_x_abs = jd_F32Abs(off_x);
            f32 off_y_abs = jd_F32Abs(off_y);
            
            if (off_y_abs <= (line_adv * dpi_scaling * 0.5) ) {
                y_match_found = true;
            }
            
            if (i == 0 && desired_pos.y < pos.y) { // If on the first glyph we are out of bounds to the top, set the cursor to the start of the string
                i = 0;
                return i;
            }
            
            if ((off_x_abs <= (g->h_advance * dpi_scaling * 0.5)) && y_match_found) {
                jd_ClampToRange(i, 0, utf32_string.count);
                break;
            }
            
            if (utf32_string.utf32[i] == 0x0a) {
                if (y_match_found) {
                    break;
                }
                pos.x = 0.0f;
                pos.y += line_adv * dpi_scaling;
                continue;
            }
            
            if (utf32_string.utf32[i] == 0x0d) {
                continue;
            }
            
            if (utf32_string.utf32[i] == 0) {
                continue;
            }
            
            f32 adv = g->h_advance;
            
            if (pos.x + (adv * dpi_scaling) > wrap_width) {
                pos.y += line_adv * dpi_scaling;
                pos.x = 0.0f;
                
            } else {
                pos.x += adv * dpi_scaling;
            }
            
        }
    }
    
    return i;
}

jd_V2F jd_CalcCursorPos(jd_String font_id, jd_UTFDecodedString utf32_string, f32 wrap_width, u64 cursor_index) {
    u64 index = 0;
    jd_V2F pos = {0};
    
    jd_Renderer* renderer = jd_RendererGet();
    jd_Font* font = jd_FontGetByID(font_id);
    jd_Glyph* fallback = jd_FontGetGlyph(font, '?');
    
    f32 dpi_scaling = ((f32)jd_WindowGetDPI(renderer->current_window) / (f32)fallback->face->dpi);
    
    f32 line_adv = fallback->face->line_adv;
    
    jd_V2F size = jd_V2F(0.0, 0.0);
    
    b8 wrap = (wrap_width > 0.0f);
    
    b8 end = cursor_index == utf32_string.count;
    
    f32 last_char_adv = 0.0f;
    
    if (!wrap) {
        for (u64 i = 0; i < jd_Min(cursor_index, utf32_string.count); i++) {
            if (utf32_string.utf32[i] == 0x0a) {
                pos.x = 0.0f;
                pos.y += fallback->face->line_adv * dpi_scaling;
                continue;
            }
            
            if (utf32_string.utf32[i] == 0x0d) {
                continue;
            }
            if (utf32_string.utf32[i] == 0) {
                continue;
            }
            
            jd_Glyph* g = jd_FontGetGlyph(font, utf32_string.utf32[i]);
            if (!g || g->codepoint == 0) {
                g = fallback;
            }
            
            pos.x += g->h_advance * dpi_scaling;
        }
    }  else {
        for (u64 i = 0; i < jd_Min(cursor_index, utf32_string.count); i++) {
            if (utf32_string.utf32[i] == 0x0a) {
                pos.x = 0.0f;
                pos.y += line_adv * dpi_scaling;
                continue;
            }
            
            if (utf32_string.utf32[i] == 0x0d) {
                continue;
            }
            
            if (utf32_string.utf32[i] == 0) {
                continue;
            }
            
            jd_Glyph* g = jd_FontGetGlyph(font, utf32_string.utf32[i]);
            if (!g || g->codepoint == 0) {
                g = fallback;
            }
            
            f32 adv = g->h_advance;
            
            last_char_adv = adv * dpi_scaling;
            
            if (pos.x + (adv * dpi_scaling) > wrap_width) {
                pos.y += line_adv * dpi_scaling;
                pos.x = 0.0f;
                pos.x += last_char_adv;
            } else {
                pos.x += adv * dpi_scaling;
            }
        }
        
    }
    
    return pos;
}

u64 jd_CalcCursorIndexUTF8(jd_String font_id, jd_String str, f32 wrap_width, jd_V2F desired_pos) {
    jd_Renderer* renderer = jd_RendererGet();
    if (!str.count) return 0;
    jd_ScratchArena scratch = jd_ScratchArenaCreate(renderer->frame_arena);
    jd_UTFDecodedString utf32 = jd_UnicodeDecodeUTF8String(scratch.arena, jd_UnicodeTF_UTF32, str, false);
    u64 index = jd_CalcCursorIndex(font_id, utf32, wrap_width, desired_pos);
    jd_ScratchArenaRelease(scratch);
    return index;
}

jd_V2F jd_CalcCursorPosUTF8(jd_String font_id, jd_String str, f32 wrap_width, u64 cursor_index) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_ScratchArena scratch = jd_ScratchArenaCreate(renderer->frame_arena);
    jd_UTFDecodedString utf32 = jd_UnicodeDecodeUTF8String(scratch.arena, jd_UnicodeTF_UTF32, str, false);
    jd_V2F pos = jd_CalcCursorPos(font_id, utf32, wrap_width, cursor_index);
    jd_ScratchArenaRelease(scratch);
    return pos;
}

jd_V2F jd_CalcStringSizeUTF8(jd_String font_id, jd_String str, f32 wrap_width) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_ScratchArena scratch = jd_ScratchArenaCreate(renderer->frame_arena);
    jd_UTFDecodedString utf32 = jd_UnicodeDecodeUTF8String(scratch.arena, jd_UnicodeTF_UTF32, str, false);
    jd_V2F ss = jd_CalcStringSize(font_id, utf32, wrap_width);
    jd_ScratchArenaRelease(scratch);
    return ss;
}

void jd_DrawStringUTF32(jd_String font_id, jd_UTFDecodedString utf32_string, jd_V3F window_pos, jd_TextOrigin baseline, jd_V4F color, f32 wrap_width, jd_RectF32 clip_rect) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_Font* font = jd_FontGetByID(font_id);
    f32 dpi_scaling = ((f32)jd_WindowGetDPI(renderer->current_window) / (f32)font->faces[0].dpi);
    jd_Glyph* fallback = jd_FontGetGlyph(font, '?');
    jd_V3F pos = window_pos;
    
    pos.x *= renderer->dpi_scaling;
    pos.y *= renderer->dpi_scaling;
    
    jd_V2F starting_pos = {pos.x, pos.y};
    
    jd_V4F supplied_color = color;
    jd_V4F glyph_color = supplied_color;
    
    if (clip_rect.min.x == 0 &&
        clip_rect.max.x == 0 &&
        clip_rect.min.y == 0 &&
        clip_rect.max.y == 0) {
        clip_rect.min = (jd_V2F){0.0, 0.0}; 
        clip_rect.max = jd_RendererGet()->render_size;
    }
    
    b8 wrap = (wrap_width > 0.0f);
    jd_Glyph* first_glyph = jd_FontGetGlyph(font, utf32_string.utf32[0]);
    if (!first_glyph || first_glyph->codepoint == 0) {
        first_glyph = fallback;
        glyph_color = (jd_V4F){1.0, 0.0, 0.0, 1.0};
    }
    
    f32 descent = first_glyph->face->descent;
    f32 ascent  = first_glyph->face->ascent;
    switch (baseline) {
        default: return;
        
        case jd_TextOrigin_TopLeft: {
            pos.y += ((ascent + descent) * dpi_scaling);
            break;
        }
        
        case jd_TextOrigin_BottomLeft: {
            break;
        }
    }
    
    for (u64 i = 0; i < utf32_string.count; i++) {
        glyph_color = supplied_color;
        if (utf32_string.utf32[i] == 0x0a) {
            pos.x = starting_pos.x;
            pos.y += font->faces[0].line_adv * dpi_scaling;
            continue;
        }
        
        if (utf32_string.utf32[i] == 0x0d) {
            continue;
        }
        
        if (utf32_string.utf32[i] == 0) {
            continue;
        }
        
        jd_Glyph* glyph = jd_FontGetGlyph(font, utf32_string.utf32[i]);
        if (!glyph || glyph->codepoint == 0) {
            glyph = fallback;
            glyph_color = (jd_V4F){1.0, 0.0, 0.0, 1.0};
        }
        
        f32 adv = glyph->h_advance;
        
        if (wrap) {
            if ((pos.x - starting_pos.x) + (adv * dpi_scaling) > wrap_width) {
                pos.x = starting_pos.x;
                pos.y += font->faces[0].line_adv * dpi_scaling;
                //pos.y += glyph->face->descent * dpi_scaling;
            }
            
        }
        
        jd_Internal_DrawGlyph(font, pos, glyph_color, glyph, &pos.x, clip_rect);
    }
}


void jd_DrawString(jd_String font_id, jd_String str, jd_V2F window_pos, jd_TextOrigin baseline, jd_V4F color, f32 wrap_width, jd_RectF32 clip_rect) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_UTFDecodedString utf32_string = jd_UnicodeDecodeUTF8String(renderer->frame_arena, jd_UnicodeTF_UTF32, str, false);
    jd_V3F pos = {window_pos.x, window_pos.y, 0.0f};
    jd_DrawStringUTF32(font_id, utf32_string, pos, baseline, color, wrap_width, clip_rect);
}

void jd_DrawStringWithZ(jd_String font_id, jd_String str, jd_V3F window_pos, jd_TextOrigin baseline, jd_V4F color, f32 wrap_width, jd_RectF32 clip_rect) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_UTFDecodedString utf32_string = jd_UnicodeDecodeUTF8String(renderer->frame_arena, jd_UnicodeTF_UTF32, str, false);
    jd_DrawStringUTF32(font_id, utf32_string, window_pos, baseline, color, wrap_width, clip_rect);
}

#if 0
void jd_DrawStringWithBG(jd_String font_id, jd_String str, jd_V2F window_pos, jd_TextOrigin baseline, jd_V4F text_color, jd_V4F bg_color, f32 wrap_width, f32 box_rounding, f32 box_softness, f32 thickness) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_Font* font = jd_FontGetByID(font_id);
    f32 dpi_scaling = ((f32)jd_WindowGetDPI(renderer->current_window) / (f32)font->faces[0].dpi);
    jd_UTFDecodedString utf32_string = jd_UnicodeDecodeUTF8String(jd_RendererGet()->frame_arena, jd_UnicodeTF_UTF32, str, false);
    
    jd_V2F box_pos = {window_pos.x, window_pos.y};
    jd_V2F max = jd_CalcStringSize(font_id, utf32_string, wrap_width);
    
    switch (baseline) {
        default: return;
        
        case jd_TextOrigin_TopLeft: {
            break;
        }
        
        case jd_TextOrigin_BottomLeft: {
            box_pos.y -= max.y;
            break;
        }
    }
    
    jd_V3F pos = {window_pos.x, window_pos.y, 0.0f};
    jd_DrawRect(box_pos, max, bg_color, box_rounding, box_softness, thickness);
    jd_DrawStringUTF32(font_id, utf32_string, pos, baseline, text_color, wrap_width);
}
#endif

f32 jd_FontGetLineAdvForCodepoint(jd_String font_id, u32 codepoint) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_Font* font = jd_FontGetByID(font_id);
    jd_Glyph* g = jd_FontGetGlyph(font, codepoint);
    f32 dpi_scaling = ((f32)jd_WindowGetDPI(renderer->current_window) / (f32)g->face->dpi);
    f32 line_adv = g->face->line_adv * dpi_scaling;
    return line_adv;
}

f32 jd_FontGetLineHeightForCodepoint(jd_String font_id, u32 codepoint) {
    jd_Renderer* renderer = jd_RendererGet();
    jd_Font* font = jd_FontGetByID(font_id);
    jd_Glyph* g = jd_FontGetGlyph(font, codepoint);
    f32 dpi_scaling = ((f32)jd_WindowGetDPI(renderer->current_window) / (f32)g->face->dpi);
    f32 height = g->face->ascent + g->face->descent;
    return height;
}

void jd_DrawRectWithZ(jd_V3F window_pos, jd_V2F size, jd_V4F col, f32 rounding, f32 softness, f32 thickness, jd_RectF32 clip_rectangle) {
    jd_Renderer* renderer = jd_RendererGet();
    f32 x = window_pos.x * renderer->dpi_scaling;
    f32 y = window_pos.y * renderer->dpi_scaling;
    f32 z = window_pos.z;
    
    f32 dpi_scaling = renderer->dpi_scaling;
    
    jd_RectF32 rect = {0};
    rect.min.x = x;
    rect.min.y = y;
    rect.max.x = x + (size.x * dpi_scaling);
    rect.max.y = y + (size.y * dpi_scaling);
    
    jd_RectF32 clipped_rect = jd_RectClip(rect, clip_rectangle);
    
    if (clipped_rect.max.x < clipped_rect.min.x ||
        clipped_rect.max.y < clipped_rect.min.y) {
        return;
    }
    
    rect = clipped_rect;
    
    if (jd_internal_rectangle_texture == 0) { // realistically this should pretty much never occur
        jd_2DTexture* tex = jd_TexturePoolAddTexture((jd_V2U){256, 256}, 4, jd_2DTextureMode_Font);
        jd_internal_rectangle_texture = tex;
        u8* white_bitmap = jd_HeapAlloc(sizeof(u32) * 256 * 256);
        jd_MemSet(white_bitmap, 0xFF, sizeof(u32) * 256 * 256);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 256, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, white_bitmap);
        tex->used_slots++;
    }
    
    
    jd_V4F inst_rect = {
        .x0 = rect.min.x, 
        .y0 = rect.min.y,
        .x1 = rect.max.x,
        .y1 = rect.max.y
    };
    
    
    jd_GLVertex top_right = {
        .pos = { rect.max.x, rect.max.y, z },
        .tx = {1.0f, 1.0f, 0.0f},
        .col = col,
        .rect = inst_rect,
        .rounding = rounding * dpi_scaling,
        .softness = softness * dpi_scaling,
        .thickness = thickness * dpi_scaling
    };
    
    jd_GLVertex bottom_right = {
        .pos = { rect.max.x, rect.min.y, z },
        .tx = {1.0f, 0.0f, 0.0f},
        .col = col,
        .rect = inst_rect,
        .rounding = rounding * dpi_scaling,
        .softness = softness * dpi_scaling,
        .thickness = thickness * dpi_scaling
    };
    
    jd_GLVertex bottom_left = {
        .pos = { rect.min.x, rect.min.y, z },
        .tx = {0.0f, 0.0f, 0.0f},
        .col = col,
        .rect = inst_rect,
        .rounding = rounding * dpi_scaling,
        .softness = softness * dpi_scaling,
        .thickness = thickness * dpi_scaling
    };
    
    jd_GLVertex top_left = {
        .pos = { rect.min.x, rect.max.y, z },
        .tx = {0.0f, 1.0f, 0.0f},
        .col = col,
        .rect = inst_rect,
        .rounding = rounding * dpi_scaling,
        .softness = softness * dpi_scaling,
        .thickness = thickness * dpi_scaling
    };
    
    jd_DArray* vertices = renderer->vertices;
    jd_TextureQueue(jd_internal_rectangle_texture);
    jd_DArrayPushBack(vertices, &top_right);
    jd_DArrayPushBack(vertices, &bottom_right);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &top_left);
    jd_DArrayPushBack(vertices, &top_right);
}

void jd_DrawRect(jd_V2F window_pos, jd_V2F size, jd_V4F col, f32 rounding, f32 softness, f32 thickness) {
    jd_Renderer* renderer = jd_RendererGet();
    f32 x = window_pos.x * renderer->dpi_scaling;
    f32 y = window_pos.y * renderer->dpi_scaling;
    f32 z = 0.0f;
    
    f32 dpi_scaling = renderer->dpi_scaling;
    
    jd_RectF32 rect = {0};
    rect.min.x = x;
    rect.min.y = y;
    rect.max.x = x + (size.x * dpi_scaling);
    rect.max.y = y + (size.y * dpi_scaling);
    
    if (jd_internal_rectangle_texture == 0) { // realistically this should pretty much never occur
        jd_2DTexture* tex = jd_TexturePoolAddTexture((jd_V2U){256, 256}, 4, jd_2DTextureMode_Font);
        jd_internal_rectangle_texture = tex;
        u8* white_bitmap = jd_HeapAlloc(sizeof(u32) * 256 * 256);
        jd_MemSet(white_bitmap, 0xFF, sizeof(u32) * 256 * 256);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 256, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, white_bitmap);
        tex->used_slots++;
    }
    
    jd_V4F inst_rect = {
        .x0 = rect.min.x, 
        .y0 = rect.min.y,
        .x1 = rect.max.x,
        .y1 = rect.max.y
    };
    
    
    jd_GLVertex top_right = {
        .pos = { rect.max.x, rect.max.y, z },
        .tx = {1.0f, 1.0f, 0.0f},
        .col = col,
        .rect = inst_rect,
        .rounding = rounding * dpi_scaling,
        .softness = softness * dpi_scaling,
        .thickness = thickness * dpi_scaling
    };
    
    jd_GLVertex bottom_right = {
        .pos = { rect.max.x, rect.min.y, z },
        .tx = {1.0f, 0.0f, 0.0f},
        .col = col,
        .rect = inst_rect,
        .rounding = rounding * dpi_scaling,
        .softness = softness * dpi_scaling,
        .thickness = thickness * dpi_scaling
    };
    
    jd_GLVertex bottom_left = {
        .pos = { rect.min.x, rect.min.y, z },
        .tx = {0.0f, 0.0f, 0.0f},
        .col = col,
        .rect = inst_rect,
        .rounding = rounding * dpi_scaling,
        .softness = softness * dpi_scaling,
        .thickness = thickness * dpi_scaling
    };
    
    jd_GLVertex top_left = {
        .pos = { rect.min.x, rect.max.y, z },
        .tx = {0.0f, 1.0f, 0.0f},
        .col = col,
        .rect = inst_rect,
        .rounding = rounding * dpi_scaling,
        .softness = softness * dpi_scaling,
        .thickness = thickness * dpi_scaling
    };
    
    jd_DArray* vertices = renderer->vertices;
    jd_TextureQueue(jd_internal_rectangle_texture);
    jd_DArrayPushBack(vertices, &top_right);
    jd_DArrayPushBack(vertices, &bottom_right);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &top_left);
    jd_DArrayPushBack(vertices, &top_right);
}

void jd_RendererInit() {
    jd_Arena* arena = jd_ArenaCreate(0, 0);
    jd_Renderer* renderer = jd_global_renderer = jd_ArenaAlloc(arena, sizeof(*renderer));
    renderer->arena = arena;
    renderer->frame_arena = jd_ArenaCreate(0, 0);
    renderer->dpi_scaling = 1.0f;
    renderer->vertices = jd_DArrayCreate(MEGABYTES(64) / sizeof(jd_GLVertex), sizeof(jd_GLVertex));
    jd_RenderObjects* objects = &renderer->objects;
    jd_ShaderCreate(renderer);
    
    u32 max_texture_units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
    
    u32 max_texture_depth = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_texture_depth);
    
    renderer->max_texture_depth = max_texture_depth;
    renderer->max_texture_units = max_texture_units;
    
    glGenVertexArrays(1, &objects->vao);
    glGenBuffers(1, &objects->vbo);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    
    glBindVertexArray(objects->vao);
    glBindBuffer(GL_ARRAY_BUFFER, objects->vbo);
    
    u64 pos = 0;
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(jd_GLVertex), (void*)pos);
    pos += sizeof(jd_V3F);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(jd_GLVertex), (void*)pos);
    pos += sizeof(jd_V3F);
    
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(jd_GLVertex), (void*)pos);
    pos += sizeof(jd_V4F);
    
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(jd_GLVertex), (void*)pos);
    pos += sizeof(jd_V4F);
    
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(jd_GLVertex), (void*)pos);
    pos += sizeof(f32);
    
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(jd_GLVertex), (void*)pos);
    pos += sizeof(f32);
    
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(jd_GLVertex), (void*)pos);
    pos += sizeof(f32);
    
    //glBindBuffer(GL_ARRAY_BUFFER, objects->vbo);
    glBufferData(GL_ARRAY_BUFFER, MEGABYTES(1) * sizeof(jd_GLVertex), NULL, GL_DYNAMIC_DRAW);
    //glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void jd_RendererSetDPIScale(jd_Renderer* renderer, f32 scale) {
    b32 font_refresh = false;
    if (scale != renderer->dpi_scaling) font_refresh = true;
}

void jd_RendererBegin(jd_V2F render_size) {
    jd_RendererGet()->render_size = render_size;
    glViewport(0, 0, render_size.w, render_size.h);
    glClearColor(0.33f, 0.33f, 0.33f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

void jd_RendererDraw() {
    jd_Renderer* renderer = jd_RendererGet();
    jd_V2F render_size = renderer->render_size;
    
    glUseProgram(renderer->objects.shader);
    
    i32 tex_loc = glGetUniformLocation(renderer->objects.shader, "tex");
    glUniform1i(tex_loc, 0);
    
    i32 screen_conv_location = glGetUniformLocation(renderer->objects.shader, "screen_conv");
    glUniform2f(screen_conv_location, 2.0f / render_size.w, 2.0f / render_size.h);
    
    glBindVertexArray(renderer->objects.vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->objects.vbo);
    
#if 0    
    jd_2DTexture* tex = renderer->render_chain;
    jd_2DTexture* last_tex = 0;
    jd_ForDLLForward(tex, tex != 0) {
        if (last_tex) {
            jd_DLinksClear(last_tex);
        }
        jd_DArray* vertices = tex->vertices;
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices->count * sizeof(jd_GLVertex), vertices->view.mem);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex->gl_index);
        glDrawArrays(GL_TRIANGLES, 0, vertices->count);
        jd_DArrayClearNoDecommit(vertices);
        last_tex = tex;
    }
    
    renderer->render_chain = 0;
#endif
    
    jd_DArray* vertices = renderer->vertices;
    
    jd_DrawGroup* dg = renderer->draw_group_chain_head;
    jd_DrawGroup* last_dg = 0;
    jd_ForDLLForward(dg, dg != 0) {
        if (last_dg) {
            jd_DLinksClear(last_dg);
        }
        
        u64 size = 0;
        if (dg->next) {
            size = dg->next->index - dg->index;
        } else {
            size = vertices->count - dg->index;
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, size * sizeof(jd_GLVertex), vertices->view.mem + (dg->index * sizeof(jd_GLVertex)));
        glBindTexture(GL_TEXTURE_2D_ARRAY, dg->tex->gl_index);
        glDrawArrays(GL_TRIANGLES, 0, size);
        last_dg = dg;
    }
    
    jd_DArrayClearNoDecommit(vertices);
    renderer->draw_group_chain_head  = 0;
    renderer->draw_group_chain_tail  = 0;
    
    jd_ArenaPopTo(renderer->frame_arena, 0);
}
