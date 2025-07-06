jd_ForceInline i64 jd_GetTicks() {
    LARGE_INTEGER perf_ct = {0};
    QueryPerformanceCounter(&perf_ct);
    return perf_ct.QuadPart;
}

jd_ExportFn jd_ForceInline jd_Timer jd_TimerStart() {
    jd_Timer watch = {0};
    watch.start = jd_GetTicks();
    return watch;
}

jd_ExportFn jd_ForceInline jd_Timer jd_TimerStop(jd_Timer watch) {
    i64 diff = jd_GetTicks() - watch.start;
    watch.stop = ((f64)diff / (jd_SysInfoGetPerformanceFrequency() / 1000.0f));
    return watch;
}