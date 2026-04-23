#include <initguid.h>

#include "dep/c_d2d_dwrite/cd2d.h"
#include "dep/c_d2d_dwrite/cdwrite.h"

#define jd_Font_Internal_DWrite_Max_Faces 256

static jd_Font2 jd_os_base_font = {0};
static jd_Font2 jd_os_symbol_font = {0};

typedef struct jd_Font_Internal_DWriteObjects {
    IDWriteFontFace* face;
    IDWriteFontFile* file;
} jd_Font_Internal_DWriteObjects;

typedef struct jd_Internal_FontState {
    jd_Arena* arena;
    IDWriteFactory* dwrite_factory;
    IDWriteRenderingParams* rendering_params;
    IDWriteGdiInterop* gdi_interop;
    IDWriteFontFallback* fallback;
    
    jd_Font_Internal_DWriteObjects per_handle_objects[jd_Font_Internal_DWrite_Max_Faces];
    u32 loaded_handles;
    
    ID2D1Factory* d2d_factory;
    ID2D1DCRenderTarget* d2d_render_target;
    ID2D1SolidColorBrush* d2d_brush;
    IDXGISurface* d2d_surface;
    
    jd_V2U render_target_dimensions;
} jd_Internal_FontState;

static jd_Internal_FontState* jd_internal_font_state = 0;

void jd_Font_Internal_DWriteInit() {
    jd_Arena* arena = jd_ArenaCreate(0, 0);
    jd_internal_font_state = jd_ArenaAlloc(arena, sizeof(jd_Internal_FontState));
    jd_internal_font_state->arena = arena;
    
    // get dwrite factory
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory2, &jd_internal_font_state->dwrite_factory);
    if (!jd_internal_font_state->dwrite_factory) {
        jd_LogError("Could not create DirectWrite factory", jd_Error_MissingResource, jd_Error_Fatal);
    }
    
    
    // make rendering params
    
    IDWriteFactory2_CreateCustomRenderingParams2((IDWriteFactory2*)jd_internal_font_state->dwrite_factory, 1.0f, 0.0f, 0.0f, 0.0f, 
                                                 DWRITE_PIXEL_GEOMETRY_FLAT,
                                                 DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC,
                                                 DWRITE_GRID_FIT_MODE_ENABLED,
                                                 (IDWriteRenderingParams2 **)&jd_internal_font_state->rendering_params);
    
    if (!jd_internal_font_state->rendering_params) {
        jd_LogError("Could not create DirectWrite rendering parameters", jd_Error_MissingResource, jd_Error_Fatal);
    }
    
    // get gdi interop
    IDWriteFactory_GetGdiInterop(jd_internal_font_state->dwrite_factory, &jd_internal_font_state->gdi_interop);
    if (!jd_internal_font_state->gdi_interop) {
        jd_LogError("Could not create DirectWrite GDI interop.", jd_Error_MissingResource, jd_Error_Fatal);
    }
    
    // get font fallback
    IDWriteFactory2_GetSystemFontFallback((IDWriteFactory2*)jd_internal_font_state->dwrite_factory, &jd_internal_font_state->fallback);
    if (!jd_internal_font_state->fallback) {
        jd_LogError("Could not create DirectWrite font fallback.", jd_Error_MissingResource, jd_Error_Fatal);
    }
    
    D2D1_FACTORY_OPTIONS options = {0};
    HRESULT result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, &options, &jd_internal_font_state->d2d_factory);
    if (!jd_internal_font_state->d2d_factory) {
        jd_LogError("Could not create D2D Factory", jd_Error_MissingResource, jd_Error_Fatal);
    }
    
    D2D1_RENDER_TARGET_PROPERTIES props = {
        .type = D2D1_RENDER_TARGET_TYPE_DEFAULT,
        .pixelFormat = {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED},
    };
    
    result = ID2D1Factory_CreateDCRenderTarget(jd_internal_font_state->d2d_factory,  &props, &jd_internal_font_state->d2d_render_target);
    if (!jd_internal_font_state->d2d_render_target) {
        jd_LogError("Could not create D2D render target", jd_Error_MissingResource, jd_Error_Fatal);
    }
}

jd_Font2 jd_FontAdd(jd_String path) {
    if (!jd_internal_font_state) {
        jd_Font_Internal_DWriteInit();
    }
    jd_Font2 font = {0};
    if (!jd_DiskPathExists(path)) {
        jd_LogError("Requested font does not exist.", jd_Error_BadInput, jd_Error_Critical);
        return font;
    }
    
    jd_Font_Internal_DWriteObjects* objects = &jd_internal_font_state->per_handle_objects[jd_internal_font_state->loaded_handles];
    
    IDWriteFactory_CreateFontFileReference(jd_internal_font_state->dwrite_factory, (WCHAR*)jd_UTF8ToUTF16(jd_internal_font_state->arena, path).mem, 0, &objects->file);
    if (!objects->file) {
        jd_LogError("DirectWrite: Could not load requested font file!", jd_Error_BadInput, jd_Error_Common);
        return font;
    }
    
    jd_String extension = jd_DiskPathGetExtension(path);
    DWRITE_FONT_FACE_TYPE type = DWRITE_FONT_FACE_TYPE_TRUETYPE;
    if (jd_StringMatch(extension, jd_StrLit("ttc"))){
        type = DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION;
    }
    else if (jd_StringMatch(extension, jd_StrLit("fon"))) {
        type = DWRITE_FONT_FACE_TYPE_BITMAP;
    }
    
    IDWriteFactory_CreateFontFace(jd_internal_font_state->dwrite_factory, type, 1, &objects->file, 0, DWRITE_FONT_SIMULATIONS_NONE, &objects->face);
    if (!objects->face) {
        jd_LogError("DirectWrite: Could not create font face", jd_Error_BadInput, jd_Error_Common);
        return font;
    }
    
    font.path = jd_StringPush(jd_internal_font_state->arena, path);
    font.metrics_table = jd_ArenaAlloc(jd_internal_font_state->arena, sizeof(jd_GlyphMetrics) * KILOBYTES(64));
    font.metrics_table_count = KILOBYTES(64);
    font.handle = jd_internal_font_state->loaded_handles + 1;
    
    jd_internal_font_state->loaded_handles++;
    
    return font;
}

D2D_MATRIX_3X2_F D2DGetIdentity() {
    D2D_MATRIX_3X2_F identity = {0};
    
    identity._11 = 1.f;
    identity._12 = 0.f;
    identity._21 = 0.f;
    identity._22 = 1.f;
    identity._31 = 0.f;
    identity._32 = 0.f;
    
    return identity;
}

typedef struct IDWriteTextAnalysisSourceVtbl {
    // IUnknown methods
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDWriteTextAnalysisSource* This, REFIID riid, void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(IDWriteTextAnalysisSource* This);
    ULONG (STDMETHODCALLTYPE* Release)(IDWriteTextAnalysisSource* This);
    
    // IDWriteTextAnalysisSource methods
    HRESULT (STDMETHODCALLTYPE* GetTextAtPosition)(IDWriteTextAnalysisSource* This, UINT32 textPosition, WCHAR const** textString, UINT32* textLength);
    HRESULT (STDMETHODCALLTYPE* GetTextBeforePosition)(IDWriteTextAnalysisSource* This, UINT32 textPosition, WCHAR const** textString, UINT32* textLength);
    DWRITE_READING_DIRECTION (STDMETHODCALLTYPE* GetParagraphReadingDirection)(IDWriteTextAnalysisSource* This);
    HRESULT (STDMETHODCALLTYPE* GetLocaleName)(IDWriteTextAnalysisSource* This, UINT32 textPosition, UINT32* textLength, WCHAR const** localeName);
    HRESULT (STDMETHODCALLTYPE* GetNumberSubstitution)(IDWriteTextAnalysisSource* This, UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution** numberSubstitution);
} IDWriteTextAnalysisSourceVtbl;

typedef struct FontFallBackWrapper {
    IDWriteTextAnalysisSourceVtbl* lpVtbl;
    u32 codepoint;
    u16 utf16[3];
} FontFallBackWrapper;

HRESULT STDMETHODCALLTYPE jd_Internal_FontFallback_GetTextAtPosition(IDWriteTextAnalysisSource* this, u32 text_position, WCHAR const** text_string, u32* text_length) {
    FontFallBackWrapper* impl = (FontFallBackWrapper*)this;
    u32 total_length = 0;
    
    if (impl->codepoint > 0xFFFF) {
        u32 decode = impl->codepoint - 0x10000;
        u16 high_sur = (decode >> 10) + 0xD800;
        u16 low_sur  = (decode & 0x3FF) + 0xDC00;
        
        impl->utf16[0] = high_sur;
        impl->utf16[1] = low_sur;
        total_length = 2;
    } else {
        impl->utf16[0] = (u16)impl->codepoint;
        total_length = 1;
    }
    
    if (text_position > total_length) {
        *text_length = 0;
        *text_string = 0;
        return S_OK;
    }
    
    *text_string = &impl->utf16[text_position];
    *text_length = total_length - text_position;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE jd_Internal_FontFallback_GetTextBeforePosition(IDWriteTextAnalysisSource* this, u32 text_position, WCHAR const** text_string, u32* text_length) {
    if (text_position == 0) {
        return S_OK;
    }
    u32 total_length = 0;
    FontFallBackWrapper* impl = (FontFallBackWrapper*)this;
    if (impl->codepoint > 0xFFFF) {
        u32 decode = impl->codepoint - 0x10000;
        u16 high_sur = (decode >> 10) + 0xD800;
        u16 low_sur  = (decode & 0x3FF) + 0xDC00;
        
        impl->utf16[0] = high_sur;
        impl->utf16[1] = low_sur;
        total_length = 2;
    } else {
        impl->utf16[0] = (u16)impl->codepoint;
        total_length = 1;
    }
    
    text_position -= 1;
    
    *text_string = &impl->utf16[text_position];
    *text_length = total_length - text_position;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE jd_Internal_FontFallback_GetLocaleName(IDWriteTextAnalysisSource* This, UINT32 textPosition, UINT32* textLength, WCHAR const** localeName) {
    *localeName = L"en-US";
    *textLength = jd_ArrayCount(*localeName);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE jd_Internal_FontFallback_GetNumberSubstitution(IDWriteTextAnalysisSource* This, UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution** numberSubstitution) {
    return S_OK;
}

DWRITE_READING_DIRECTION STDMETHODCALLTYPE jd_Internal_FontFallback_GetReadingDirection(IDWriteTextAnalysisSource* this) {
    return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
}

HRESULT STDMETHODCALLTYPE DWriteStump(IDWriteTextAnalysisSource* this) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE QueryInterfaceStump(IDWriteTextAnalysisSource* this, REFIID riid, void** ppvObject) {
    return S_OK;
}

jd_ReadOnly static u32 _jd_fonts_default_seed = 104720809;

jd_ForceInline u32 jd_GlyphMetricsID(jd_Font2* font, u32 codepoint) {
    u32 to_hash = (codepoint << 11) + font->handle;
    u32 key = jd_HashU32toU32(to_hash, _jd_fonts_default_seed);
    return key;
}

void jd_Font_Internal_LoadGlyphMetricsForString(jd_Font2* font, jd_Arena* arena, jd_String32 string32) {
    u16* glyph_indices = jd_ArenaAlloc(arena, sizeof(u32) * string32.count);
    
    jd_Font_Internal_DWriteObjects* objects = &jd_internal_font_state->per_handle_objects[font->handle - 1];
    IDWriteFontFace_GetGlyphIndices(objects->face, string32.mem, string32.count, glyph_indices);
    IDWriteFontFace* face = objects->face;
    
    DWRITE_FONT_METRICS face_metrics = {0};
    IDWriteFontFace_GetMetrics(face, &face_metrics);
    f32 per_em = (f32)face_metrics.designUnitsPerEm;
    
    f32 dpi_factor = (96.0f/72.0f);
    
    font->ascent = dpi_factor * ((f32)face_metrics.ascent / per_em);
    font->descent = dpi_factor * ((f32)face_metrics.descent / per_em);
    font->line_gap = dpi_factor * ((f32)face_metrics.lineGap / per_em);
    font->line_adv = font->ascent + font->descent + font->line_gap;
    font->layout_line_height = dpi_factor * ((f32)face_metrics.capHeight / per_em);
    
    for (u64 i = 0; i < string32.count; i++) {
        b32 fallback_used = false;
        jd_GlyphMetrics* metrics = jd_FontGetGlyphMetrics(font, *(string32.mem + i));
        IDWriteFontFace* face = objects->face;
        IDWriteFont* fallback_font = 0;
        if (metrics->loaded == 0) {
            u16 gi = *(glyph_indices + i);
            if (gi == 0) {
                IDWriteTextAnalysisSourceVtbl table = {
                    QueryInterfaceStump,
                    DWriteStump,
                    DWriteStump,
                    jd_Internal_FontFallback_GetTextAtPosition,
                    jd_Internal_FontFallback_GetTextBeforePosition, 
                    jd_Internal_FontFallback_GetReadingDirection, 
                    jd_Internal_FontFallback_GetLocaleName, 
                    jd_Internal_FontFallback_GetNumberSubstitution
                };
                
                FontFallBackWrapper wrapper = {
                    .lpVtbl = &table,
                    .codepoint = string32.mem[i]
                };
                
                u32 mapped_length = 0;
                f32 scale = 0;
                HRESULT result = 0;
                u32 text_length = (string32.mem[i] > 0xFFFF) ? 2 : 1;
                result = IDWriteFontFallback_MapCharacters(jd_internal_font_state->fallback, (IDWriteTextAnalysisSource*)&wrapper, 0, text_length, 0, 0, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &fallback_font, &scale);
                
                if (!fallback_font) {
                    continue;
                }
                
                IDWriteFont_CreateFontFace(fallback_font, &face);
                IDWriteFontFace_GetGlyphIndices(face, &string32.mem[i], 1, &gi);
                
                fallback_used = true;
            }
            
            DWRITE_GLYPH_METRICS glyph_metrics = {0};
            IDWriteFontFace_GetDesignGlyphMetrics(face, &gi, 1, &glyph_metrics, 0);
            
            metrics->h_advance = dpi_factor * ((f32)glyph_metrics.advanceWidth / per_em);
            
            if (fallback_used) {
                IDWriteFont_Release(fallback_font);
                IDWriteFontFace_Release(face);
            }
            
            metrics->loaded = true;
        }
    }
}

jd_V2F jd_FontGetTextLayoutExtent(jd_Font2* font, u16 point_size, jd_String string, f32 wrap, b32 wrap_on_newlines) {
    if (!font || !font->handle) {
        jd_LogError("Font has not been initialized. Font should be returned by jd_FontAdd", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_V2F ext = {0, font->layout_line_height * point_size};
    jd_V2U pen = {0, 0};
    jd_ScratchArena s = jd_ScratchArenaCreate(jd_internal_font_state->arena);
    jd_String32 string32 = jd_UTF8ToUTF32(s.arena, string);
    
    jd_Font_Internal_LoadGlyphMetricsForString(font, s.arena, string32);
    
    for (u64 i = 0; i < string32.count; i++) {
        u32 codepoint = *(string32.mem + i);
        if (wrap_on_newlines && codepoint == '\n') {
            ext.y += jd_F32RoundUp(font->line_adv * point_size);
            pen.x = 0;
            pen.y += jd_F32RoundUp(font->line_adv * point_size);
        }
        
        if (codepoint == '\r') {
            continue;
        }
        
        jd_GlyphMetrics* metrics = jd_FontGetGlyphMetrics(font, codepoint);
        if ((wrap > 0.0f) && pen.x + (metrics->h_advance * point_size) > wrap) {
            ext.y += jd_F32RoundUp(font->line_adv * point_size);
            pen.x = 0;
            pen.y += jd_F32RoundUp(font->line_adv * point_size);
        } else {
            pen.x += jd_F32RoundUp(metrics->h_advance * point_size);
        }
        
        ext.x = jd_Max(pen.x, ext.x);
    }
    
    jd_ScratchArenaRelease(s);
    
    return ext;
}

jd_V2F jd_FontGetPenPositionForIndex(jd_Font2* font, u16 point_size, jd_String string, u64 index, f32 wrap, b32 wrap_on_newlines) {
    if (!font || !font->handle) {
        jd_LogError("Font has not been initialized. Font should be returned by jd_FontAdd", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_V2F ext = {0, font->layout_line_height * point_size};
    jd_V2U pen = {0, 0};
    jd_ScratchArena s = jd_ScratchArenaCreate(jd_internal_font_state->arena);
    jd_String32 string32 = jd_UTF8ToUTF32(s.arena, string);
    
    jd_Font_Internal_LoadGlyphMetricsForString(font, s.arena, string32);
    
    for (u64 i = 0; i < string32.count && i < index; i++) {
        u32 codepoint = *(string32.mem + i);
        if (wrap_on_newlines && codepoint == '\n') {
            ext.y += font->line_adv * point_size;
            pen.x = 0;
            pen.y += font->line_adv * point_size;
        }
        
        if (codepoint == '\r') {
            continue;
        }
        
        jd_GlyphMetrics* metrics = jd_FontGetGlyphMetrics(font, codepoint);
        if ((wrap > 0.0f) && pen.x + (metrics->h_advance * point_size) > wrap) {
            ext.y += font->line_adv * point_size;
            pen.x = 0;
            pen.y += font->line_adv * point_size;
        } else {
            pen.x += (metrics->h_advance * point_size);
        }
        
        ext.x = jd_Max(pen.x, ext.x);
    }
    
    jd_ScratchArenaRelease(s);
    return (jd_V2F){pen.x, pen.y};
}

u64 jd_FontGetIndexForPenPosition(jd_Font2* font, u16 point_size, jd_String string, jd_V2F position, f32 wrap, b32 wrap_on_newlines) {
    if (!font || !font->handle) {
        jd_LogError("Font has not been initialized. Font should be returned by jd_FontAdd", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_V2U pen = {0, 0};
    jd_ScratchArena s = jd_ScratchArenaCreate(jd_internal_font_state->arena);
    jd_String32 string32 = jd_UTF8ToUTF32(s.arena, string);
    jd_Font_Internal_LoadGlyphMetricsForString(font, s.arena, string32);
    b32 y_match_found = false;
    u64 i = 0;
    for (i = 0; i < string32.count; i++) {
        u32 codepoint = *(string32.mem + i);
        if (wrap_on_newlines && codepoint == '\n') {
            pen.x = 0;
            pen.y += font->line_adv * point_size;
        }
        
        if (codepoint == '\r') {
            continue;
        }
        
        if (i == 0 && position.y < pen.y) {
            i = 0;
            return i;
        }
        
        f32 off_x = position.x - pen.x;
        f32 off_y = position.y - pen.y;
        
        f32 off_x_abs = jd_F32Abs(off_x);
        f32 off_y_abs = jd_F32Abs(off_y);
        
        if (off_y_abs <= (font->line_adv * point_size * 0.5) ) {
            y_match_found = true;
        }
        
        jd_GlyphMetrics* metrics = jd_FontGetGlyphMetrics(font, codepoint);
        
        if ((off_x_abs <= (metrics->h_advance * point_size * 0.5)) && y_match_found) {
            jd_ClampToRange(i, 0, string32.count);
            break;
        }
        
        if ((wrap > 0.0f) && pen.x + (metrics->h_advance * point_size) > wrap) {
            pen.x = 0;
            pen.y += font->line_adv * point_size;
        } else {
            pen.x += (metrics->h_advance * point_size);
        }
        
    }
    
    jd_ScratchArenaRelease(s);
    return i;
}

jd_V2F jd_FontGetTextGraphicalExtent(jd_Font2* font, jd_String string) {
    if (!font || !font->handle) {
        jd_LogError("Font has not been initialized. Font should be returned by jd_FontAdd", jd_Error_APIMisuse, jd_Error_Fatal);
    }
}

jd_GlyphMetrics* jd_FontGetGlyphMetrics(jd_Font2* font, u32 codepoint) {
    u32 glyph_id = jd_GlyphMetricsID(font, codepoint);
    jd_GlyphMetrics* metrics = &font->metrics_table[glyph_id % font->metrics_table_count];
    if (metrics->codepoint == 0 && metrics->font == 0) {
        metrics->codepoint = codepoint;
        metrics->font = font;
        return metrics;
    }
    
    while ((metrics->codepoint != codepoint || metrics->font != font) && metrics->hash_chain != 0) {
        metrics = metrics->hash_chain;
    }
    
    if (metrics->codepoint != codepoint || metrics->font != font) {
        metrics->hash_chain = jd_ArenaAlloc(jd_internal_font_state->arena, sizeof(*metrics));
        metrics = metrics->hash_chain;
        metrics->codepoint = codepoint;
        metrics->font = font;
    }
    
    return metrics;
}

jd_Bitmap jd_FontGetGlyphBitmap(jd_Arena* arena, jd_Font2* font, u32 codepoint, u32 size) {
    jd_Bitmap zero_bitmap = {0};
    
    if (!font || !font->handle) {
        jd_LogError("Font has not been initialized. Font should be returned by jd_FontAdd", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_Font_Internal_DWriteObjects* objects = &jd_internal_font_state->per_handle_objects[font->handle - 1];
    
    u16 glyph_index = 0;
    
    IDWriteFontFace_GetGlyphIndices(objects->face, &codepoint, 1, &glyph_index);
    IDWriteFontFace* face = objects->face;
    IDWriteFontFace* main_face = face;
    IDWriteFont* fallback_font = 0;
    b32 fallback_used = false;
    
    if (!glyph_index && codepoint != 0) {
        IDWriteTextAnalysisSourceVtbl table = {
            QueryInterfaceStump,
            DWriteStump,
            DWriteStump,
            jd_Internal_FontFallback_GetTextAtPosition,
            jd_Internal_FontFallback_GetTextBeforePosition, 
            jd_Internal_FontFallback_GetReadingDirection, 
            jd_Internal_FontFallback_GetLocaleName, 
            jd_Internal_FontFallback_GetNumberSubstitution
        };
        
        FontFallBackWrapper wrapper = {
            .lpVtbl = &table,
            .codepoint = codepoint
        };
        
        u32 mapped_length = 0;
        f32 scale = 0;
        HRESULT result = 0;
        
        u32 text_length = (codepoint > 0xFFFF) ? 2 : 1;
        result = IDWriteFontFallback_MapCharacters(jd_internal_font_state->fallback, (IDWriteTextAnalysisSource*)&wrapper, 0, text_length, 0, 0, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &fallback_font, &scale);
        
        if (fallback_font) {
            result = IDWriteFont_CreateFontFace(fallback_font, &face);
            result = IDWriteFontFace_GetGlyphIndices(face, &codepoint, 1, &glyph_index);
            fallback_used = true;
        }
    }
    
    DWRITE_FONT_METRICS face_metrics = {0};
    IDWriteFontFace_GetMetrics(face, &face_metrics);
    f32 per_em = (f32)face_metrics.designUnitsPerEm;
    
    f32 dpi_factor = (96.0f/72.0f);
    
    if ((!fallback_used) || font->line_adv == 0) {
        font->ascent = dpi_factor * ((f32)face_metrics.ascent / per_em);
        font->descent = dpi_factor * ((f32)face_metrics.descent / per_em);
        font->line_gap = dpi_factor * ((f32)face_metrics.lineGap / per_em);
        font->line_adv = font->ascent + font->descent + font->line_gap;
        font->layout_line_height = dpi_factor * ((f32)face_metrics.capHeight / per_em);
    }
    
    DWRITE_GLYPH_METRICS glyph_metrics = {0};
    IDWriteFontFace_GetDesignGlyphMetrics(face, &glyph_index, 1, &glyph_metrics, 0);
    
    jd_GlyphMetrics* metrics = jd_FontGetGlyphMetrics(font, codepoint);
    metrics->h_advance = (dpi_factor * glyph_metrics.advanceWidth / per_em);
    metrics->offset_x = dpi_factor * ((f32)glyph_metrics.leftSideBearing / per_em);
    metrics->offset_y = dpi_factor * ((f32)glyph_metrics.topSideBearing / per_em);
    
    i16 render_height = (dpi_factor * size * (((f32)face_metrics.ascent + (f32)face_metrics.descent) / per_em));
    render_height = (i16)(render_height + 0.5f);
    i16 render_width = dpi_factor * size * glyph_metrics.advanceWidth / per_em;
    
    IDWriteBitmapRenderTarget* target = 0;
    IDWriteGdiInterop_CreateBitmapRenderTarget(jd_internal_font_state->gdi_interop, 0, render_width, render_height, &target);
    IDWriteBitmapRenderTarget_SetPixelsPerDip(target, 1.0f);
    
    COLORREF bg_color = RGB(0, 0, 0);
    
    DWRITE_GLYPH_RUN glyph_run = {
        .fontFace = face,
        .fontEmSize = size * dpi_factor,
        .glyphCount = 1,
        .glyphIndices = &glyph_index
    };
    
    RECT dc_rect = {
        0,
        0,
        render_width,
        render_height
    };
    
    HDC dc = IDWriteBitmapRenderTarget_GetMemoryDC(target);
    HGDIOBJ original = SelectObject(dc, GetStockObject(DC_PEN));
    SetDCPenColor(dc, bg_color);
    SelectObject(dc, GetStockObject(DC_BRUSH));
    SetDCBrushColor(dc, bg_color);
    Rectangle(dc, 0, 0, render_width, render_height);
    SelectObject(dc, original);
    ID2D1DCRenderTarget_BindDC(jd_internal_font_state->d2d_render_target, dc, &dc_rect);
    
    f32 descent = size * font->descent;
    
    IDWriteColorGlyphRunEnumerator* color_enum = 0;
    HRESULT color_present = IDWriteFactory2_TranslateColorGlyphRun((IDWriteFactory2*)jd_internal_font_state->dwrite_factory, 0.0f, (render_height) - descent, &glyph_run, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &color_enum);
    
    b32 color_glyph = false;
    
    ID2D1DCRenderTarget_SetTextRenderingParams(jd_internal_font_state->d2d_render_target, jd_internal_font_state->rendering_params);
    
    if (color_present == DWRITE_E_NOCOLOR) {
        D2D1_POINT_2F baseline_origin = {0, render_height - (descent)};
        D2D1_COLOR_F color = {1.0f, 1.0f, 1.0f, 1.0f};
        D2D1_COLOR_F clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
        D2D1_BRUSH_PROPERTIES props = {1.0f, D2DGetIdentity()};
        ID2D1SolidColorBrush* brush = 0;
        
        ID2D1DCRenderTarget_CreateSolidColorBrush(jd_internal_font_state->d2d_render_target, &color, &props, &brush);
        ID2D1DCRenderTarget_BeginDraw(jd_internal_font_state->d2d_render_target);
        D2D_MATRIX_3X2_F identity = D2DGetIdentity();
        ID2D1DCRenderTarget_SetTransform(jd_internal_font_state->d2d_render_target, &identity);
        ID2D1DCRenderTarget_Clear(jd_internal_font_state->d2d_render_target, &clear_color);
        ID2D1DCRenderTarget_DrawGlyphRun(jd_internal_font_state->d2d_render_target, baseline_origin, &glyph_run, (ID2D1Brush*)brush, DWRITE_MEASURING_MODE_NATURAL);
        ID2D1DCRenderTarget_EndDraw(jd_internal_font_state->d2d_render_target, 0, 0);
    } else {
        color_glyph = true;
        if (color_enum) {
            DWRITE_COLOR_GLYPH_RUN* run = 0;
            b32 has_run = true;
            
            
            D2D1_COLOR_F clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
            D2D1_BRUSH_PROPERTIES props = {1.0f, D2DGetIdentity()};
            
            ID2D1DCRenderTarget_BeginDraw(jd_internal_font_state->d2d_render_target);
            D2D_MATRIX_3X2_F identity = D2DGetIdentity();
            ID2D1DCRenderTarget_SetTransform(jd_internal_font_state->d2d_render_target, &identity);
            ID2D1DCRenderTarget_Clear(jd_internal_font_state->d2d_render_target, &clear_color);
            
            while (1) {
                HRESULT result = IDWriteColorGlyphRunEnumerator_MoveNext(color_enum, &has_run);
                if (!has_run) {
                    break;
                }
                
                result = IDWriteColorGlyphRunEnumerator_GetCurrentRun(color_enum, &run);
                
                D2D1_COLOR_F color = {run->runColor.r, run->runColor.g, run->runColor.b, run->runColor.a};
                ID2D1SolidColorBrush* brush = 0;
                ID2D1DCRenderTarget_CreateSolidColorBrush(jd_internal_font_state->d2d_render_target, &color, &props, &brush);
                
                D2D1_POINT_2F baseline_origin = {run->baselineOriginX, run->baselineOriginY};
                ID2D1DCRenderTarget_DrawGlyphRun(jd_internal_font_state->d2d_render_target, baseline_origin, &run->glyphRun, (ID2D1Brush*)brush, DWRITE_MEASURING_MODE_NATURAL);
                
            }
            
            ID2D1DCRenderTarget_EndDraw(jd_internal_font_state->d2d_render_target, 0, 0);
        }
    }
    
#if 0    
    RECT bb = {0};
    IDWriteRenderingParams* rendering_params = jd_internal_font_state->rendering_params;
    
    f32 descent = size * font->descent;
    
    IDWriteColorGlyphRunEnumerator* color_enum = 0;
    HRESULT color_present = IDWriteFactory2_TranslateColorGlyphRun((IDWriteFactory2*)jd_internal_font_state->dwrite_factory, 0.0f, (render_height) - descent, &glyph_run, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &color_enum);
    
    b32 color_glyph = false;
    
    if (color_present == DWRITE_E_NOCOLOR) {
        COLORREF fg_color = RGB(255, 255, 255);
        HRESULT result = IDWriteBitmapRenderTarget_DrawGlyphRun(target, 0.0f, render_height - (descent), DWRITE_MEASURING_MODE_NATURAL, &glyph_run, rendering_params, fg_color, &bb);
    } else {
        color_glyph = true;
        if (color_enum) {
            DWRITE_COLOR_GLYPH_RUN* run = 0;
            b32 has_run = true;
            while (1) {
                HRESULT result = IDWriteColorGlyphRunEnumerator_MoveNext(color_enum, &has_run);
                if (!has_run) {
                    break;
                }
                
                result = IDWriteColorGlyphRunEnumerator_GetCurrentRun(color_enum, &run);
                COLORREF fg_color = RGB(run->runColor.r * 255, run->runColor.g * 255, run->runColor.b * 255);
                RECT rect = {0};
                IDWriteBitmapRenderTarget_DrawGlyphRun(target, run->baselineOriginX, run->baselineOriginY, DWRITE_MEASURING_MODE_NATURAL, &run->glyphRun, jd_internal_font_state->rendering_params, fg_color, &rect);
                
            }
            
        }
    }
#endif
    
    u8* return_bitmap = jd_ArenaAlloc(arena, sizeof(u32) * render_width * render_height);
    DIBSECTION dib = {0};
    {
        HBITMAP bitmap = (HBITMAP)GetCurrentObject(dc, OBJ_BITMAP);
        GetObject(bitmap, sizeof(dib), &dib);
    }
    
    u8* src = (u8*)dib.dsBm.bmBits;
    if (color_glyph) {
        for (u64 i = 0; i < (render_width * render_height); i++) {
            return_bitmap[i * 4] = src[(i * 4) + 2];
            return_bitmap[(i * 4) + 1] = src[(i * 4) + 1];
            return_bitmap[(i * 4) + 2] = src[i * 4];
            return_bitmap[(i * 4) + 3] = src[(i * 4) + 3];
        }
    } else {
        for (u64 i = 0; i < (render_width * render_height); i++) {
            return_bitmap[i * 4] = 0xff;
            return_bitmap[1 + (i * 4)] = 0xff;
            return_bitmap[2 + (i * 4)] = 0xff;
            return_bitmap[3 + (i * 4)] = src[(i * 4) + 3];
        }
        
    }
    
    if (fallback_used) {
        IDWriteFont_Release(fallback_font);
        IDWriteFontFace_Release(face);
    }
    
    IDWriteBitmapRenderTarget_Release(target);
    
    jd_Bitmap bmp = {
        .bytes_per_pixel = 4,
        .width = render_width,
        .height = render_height,
        .bitmap = return_bitmap
    };
    
    return bmp;
}

jd_Font2* jd_OSBaseFont() {
    if (!jd_os_base_font.handle) {
        jd_os_base_font = jd_FontAdd(jd_StrLit("C:\\Windows\\Fonts\\segoeui.ttf"));
    }
    
    return &jd_os_base_font;
}

jd_Font2* jd_OSSymbolFont() {
    if (!jd_os_symbol_font.handle) {
        jd_os_symbol_font = jd_FontAdd(jd_StrLit("C:\\Windows\\Fonts\\segoeicons.ttf"));
    }
    
    return &jd_os_symbol_font;
}