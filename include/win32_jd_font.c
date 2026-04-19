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
    IDWriteRenderingParams* rendering_params;
    IDWriteGdiInterop* gdi_interop;
    IDWriteFontFallback* fallback;
    
    jd_Font_Internal_DWriteObjects per_handle_objects[jd_Font_Internal_DWrite_Max_Faces];
    u32 loaded_handles;
    
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
                                                 DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL,
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
    u16 utf16[2];
} FontFallBackWrapper;

HRESULT STDMETHODCALLTYPE jd_Internal_FontFallback_GetTextAtPosition(IDWriteTextAnalysisSource* this, u32 text_position, WCHAR const** text_string, u32* text_length) {
    FontFallBackWrapper* impl = (FontFallBackWrapper*)this;
    if (impl->codepoint > 0xFFFF) {
        u32 decode = impl->codepoint - 0x10000;
        u16 high_sur = (decode >> 10) + 0xD800;
        u16 low_sur  = (decode & 0x3FF) + 0xDC00;
        
        impl->utf16[0] = low_sur;
        impl->utf16[1] = high_sur;
        *text_length = 2;
    } else {
        impl->utf16[0] = (u16)impl->codepoint;
        *text_length = 1;
    }
    
    *text_string = &impl->utf16[0];
    return S_OK;
}

HRESULT STDMETHODCALLTYPE jd_Internal_FontFallback_GetTextBeforePosition(IDWriteTextAnalysisSource* this, u32 text_position, WCHAR const** text_string, u32* text_length) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE jd_Internal_FontFallback_GetLocaleName(IDWriteTextAnalysisSource* This, UINT32 textPosition, UINT32* textLength, WCHAR const** localeName) {
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
    font->layout_line_height = (f32)((u32)((dpi_factor * ((f32)face_metrics.capHeight / per_em)) + 0.5));
    
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
                result = IDWriteFontFallback_MapCharacters(jd_internal_font_state->fallback, (IDWriteTextAnalysisSource*)&wrapper, 0, 1, 0, 0, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &fallback_font, &scale);
                
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
    
    COLORREF bg_color = RGB(0, 0, 0);
    COLORREF fg_color = RGB(255, 255, 255);
    
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
        result = IDWriteFontFallback_MapCharacters(jd_internal_font_state->fallback, (IDWriteTextAnalysisSource*)&wrapper, 0, 1, 0, 0, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &fallback_font, &scale);
        
        if (fallback_font) {
            IDWriteFont_CreateFontFace(fallback_font, &face);
            IDWriteFontFace_GetGlyphIndices(face, &codepoint, 1, &glyph_index);
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
        font->layout_line_height = (f32)((u64)((dpi_factor * ((f32)face_metrics.capHeight / per_em)) + 0.5));
    }
    
    DWRITE_GLYPH_METRICS glyph_metrics = {0};
    IDWriteFontFace_GetDesignGlyphMetrics(face, &glyph_index, 1, &glyph_metrics, 0);
    
    jd_GlyphMetrics* metrics = jd_FontGetGlyphMetrics(font, codepoint);
    metrics->h_advance = (dpi_factor * glyph_metrics.advanceWidth / per_em);
    
    i16 render_height = (dpi_factor * size * (((f32)face_metrics.ascent + (f32)face_metrics.descent) / per_em));
    render_height = (i16)(render_height + 0.5f);
    i16 render_width = dpi_factor * size * glyph_metrics.advanceWidth / per_em;
    
    IDWriteBitmapRenderTarget* target = 0;
    IDWriteGdiInterop_CreateBitmapRenderTarget(jd_internal_font_state->gdi_interop, 0, render_width, render_height, &target);
    IDWriteBitmapRenderTarget_SetPixelsPerDip(target, 1.0f);
    
    HDC dc = IDWriteBitmapRenderTarget_GetMemoryDC(target);
    HGDIOBJ original = SelectObject(dc, GetStockObject(DC_PEN));
    SetDCPenColor(dc, bg_color);
    SelectObject(dc, GetStockObject(DC_BRUSH));
    SetDCBrushColor(dc, bg_color);
    Rectangle(dc, 0, 0, render_width, render_height);
    SelectObject(dc, original);
    
    DWRITE_GLYPH_RUN glyph_run = {
        .fontFace = face,
        .fontEmSize = size * dpi_factor,
        .glyphCount = 1,
        .glyphIndices = &glyph_index
    };
    
    RECT bb = {0};
    IDWriteRenderingParams* rendering_params = jd_internal_font_state->rendering_params;
    
    f32 descent = size * font->descent;
    
    HRESULT result = IDWriteBitmapRenderTarget_DrawGlyphRun(target, 0.0f, render_height - (descent), DWRITE_MEASURING_MODE_NATURAL, &glyph_run, rendering_params, fg_color, &bb);
    
    u8* return_bitmap = jd_ArenaAlloc(arena, sizeof(u32) * render_width * render_height);
    DIBSECTION dib = {0};
    {
        HBITMAP bitmap = (HBITMAP)GetCurrentObject(dc, OBJ_BITMAP);
        GetObject(bitmap, sizeof(dib), &dib);
    }
    
    u8* src = (u8*)dib.dsBm.bmBits;
    
    for (u64 i = 0; i < (render_width * render_height); i++) {
        return_bitmap[i * 4] = 0xff;
        return_bitmap[1 + (i * 4)] = 0xff;
        return_bitmap[2 + (i * 4)] = 0xff;
        return_bitmap[3 + (i * 4)] = src[i * 4];
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
