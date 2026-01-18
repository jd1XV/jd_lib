u64 jd_Pow_u64(u64 b, u64 e) {
    u64 x = 0;
    if (e > 0) {
        x = b;
    }
    
    for (u64 i = 1; i < e; i++) {
        x *= b;
    }
    return x;
}

jd_V4F jd_V4FMul4(jd_V4F a, jd_V4F b) {
    jd_CPUFlags cpu_flags = jd_SysInfoGetCPUFlags();
    if (jd_CPUFlagIsSet(cpu_flags, jd_CPUFlags_SupportsSSE)) {
        __m128 a128 = _mm_loadu_ps((f32*)&a);
        __m128 b128 = _mm_loadu_ps((f32*)&b);
        __m128 p = _mm_mul_ps(a128, b128); 
        jd_V4F res = {0};
        _mm_storeu_ps((f32*)&res, p);
        return res;
    } else {
        return (jd_V4F){a.x * b.x, a.y * b.y, a.w * b.w, a.h * b.h};
    }
}

jd_V4F jd_V4FMulS(jd_V4F a, f32 b) {
    jd_CPUFlags cpu_flags = jd_SysInfoGetCPUFlags();
    if (jd_CPUFlagIsSet(cpu_flags, jd_CPUFlags_SupportsSSE)) {
        __m128 a128 = _mm_loadu_ps((f32*)&a);
        __m128 b128 = _mm_set1_ps(b);
        __m128 p = _mm_mul_ps(a128, b128); 
        jd_V4F res = {0};
        _mm_storeu_ps((f32*)&res, p);
        return res;
    } else {
        return (jd_V4F){a.x * b, a.y * b, a.w * b, a.h * b};
    }
}

jd_V4F jd_V4FColorLerp(jd_V4F a, jd_V4F b, f32 frac) {
    jd_V4F res = {a.r + (b.r - a.r) * frac, a.g + (b.g - a.g) * frac, a.b + (b.b - a.b) * frac, a.a + (b.a - a.a) * frac};
    return res;
}
