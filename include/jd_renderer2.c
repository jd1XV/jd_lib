#define STB_IMAGE_IMPLEMENTATION
#include "dep/stb/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "dep/stb/stb_image_resize2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "dep/stb/stb_image_write.h"

static jd_ReadOnly jd_String jd_internal_vs_string = jd_StrConst("#version 460\n"
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

static jd_ReadOnly jd_String jd_internal_fs_string = jd_StrConst("#version 460\n"
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
                                                                 "    if (fs_rounding != 0 && fs_softness == 0) {\n"
                                                                 "       softness = 1.0;\n"
                                                                 "    }\n"
                                                                 "    vec2 softness_padding = vec2(max(0, (softness * 2 - 1)), max(0, (softness * 2 - 1)));\n"
                                                                 "    \n"
                                                                 "    vec2 p0 = vec2(fs_rect.x, fs_rect.y);\n"
                                                                 "    vec2 p1 = vec2(fs_rect.z, fs_rect.w);\n"
                                                                 "    \n"
                                                                 "    vec2 rect_center = vec2(p1 + p0) / 2;\n"
                                                                 "    vec2 rect_half_size = vec2(p1 - p0) / 2;\n"
                                                                 "    \n"
                                                                 "    float dist = RoundingSDF(fs_xyz.xy, rect_center, rect_half_size - softness_padding, fs_rounding);\n"
                                                                 "    \n"
                                                                 "    float border_factor = 1.0f;\n"
                                                                 "    \n"
                                                                 "    if (fs_thickness > 0) {\n"
                                                                 "        vec2 interior_half_size = rect_half_size - vec2(fs_thickness);\n"
                                                                 "        \n"
                                                                 "        float interior_radius_reduce = min(interior_half_size.x / rect_half_size.x,\n"
                                                                 "                                           interior_half_size.y / rect_half_size.y);\n"
                                                                 "        float interior_corner_radius = (fs_rounding * interior_radius_reduce);\n"
                                                                 "        \n"
                                                                 "        float inside_d = RoundingSDF(fs_xyz.xy, rect_center, interior_half_size - softness_padding, interior_corner_radius);\n"
                                                                 "        \n"
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

typedef struct jd_CachedTexture {
    u32 key;
    jd_V2I source_size;
    jd_V2I dest_size;
    
    struct jd_CachedTextureSlot* gpu_slot;
    struct jd_CachedTexture* hash_chain;
    struct jd_TextureCache* cache;
} jd_CachedTexture;

#define jd_CachedTextureAtlas_MaxGPUTextures 8

typedef struct jd_CachedTextureSlot {
    jd_CachedTexture* bound;
    u32 layer;
    jd_V2U offset;
    jd_V2U size;
    
    jd_Node(jd_CachedTextureSlot);
} jd_CachedTextureSlot;

typedef struct jd_TextureCache {
    jd_Arena* arena;
    
    u32 gl_index;
    
    u32 width;
    u32 height;
    u32 cell_width;
    u32 cell_height;
    u32 depth;
    
    jd_CachedTextureSlot* gpu_slots;
    jd_CachedTextureSlot* lru;
    jd_CachedTextureSlot* mru;
    u32 free_slots;
    u32 max_slots;
    
    jd_CachedTexture* hash_table;
    u64 hash_table_count;
} jd_TextureCache;

typedef struct jd_2DRenderObjects {
    u32 vao;
    u32 vbo;
    u32 shader;
    u32 fbo;
    u32 tcb;
    u32 rbo;
    u32 fbo_tex;
} jd_2DRenderObjects;

typedef struct jd_2DDrawGroup {
    u64 vertices_index;
    u32 gl_texture;
    
    jd_Node(jd_2DDrawGroup);
} jd_2DDrawGroup;

typedef struct jd_GL2DVertex {
    jd_V3F pos;
    jd_V3F tx;
    jd_V4F color;
    jd_V4F rect;
    f32 corner_rad;
    f32 softness;
    f32 thickness;
} jd_GL2DVertex;

typedef struct jd_2DRenderer {
    jd_Arena* arena;
    jd_Arena* frame_arena;
    jd_DArray* vertices; // type: jd_GL2DVertex
    jd_2DRenderObjects objects;
    
    jd_Window* bound_window;
    
    jd_V2I viewport_size;
    jd_2DDrawGroup* draw_group_chain_first;
    jd_2DDrawGroup* draw_group_chain_last;
    
    jd_TextureCache* core_texture_cache;
    jd_Texture rectangle_texture;
} jd_2DRenderer;

static jd_2DRenderer jd_global_2d_renderer = {0};

void jd_2DShaderCreate() {
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
    
    jd_global_2d_renderer.objects.shader = id;
}


jd_TextureCache* jd_TextureCacheCreate(jd_Arena* arena, u32 width, u32 height, u32 depth, u32 cell_width, u32 cell_height) {
    jd_TextureCache* cache = jd_ArenaAlloc(arena, sizeof(*cache));
    cache->arena = arena;
    cache->cell_width = cell_width;
    cache->cell_height = cell_height;
    cache->width = width;
    cache->height = height;
    cache->depth = depth;
    
    if (width < cell_width || height < cell_height) {
        jd_LogError("Requested texture cache size smaller than cell size", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    u32 cells_wide = width / cell_width;
    u32 cells_high = height / cell_height;
    u32 max_slots = (cells_wide * cells_high) * depth;
    cache->max_slots = max_slots;
    cache->free_slots = max_slots;
    cache->hash_table_count = max_slots * 2;
    cache->hash_table = jd_ArenaAlloc(arena, sizeof(jd_CachedTexture) * cache->hash_table_count);
    cache->gpu_slots = jd_ArenaAlloc(arena, sizeof(jd_CachedTextureSlot) * max_slots);
    
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;
    
    for (u64 i = 0; i < max_slots; i++) {
        if (x + cell_width > width) {
            x = 0;
            y += cell_height;
        }
        
        if (y + cell_height > height) {
            x = 0;
            y = 0;
            z++;
        }
        
        cache->gpu_slots[i].offset.x = x;
        cache->gpu_slots[i].offset.y = y;
        cache->gpu_slots[i].size.x = cell_width;
        cache->gpu_slots[i].size.y = cell_height;
        cache->gpu_slots[i].layer = z;
        
        x += cell_width;
    }
    
    glGenTextures(1, &cache->gl_index);
    glBindTexture(GL_TEXTURE_2D_ARRAY, cache->gl_index);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, width, height, depth);
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1);
    
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    
    return cache;
}

jd_ReadOnly static u32 _jd_renderer_default_seed = 104720809;

static void jd_CachedTextureAlphaToRGBA(u8* src, u64 src_size, u8* dst) {
    for (u64 i = 0; i < src_size; i++) {
        dst[i * 4] = 0xff;
        dst[1 + (i * 4)] = 0xff;
        dst[2 + (i * 4)] = 0xff;
        dst[3 + (i * 4)] = src[i];
    }
}

u32 jd_TextureKey(jd_String s) {
    return jd_HashStrToU32(s, _jd_renderer_default_seed);
}

void jd_TextureCacheCopyTexture(jd_CachedTexture* t, jd_CachedTextureSlot* slot, u8* bitmap, b32 monochrome, u32 width, u32 height, b32 fit) {
    jd_TextureCache* cache = t->cache;
    jd_ScratchArena s = jd_ScratchArenaCreate(cache->arena);
    
    u8* transfer_bitmap = 0;
    
    if (monochrome) {
        u8* swizzle_bitmap = jd_ArenaAlloc(s.arena, width * height * 4);
        jd_CachedTextureAlphaToRGBA(bitmap, width * height, swizzle_bitmap);
        bitmap = swizzle_bitmap;
    }
    
    jd_V2F transfer_size = {0};
    
    if (fit) {
        b32 portrait = (width < height);
        if (portrait) {
            t->dest_size.y = cache->cell_height;
            f32 scale_factor = (f32)t->dest_size.y / (f32)height;
            t->dest_size.x = width * scale_factor;
        } else {
            t->dest_size.x = cache->cell_width;
            f32 scale_factor = (f32)t->dest_size.x / (f32)width;
            t->dest_size.y = height * scale_factor;
        }
        
        u8* new_bitmap = jd_ArenaAlloc(s.arena, 4 * (t->dest_size.x * t->dest_size.y));
        stbir_resize_uint8_linear(bitmap, width, height, width * 4, new_bitmap, t->dest_size.x, t->dest_size.y, t->dest_size.x * 4, STBIR_RGBA_PM);
        transfer_bitmap = new_bitmap;
    } else {
        transfer_bitmap = bitmap;
        t->dest_size.y = jd_Min(height, cache->cell_height);
        t->dest_size.x = jd_Min(width, cache->cell_width);
    }
    
    glBindTexture(GL_TEXTURE_2D_ARRAY, cache->gl_index);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, slot->offset.x, slot->offset.y, slot->layer, t->dest_size.x, t->dest_size.y, 1, GL_RGBA, GL_UNSIGNED_BYTE, transfer_bitmap);
    //glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    
    jd_ScratchArenaRelease(s);
}

b32 jd_TextureCacheCheck(jd_TextureCache* cache, u32 key, jd_Texture* texture) {
    jd_CachedTexture* t = &cache->hash_table[key % cache->hash_table_count];
    
    if (t->key == 0) {
        return false;
    }
    
    while (t->key != key && t->hash_chain != 0) {
        t = t->hash_chain;
    }
    
    if (t->key != key) {
        return false;
    }
    
    jd_CachedTextureSlot* slot = t->gpu_slot;
    
    if (slot) {
        jd_Texture return_tex = {
            .index = cache->gl_index
        };
        
        return_tex.uvw_max.u = (f32)(slot->offset.x + t->dest_size.x) / (f32)cache->width;
        return_tex.uvw_max.v = (f32)(slot->offset.y + t->dest_size.y) / (f32)cache->height;
        return_tex.uvw_max.w = slot->layer;
        
        return_tex.uvw_min.u = (slot->offset.x > 0) ? ((f32)slot->offset.x / (f32)cache->width) : 0;
        return_tex.uvw_min.v = (slot->offset.y > 0) ? ((f32)slot->offset.y / (f32)cache->width) : 0;
        return_tex.uvw_min.w = slot->layer;
        
        *texture = return_tex;
        return true;
    } else {
        return false;
    }
    
}

jd_Texture jd_TextureCacheInsert(jd_TextureCache* cache, b8 perm, u32 key, u8* bitmap, b32 monochrome, u32 width, u32 height, b32 fit) {
    jd_CachedTexture* t = &cache->hash_table[key % cache->hash_table_count];
    
    if (t->key == 0) {
        t->key = key;
        t->source_size.x = width;
        t->source_size.y = height;
    } 
    
    while (t->key != key && t->hash_chain != 0) {
        t = t->hash_chain;
    }
    
    if (t->key != key) {
        t->hash_chain = jd_ArenaAlloc(cache->arena, sizeof(jd_CachedTexture));
        t = t->hash_chain;
        t->key = key;
    }
    
    t->cache = cache;
    
    jd_CachedTextureSlot* slot = t->gpu_slot;
    if (!slot) {
        if (cache->free_slots > 0) {
            slot = &cache->gpu_slots[cache->max_slots - cache->free_slots];
            cache->free_slots--;
        } else {
            slot = cache->lru;
            if (!slot) {
                jd_LogError("No more texture cache slots. Allocate more slots or allocate fewer permanent textures.", jd_Error_OutOfMemory, jd_Error_Fatal);
            }
            cache->lru = slot->next;
            if (slot->next) {
                slot->next->prev = 0;
            }
        }
        
        slot->bound = t;
        t->gpu_slot = slot;
        jd_TextureCacheCopyTexture(t, slot, bitmap, monochrome, width, height, fit);
        
    }
    
    if (!perm) {
        if (!cache->mru) {
            cache->mru = slot;
            cache->lru = slot;
        } else if (cache->mru != slot) {
            jd_DLinkNext(cache->mru, slot);
            cache->mru = slot;
        }
    }
    
    jd_Texture return_tex = {
        .index = cache->gl_index,
    };
    
    return_tex.uvw_max.u = (f32)(slot->offset.x + t->dest_size.x) / (f32)cache->width;
    return_tex.uvw_max.v = (f32)(slot->offset.y + t->dest_size.y) / (f32)cache->height;
    return_tex.uvw_max.w = slot->layer;
    
    return_tex.uvw_min.u = (slot->offset.x > 0) ? ((f32)slot->offset.x / (f32)cache->width) : 0;
    return_tex.uvw_min.v = (slot->offset.y > 0) ? ((f32)slot->offset.y / (f32)cache->width) : 0;
    return_tex.uvw_min.w = slot->layer;
    
    return return_tex;
}

void jd_2DRendererInit() {
    jd_Arena* arena = jd_ArenaCreate(0, 0);
    jd_2DRenderer* r = &jd_global_2d_renderer;
    r->arena = arena;
    r->frame_arena = jd_ArenaCreate(0, 0);
    r->vertices = jd_DArrayCreate(KILOBYTES(512) * sizeof(jd_GL2DVertex), sizeof(jd_GL2DVertex));
    jd_2DRenderObjects* objects = &r->objects;
    jd_2DShaderCreate();
    
    u32 max_texture_units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
    
    u32 max_texture_depth = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_texture_depth);
    
    glGenVertexArrays(1, &objects->vao);
    glGenBuffers(1, &objects->vbo);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
#if 0    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
#endif
    
    glBindVertexArray(objects->vao);
    glBindBuffer(GL_ARRAY_BUFFER, objects->vbo);
    
    u64 pos = 0;
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(jd_GL2DVertex), (void*)pos);
    pos += sizeof(jd_V3F);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(jd_GL2DVertex), (void*)pos);
    pos += sizeof(jd_V3F);
    
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(jd_GL2DVertex), (void*)pos);
    pos += sizeof(jd_V4F);
    
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(jd_GL2DVertex), (void*)pos);
    pos += sizeof(jd_V4F);
    
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(jd_GL2DVertex), (void*)pos);
    pos += sizeof(f32);
    
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(jd_GL2DVertex), (void*)pos);
    pos += sizeof(f32);
    
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(jd_GL2DVertex), (void*)pos);
    pos += sizeof(f32);
    
    glBufferData(GL_ARRAY_BUFFER, KILOBYTES(512) * sizeof(jd_GL2DVertex), NULL, GL_DYNAMIC_DRAW);
    
    u64 core_texture_cache_w = 2048;
    u64 core_texture_cache_h = 2048;
    u64 core_texture_cache_d = 4;
    u64 core_texture_cache_cw = 128;
    u64 core_texture_cache_ch = 256;
    
    r->core_texture_cache = jd_TextureCacheCreate(arena, core_texture_cache_w, core_texture_cache_h, core_texture_cache_d, core_texture_cache_cw, core_texture_cache_ch);
    u8* bitmap = jd_ArenaAlloc(r->frame_arena, sizeof(u32) * (core_texture_cache_cw * core_texture_cache_ch));
    jd_MemSet(bitmap, 0xFF, sizeof(u32) * (core_texture_cache_cw * core_texture_cache_ch));
    r->rectangle_texture = jd_TextureCacheInsert(r->core_texture_cache, true, jd_TextureKey(jd_StrLit("jd_internal_rectangle_texture")), bitmap, false, core_texture_cache_cw, core_texture_cache_ch, true);
}

void jd_2DRendererBindWindow(jd_Window* window) {
    jd_global_2d_renderer.bound_window = window;
}

void jd_2DRendererBegin(jd_Window* window) {
    glViewport(0, 0, window->size.x, window->size.y);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

jd_ForceInline void jd_DGQueue(jd_Texture t) {
    jd_2DRenderer* r = &jd_global_2d_renderer;
    if (r->draw_group_chain_last && r->draw_group_chain_last->gl_texture == t.index) {
        return;
    }
    
    jd_2DDrawGroup* g = jd_ArenaAlloc(r->frame_arena, sizeof(jd_2DDrawGroup));
    g->gl_texture = t.index;
    g->vertices_index = r->vertices->count;
    
    if (!r->draw_group_chain_first) {
        r->draw_group_chain_first = g;
        r->draw_group_chain_last = g;
    } else {
        jd_DLinkNext(r->draw_group_chain_last, g);
        r->draw_group_chain_last = g;
    }
}

void jd_2DColorRect(jd_V2F position, jd_V2F size, jd_V4F color, f32 corner_rad, f32 softness, f32 thickness, jd_V4F clip) {
    f32 x = position.x;
    f32 y = position.y;
    f32 z = 0;
    
    jd_V4F rect = {
        .x0 = position.x,
        .y0 = position.y,
        .x1 = position.x + size.x,
        .y1 = position.y + size.y
    };
    
    jd_Texture t = jd_global_2d_renderer.rectangle_texture;
    jd_DGQueue(t);
    
    jd_GL2DVertex top_right = {
        .pos = {rect.x1, rect.y1, z},
        .tx = t.uvw_max,
        .color = color,
        .rect = rect,
        .corner_rad = corner_rad,
        .softness = softness,
        .thickness = thickness
    };
    
    jd_GL2DVertex bottom_right = {
        .pos = {rect.x1, rect.y0, z},
        .tx = {t.uvw_max.u, t.uvw_min.v, t.uvw_max.w},
        .color = color,
        .rect = rect,
        .corner_rad = corner_rad,
        .softness = softness,
        .thickness = thickness
    };
    
    jd_GL2DVertex bottom_left = {
        .pos = {rect.x0, rect.y0, z},
        .tx = t.uvw_min,
        .color = color,
        .rect = rect,
        .corner_rad = corner_rad,
        .softness = softness,
        .thickness = thickness
    };
    
    jd_GL2DVertex top_left = {
        .pos = {rect.x0, rect.y1, z},
        .tx = {t.uvw_min.u, t.uvw_max.v, t.uvw_max.w},
        .color = color,
        .rect = rect,
        .corner_rad = corner_rad,
        .softness = softness,
        .thickness = thickness
    };
    
    jd_DArray* vertices = jd_global_2d_renderer.vertices;
    jd_DArrayPushBack(vertices, &top_right);
    jd_DArrayPushBack(vertices, &bottom_right);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &top_left);
    jd_DArrayPushBack(vertices, &top_right);
}

jd_V4F jd_2DRectClip(jd_V4F rect, jd_V4F clip) {
    jd_V4F clipped = {
        .min = {jd_Max(rect.min.x, clip.min.x), jd_Max(rect.min.y, clip.min.y)},
        .max = {jd_Min(rect.max.x, clip.max.x), jd_Min(rect.max.y, clip.max.y)}
    };
    
    return clipped;
}

void jd_2DTextureClip(jd_Texture* t, jd_V4F original, jd_V4F clipped) {
    jd_V3F uvw_min = t->uvw_min;
    jd_V3F uvw_max = t->uvw_max;
    
    jd_V2F dim = {original.max.x - original.min.x, original.max.y - original.min.y};
    jd_V2F uv_dim = {uvw_max.u - uvw_min.u, uvw_max.v - uvw_min.v};
    
    if (clipped.min.x > original.min.x) { // clipped from left
        f32 diff = clipped.min.x - original.min.x;
        f32 pct = diff / dim.x;
        uvw_min.u += (uv_dim.x * pct);
    }
    
    if (clipped.min.y > original.min.y) { // clipped from top
        f32 diff = clipped.min.y - original.min.y;
        f32 pct = diff / dim.y;
        uvw_min.v +=  (uv_dim.y * pct);
    }
    
    if (clipped.max.x < original.max.x) { // clipped from right
        f32 diff = original.max.x - clipped.max.x;
        f32 pct = diff / dim.x;
        uvw_max.u -= (uv_dim.x * pct);
    }
    
    if (clipped.max.y < original.max.y) { // clipped from bottom
        f32 diff = original.max.y - clipped.max.y;
        f32 pct = diff / dim.y;
        uvw_max.v -= (uv_dim.y * pct);
    }
    
    t->uvw_min = uvw_min;
    t->uvw_max = uvw_max;
}

void jd_2DTextureRect(jd_Texture t, jd_V2F position, jd_V2F size, f32 corner_rad, f32 softness, f32 thickness, jd_V4F clip) {
    f32 x = position.x;
    f32 y = position.y;
    f32 z = 0;
    
    jd_V4F rect = {
        .x0 = position.x,
        .y0 = position.y,
        .x1 = position.x + size.x,
        .y1 = position.y + size.y
    };
    
    jd_V4F original_rect = rect;
    
    if (!(clip.max.x == 0 && clip.max.y == 0 && clip.min.x == 0 && clip.min.y == 0)) {
        jd_V4F clipped_rect = jd_2DRectClip(rect, clip);
        if (clipped_rect.max.x <= clipped_rect.min.x ||
            clipped_rect.max.y <= clipped_rect.min.y) {
            return;
        }
        jd_2DTextureClip(&t, rect, clip);
        rect = clipped_rect;
    }
    
    jd_DGQueue(t);
    
    jd_V4F color = {1.0f, 1.0f, 1.0f, 1.0f};
    
    jd_GL2DVertex top_right = {
        .pos = {rect.x1, rect.y1, z},
        .tx = t.uvw_max,
        .color = color,
        original_rect,
        .corner_rad = corner_rad,
        .softness = softness,
        .thickness = thickness
    };
    
    jd_GL2DVertex bottom_right = {
        .pos = {rect.x1, rect.y0, z},
        .tx = {t.uvw_max.u, t.uvw_min.v, t.uvw_max.w},
        .color = color,
        original_rect,
        .corner_rad = corner_rad,
        .softness = softness,
        .thickness = thickness
    };
    
    jd_GL2DVertex bottom_left = {
        .pos = {rect.x0, rect.y0, z},
        .tx = t.uvw_min,
        .color = color,
        .rect = original_rect,
        .corner_rad = corner_rad,
        .softness = softness,
        .thickness = thickness
    };
    
    jd_GL2DVertex top_left = {
        .pos = {rect.x0, rect.y1, z},
        .tx = {t.uvw_min.u, t.uvw_max.v, t.uvw_max.w},
        .color = color,
        .rect = original_rect,
        .corner_rad = corner_rad,
        .softness = softness,
        .thickness = thickness
    };
    
    jd_DArray* vertices = jd_global_2d_renderer.vertices;
    jd_DArrayPushBack(vertices, &top_right);
    jd_DArrayPushBack(vertices, &bottom_right);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &top_left);
    jd_DArrayPushBack(vertices, &top_right);
}

jd_ForceInline u32 jd_GlyphTextureID(jd_Font2* font, u16 point_size, u32 codepoint) {
    u32 seed_hash = jd_HashU32toU32(point_size, _jd_renderer_default_seed);
    u32 to_hash = (codepoint << 11) + font->handle;
    u32 key = jd_HashU32toU32(to_hash + 1, seed_hash);
    return key;
}

jd_Texture jd_GlyphGetTexture(jd_Font2* font, u32 codepoint, u16 point_size) {
    jd_Texture t = {0};
    
    b32 exists = jd_TextureCacheCheck(jd_global_2d_renderer.core_texture_cache, jd_GlyphTextureID(font, point_size, codepoint), &t);
    if (!exists) {
        jd_Bitmap bitmap = jd_FontGetGlyphBitmap(jd_global_2d_renderer.frame_arena, font, codepoint, point_size);
        if (!bitmap.bitmap) {
            codepoint = 0;
            b32 fallback_exists = jd_TextureCacheCheck(jd_global_2d_renderer.core_texture_cache, jd_GlyphTextureID(font, point_size, codepoint), &t);
            if (!fallback_exists) {
                bitmap = jd_FontGetGlyphBitmap(jd_global_2d_renderer.frame_arena, font, codepoint, point_size);
                t = jd_TextureCacheInsert(jd_global_2d_renderer.core_texture_cache, false, jd_GlyphTextureID(font, point_size, codepoint), bitmap.bitmap, false, bitmap.width, bitmap.height, false);
            }
        } else {
            t = jd_TextureCacheInsert(jd_global_2d_renderer.core_texture_cache, false, jd_GlyphTextureID(font, point_size, codepoint), bitmap.bitmap, false, bitmap.width, bitmap.height, false);
        }
    }
    
    return t;
}

void jd_2DGlyphRect(jd_Font2* font, u32 codepoint, u16 point_size, jd_V2F position, jd_V4F color, jd_V4F clip, f32* out_advance) {
    f32 x = position.x;
    f32 y = position.y;
    f32 z = 0;
    
    jd_Texture t = jd_GlyphGetTexture(font, codepoint, point_size);
    
    jd_GlyphMetrics* g_metrics = jd_FontGetGlyphMetrics(font, codepoint);
    
    u32 height = (font->ascent + font->descent) * point_size;
    u32 width  = (g_metrics->h_advance * point_size);
    
    jd_V4F rect = {
        .x0 = position.x,
        .y0 = position.y,
        .x1 = position.x + width,
        .y1 = position.y + height
    };
    
    jd_V4F original_rect = rect;
    
    if (!(clip.max.x == 0 && clip.max.y == 0 && clip.min.x == 0 && clip.min.y == 0)) {
        jd_V4F clipped_rect = jd_2DRectClip(rect, clip);
        if (clipped_rect.max.x < clipped_rect.min.x ||
            clipped_rect.max.y < clipped_rect.min.y) {
            *out_advance += width;
            return;
        }
        jd_2DTextureClip(&t, rect, clipped_rect);
        rect = clipped_rect;
    }
    
    jd_DGQueue(t);
    
    jd_GL2DVertex top_right = {
        .pos = {rect.x1, rect.y1, z},
        .tx = t.uvw_max,
        .color = color,
        .rect = original_rect,
    };
    
    jd_GL2DVertex bottom_right = {
        .pos = {rect.x1, rect.y0, z},
        .tx = {t.uvw_max.u, t.uvw_min.v, t.uvw_max.w},
        .color = color,
        .rect = original_rect,
    };
    
    jd_GL2DVertex bottom_left = {
        .pos = {rect.x0, rect.y0, z},
        .tx = t.uvw_min,
        .color = color,
        .rect = original_rect,
    };
    
    jd_GL2DVertex top_left = {
        .pos = {rect.x0, rect.y1, z},
        .tx = {t.uvw_min.u, t.uvw_max.v, t.uvw_max.w},
        .color = color,
        .rect = original_rect,
    };
    
    jd_DArray* vertices = jd_global_2d_renderer.vertices;
    jd_DArrayPushBack(vertices, &top_right);
    jd_DArrayPushBack(vertices, &bottom_right);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &bottom_left);
    jd_DArrayPushBack(vertices, &top_left);
    jd_DArrayPushBack(vertices, &top_right);
    
    //jd_2DColorRect(position, jd_V2F(width, height), (jd_V4F){1.0f, 1.0f, 1.0f, 1.0f}, 0, 0, 1.0f, clip);
    
    *out_advance += width;
}

void jd_2DString(jd_Font2* font, jd_String string, u16 point_size, jd_V2F position, jd_V4F color, jd_V4F clip, f32 wrap, b32 wrap_on_newlines) {
    jd_V2F p = position;
    jd_V2F ext = jd_FontGetTextLayoutExtent(font, point_size, string, wrap, wrap_on_newlines);
    p.y -= ((font->ascent - font->layout_line_height) * point_size);
    for (u64 i = 0; i < string.count;) {
        u32 codepoint = jd_Codepoint8to32(string, &i);
        if (codepoint)
            jd_2DGlyphRect(font, codepoint, point_size, p, color, clip, &p.x);
    }
}

void jd_2DRendererDraw() {
    jd_2DRenderer* r = &jd_global_2d_renderer;
    jd_V2I size = jd_WindowGetDrawSize(jd_global_2d_renderer.bound_window);
    glUseProgram(r->objects.shader);
    
    i32 tex_location = glGetUniformLocation(r->objects.shader, "tex");
    glUniform1i(tex_location, 0);
    
    i32 screen_conv_location = glGetUniformLocation(r->objects.shader, "screen_conv");
    glUniform2f(screen_conv_location, 2.0f / (f32)size.w, 2.0f / (f32)size.h);
    
    glBindVertexArray(r->objects.vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->objects.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, r->vertices->count * sizeof(jd_GL2DVertex), r->vertices->view.mem);
    
    jd_2DDrawGroup* dg = r->draw_group_chain_first;
    jd_2DDrawGroup* last_dg = 0;
    jd_ForDLLForward(dg, dg != 0) {
        if (last_dg) {
            jd_DLinksClear(last_dg);
        }
        
        u64 size = 0;
        if (dg->next) {
            size = dg->next->vertices_index - dg->vertices_index;
        } else {
            size = r->vertices->count - dg->vertices_index;
        }
        
        glBindTexture(GL_TEXTURE_2D_ARRAY, dg->gl_texture);
        glDrawArrays(GL_TRIANGLES, dg->vertices_index, size);
        last_dg = dg;
    }
    
    jd_DArrayClearNoDecommit(r->vertices);
    r->draw_group_chain_first = 0;
    r->draw_group_chain_last = 0;
    
    jd_ArenaPopTo(r->frame_arena, 0);
}
