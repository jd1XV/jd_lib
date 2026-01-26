/* date = February 19th 2024 8:16 pm */

#ifndef JD_DEFS_H
#define JD_DEFS_H

typedef unsigned long long u64;
typedef unsigned int       u32;
typedef unsigned short     u16;
typedef unsigned char       u8;

typedef signed long long   i64;
typedef signed int         i32;
typedef signed short       i16;
typedef signed char         i8;

typedef i8                  b8;
typedef i16                b16;
typedef i32                b32;

typedef double             f64;
typedef float              f32;

typedef char                c8;

#define true  1
#define false 0

typedef struct jd_View {
    u8* mem;
    union {
        u64 size; // NOTE: Just for the sake of semantic comfort, files have a "size," and strings have a "count"
        u64 count; 
    };
} jd_View;

typedef struct jd_V2F {
    union {
        struct {
            f32 x;
            f32 y;
        };
        struct {
            f32 u;
            f32 v;
        };
        struct {
            f32 w;
            f32 h;
        };
        f32 val[2];
    };
    
} jd_V2F;

#define jd_V2F(x, y) (jd_V2F){x, y}

typedef struct jd_V3F {
    union {
        struct {
            f32 x;
            f32 y;
            f32 z;
        };
        struct {
            f32 u;
            f32 v;
            f32 w;
        };
    };
    
} jd_V3F;

#define jd_V3F(x, y, z) (jd_V3F){x, y, z}

typedef struct jd_V4F {
    union {
        struct {
            f32 r;
            f32 g;
            f32 b;
            f32 a;
        };
        struct {
            f32 x0;
            f32 x1;
            f32 y0;
            f32 y1;
        };
        struct {
            f32 x;
            f32 y;
            f32 w;
            f32 h;
        };
    };
} jd_V4F;

#define jd_V4F(x, y, z, w) (jd_V4F){x, y, z, w}

#define jd_V4FAddRGB(a, b) {a.x + b.x, a.y + b.y, a.w + b.w, a.h}
#define jd_V4FSubRGB(a, b) {a.x - b.x, a.y - b.y, a.w - b.w, a.h}

typedef struct jd_U32Range {
    u32 min;
    u32 max;
} jd_U32Range;

typedef struct jd_V2U64 {
    u64 x;
    u64 y;
} jd_V2U64;


typedef struct jd_V2U {
    u32 x;
    u32 y;
} jd_V2U;

typedef struct jd_V3U {
    union {
        struct {
            u32 x;
            u32 y;
            u32 z;
        };
        struct {
            u32 u;
            u32 v;
            u32 w;
        };
    };
    
} jd_V3U;

typedef struct jd_V2I {
    union {
        struct {
            i32 x;
            i32 y;
        };
        struct {
            i32 w;
            i32 h;
        };
    };
    
} jd_V2I;

typedef struct jd_RectF32 {
    union {
        struct {
            jd_V2F min;
            jd_V2F max;
        };
        
        struct {
            f32 x0;
            f32 x1;
            f32 y0;
            f32 y1;
        };
    };
    
} jd_RectF32;

typedef struct jd_u128 {
    union {
        struct {
            u64 lo;
            u64 hi;
        };
        
        u64 val[2];
    };
} jd_u128;

#define jd_u128_EQ(a, b) (a.lo == b.lo && a.hi == b.hi)

#define jd_u128_Max (1 << 128ULL)

#pragma section("jd_readonlysec", read)
#define jd_ReadOnly __declspec(allocate("jd_readonlysec"))

#define jd_ExportFn __declspec(dllexport)

#define jd_ForceInline __forceinline

#ifdef JD_DEBUG
#include <assert.h>
#define jd_Assert(x) assert(x)
#else
#define jd_Assert(x)
#endif

#endif //JD_DEFS_H
