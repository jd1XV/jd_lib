/* date = March 12th 2024 11:59 pm */
#ifndef JD_SYSINFO_H
#define JD_SYSINFO_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_error.h"
#endif

typedef enum jd_CPUFlags {
    jd_CPUFlags_None = 0,
    jd_CPUFlags_SupportsSSE    = 1 << 0,
    jd_CPUFlags_SupportsSSE2   = 1 << 1,
    jd_CPUFlags_SupportsSSE3   = 1 << 2,
    jd_CPUFlags_SupportsSSE41  = 1 << 3,
    jd_CPUFlags_SupportsSSE42  = 1 << 4,
    jd_CPUFlags_SupportsSSE4A  = 1 << 5,
    jd_CPUFlags_SupportsAVX    = 1 << 6,
    jd_CPUFlags_SupportsAVX2   = 1 << 7,
} jd_CPUFlags;

typedef struct jd_SysInfo {
    jd_CPUFlags cpu_flags;
    b8 fetched;
} jd_SysInfo;

void jd_SysInfoFetch();
jd_CPUFlags jd_SysInfoGetCPUFlags();

#define jd_CPUFlagIsSet(field, flag) ((field & flag) != 0)

#ifdef JD_IMPLEMENTATION
#ifdef JD_WINDOWS
#include "win32_jd_sysinfo.c"
#endif
#endif

#endif //JD_SYSINFO_H
