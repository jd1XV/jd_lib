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
                                                 DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
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
    
    IDWriteFactory_CreateFontFace(jd_internal_font_state->dwrite_factory, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &objects->file, 0, DWRITE_FONT_SIMULATIONS_NONE, &objects->face);
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

u8* jd_FontGetGlyphBitmap(jd_Arena* arena, jd_Font2 font, u32 codepoint, u32 size) {
    if (!font.handle) {
        jd_LogError("Font has not been initialized. Font should be returned by jd_FontAdd", jd_Error_APIMisuse, jd_Error_Fatal);
        return 0;
    }
    
    jd_Font_Internal_DWriteObjects* objects = &jd_internal_font_state->per_handle_objects[font.handle - 1];
    if (!objects->face) {
        return 0;
    }
    
    COLORREF bg_color = RGB(0, 0, 0);
    COLORREF fg_color = RGB(255, 255, 255);
    
    u32 height = 0;
    u32 width  = 0;
    
    DWRITE_FONT_METRICS face_metrics = {0};
    IDWriteFontFace_GetMetrics(objects->face, &face_metrics);
    f32 per_em = (f32)face_metrics.designUnitsPerEm;
    
    i16 render_height = (96.0f/72.0f) * size * (face_metrics.ascent + face_metrics.descent + face_metrics.lineGap) / per_em;
    render_height = (i16)(render_height + 0.5f + 1);
    
    IDWriteBitmapRenderTarget* target = 0;
    IDWriteGdiInterop_CreateBitmapRenderTarget(jd_internal_font_state->gdi_interop, 0, 128, 128, &target);
    IDWriteBitmapRenderTarget_SetPixelsPerDip(target, 1.0f);
    
    HDC dc = IDWriteBitmapRenderTarget_GetMemoryDC(target);
    HGDIOBJ original = SelectObject(dc, GetStockObject(DC_PEN));
    SetDCPenColor(dc, bg_color);
    SelectObject(dc, GetStockObject(DC_BRUSH));
    SetDCBrushColor(dc, bg_color);
    Rectangle(dc, 0, 0, 128, 128);
    SelectObject(dc, original);
    
    u16 glyph_index = 0;
    
    IDWriteFontFace_GetGlyphIndices(objects->face, &codepoint, 1, &glyph_index);
    IDWriteFontFace* face = objects->face;
    IDWriteFont* fallback_font = 0;
    b32 fallback_used = false;
    
    if (!glyph_index) {
        IDWriteTextAnalysisSourceVtbl table = {QueryInterfaceStump,DWriteStump,DWriteStump,jd_Internal_FontFallback_GetTextAtPosition,jd_Internal_FontFallback_GetTextBeforePosition, jd_Internal_FontFallback_GetReadingDirection, jd_Internal_FontFallback_GetLocaleName, jd_Internal_FontFallback_GetNumberSubstitution};
        FontFallBackWrapper wrapper = {
            .lpVtbl = &table,
            .codepoint = codepoint
        };
        
        u32 mapped_length = 0;
        f32 scale = 0;
        IDWriteFont* font = 0;
        HRESULT result = 0;
        result = IDWriteFontFallback_MapCharacters(jd_internal_font_state->fallback, (IDWriteTextAnalysisSource*)&wrapper, 0, 1, 0, 0, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font, &scale);
        IDWriteFont_CreateFontFace(font, &face);
        IDWriteFontFace_GetGlyphIndices(face, &codepoint, 1, &glyph_index);
        
        fallback_used = true;
    }
    
    DWRITE_GLYPH_RUN glyph_run = {
        .fontFace = face,
        .fontEmSize = size * (96.0f/72.0f),
        .glyphCount = 1,
        .glyphIndices = &glyph_index
    };
    
    RECT bb = {0};
    IDWriteRenderingParams* rendering_params = jd_internal_font_state->rendering_params;
    HRESULT result = IDWriteBitmapRenderTarget_DrawGlyphRun(target, 0.0f, 64, DWRITE_MEASURING_MODE_NATURAL, &glyph_run, rendering_params, fg_color, &bb);
    
    u8* return_bitmap = jd_ArenaAlloc(arena, sizeof(u32) * 128 * 128);
    DIBSECTION dib = {0};
    {
        HBITMAP bitmap = (HBITMAP)GetCurrentObject(dc, OBJ_BITMAP);
        GetObject(bitmap, sizeof(dib), &dib);
    }
    
    u8* src = (u8*)dib.dsBm.bmBits;
    
    for (u64 i = 0; i < (128 * 128); i++) {
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
    return return_bitmap;
}