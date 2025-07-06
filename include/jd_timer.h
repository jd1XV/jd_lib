/* date = April 7th 2024 5:29 pm */

#ifndef JD_TIMER_H
#define JD_TIMER_H

#ifndef JD_UNITY_H
#include "jd_sysinfo.h"
#endif

typedef union jd_Timer {
    i64 start;
    f64 stop;
} jd_Timer; 

jd_ExportFn jd_ForceInline jd_Timer jd_TimerStart();
jd_ExportFn jd_ForceInline jd_Timer jd_TimerStop(jd_Timer watch);

#define jd_FunctionTimer(fn, timer)\
do { \
timer = jd_TimerStart(); \
fn;\
timer = jd_TimerStop(timer); \
} while (0) \

#ifdef JD_IMPLEMENTATION
#ifdef JD_WINDOWS
#include "win32_jd_timer.c"
#endif
#endif

#endif //JD_TIMER_H
