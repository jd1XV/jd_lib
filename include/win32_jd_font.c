#include <initguid.h>

#include "dep/c_d2d_dwrite/cd2d.h"
#include "dep/c_d2d_dwrite/cdwrite.h"

#define jd_Font_Internal_DWrite_Max_Faces 256

typedef struct jd_Font_Internal_DWriteObjects {
    IDWriteFontFace* face;
    IDWriteFontFile* file;
} jd_Font_Internal_DWriteObjects;

typedef struct jd_Internal_FontState {
    jd_Arena* arena;
    IDWriteFactory* dwrite_factory;
    IDWriteRenderingParams2* rendering_params;
    IDWriteBitmapRenderTarget* render_target;
    IDWriteGdiInterop* gdi_interop;
    
    jd_Font_Internal_DWriteObjects* per_handle_objects[jd_Font_Internal_DWrite_Max_Faces];
    u32 loaded_handles;
    
    jd_V2U render_target_dimensions;
} jd_Internal_FontState;

static jd_Internal_FontState* jd_internal_font_state = 0;

void jd_Font_Internal_DWriteInit() {
    jd_Arena* arena = jd_ArenaCreate(0, 0);
    jd_internal_font_state = jd_ArenaAlloc(arena, sizeof(jd_Internal_FontState));
    jd_internal_font_state->arena = arena;
    
    // get dwrite factory
    {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory2, &jd_internal_font_state->dwrite_factory);
        if (!jd_internal_font_state->dwrite_factory) {
            jd_LogError("Could not create DirectWrite factory", jd_Error_MissingResource, jd_Error_Fatal);
        }
    }
    
    // make rendering params
    
    {
        IDWriteFactory2_CreateCustomRenderingParams2((IDWriteFactory2*)jd_internal_font_state->dwrite_factory, 1.0f, 0.0f, 0.0f, 0.0f, 
                                                     DWRITE_PIXEL_GEOMETRY_FLAT,
                                                     DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
                                                     DWRITE_GRID_FIT_MODE_ENABLED,
                                                     (IDWriteRenderingParams2 **)&jd_internal_font_state->rendering_params);
        
        if (!jd_internal_font_state->rendering_params) {
            jd_LogError("Could not create DirectWrite rendering parameters", jd_Error_MissingResource, jd_Error_Fatal);
        }
    }
    
    // get gdi interop
    {
        IDWriteFactory_GetGdiInterop(jd_internal_font_state->dwrite_factory, &jd_internal_font_state->gdi_interop);
        jd_V2U dim = (jd_V2U){1024, 256};
        jd_internal_font_state->render_target_dimensions = dim;
        IDWriteGdiInterop_CreateBitmapRenderTarget(jd_internal_font_state->gdi_interop, 0, dim.x, dim.y, &jd_internal_font_state->render_target);
        IDWriteBitmapRenderTarget_SetPixelsPerDip(jd_internal_font_state->render_target, 1.0);
    }
}

b32 jd_FontAdd(jd_String id, jd_String path) {
    if (!jd_DiskPathExists(path)) {
        jd_LogError("Requested font does not exist.", jd_Error_BadInput, jd_Error_Critical);
        return false;
    }
    
    jd_Font_Internal_DWriteObjects* objects = jd_internal_font_state->per_handle_objects[jd_internal_font_state->loaded_handles];
    
    IDWriteFactory_CreateFontFileReference(jd_internal_font_state->dwrite_factory, (WCHAR*)jd_UTF8ToUTF16(jd_internal_font_state->arena, path).mem, 0, &objects->file);
    if (!objects->file) {
        jd_LogError("DirectWrite: Could not load requested font file!", jd_Error_BadInput, jd_Error_Common);
        return false;
    }
    
    IDWriteFactory_CreateFontFace(jd_internal_font_state->dwrite_factory, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &objects->file, 0, DWRITE_FONT_SIMULATIONS_NONE, &objects->face);
    if (!objects->face) {
        jd_LogError("DirectWrite: Could not create font face", jd_Error_BadInput, jd_Error_Common);
        return false;
    }
    
    jd_Font2 font = {
        .id = jd_StringPush(jd_internal_font_state->arena, id),
        .path = jd_StringPush(jd_internal_font_state->arena, path),
        .metrics_table = jd_ArenaAlloc(jd_internal_font_state->arena, sizeof(jd_GlyphMetrics) * KILOBYTES(64)),
        .metrics_table_count = KILOBYTES(64),
        .handle = jd_internal_font_state->loaded_handles
    };
    
    jd_internal_font_state->loaded_handles++;
    
    return true;
}

u8* jd_FontGetGlyphBitmap(jd_Font2 font, u32 codepoint) {
    jd_Font_Internal_DWriteObjects* objects = jd_internal_font_state->per_handle_objects[font.handle];
    
    return 0;
}