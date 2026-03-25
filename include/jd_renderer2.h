/* date = March 16th 2026 11:07 am */

#ifndef JD_RENDERER2_H
#define JD_RENDERER2_H
#if 0
typedef struct jd_TypefaceUnicodeRange {
    u32 begin;
    u32 end;
} jd_TypefaceUnicodeRange;

static jd_ReadOnly jd_TypefaceUnicodeRange jd_unicode_range_basic_latin = {0, 0x7F};
static jd_ReadOnly jd_TypefaceUnicodeRange jd_unicode_range_bmp = {0, 0xFFFF};
static jd_ReadOnly jd_TypefaceUnicodeRange jd_unicode_range_all = {0, 0x10FFFF};

typedef enum jd_TextureMode {
    jd_TextureMode_Alpha,
    jd_TextureMode_RGBA
} jd_TextureMode;

typedef u64 jd_ImageKey;

typedef struct jd_ImageID {
    jd_ImageKey key;
    
    jd_TextureInfo* gpu_info;
    jd_V2F res_original;
} jd_ImageID;

#endif
#endif //JD_RENDERER2_H
