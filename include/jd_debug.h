/* date = March 1st 2024 1:07 am */

#ifndef JD_DEBUG_H
#define JD_DEBUG_H

#ifndef JD_UNITY_H
#include "jd_string.h"
#include "jd_memory.h"
#endif

jd_ExportFn void jd_DebugPrint(jd_String string);

#ifdef JD_IMPLEMENTATION

#ifdef JD_WINDOWS
#include "win32_jd_debug.c"
#endif

#endif 

#endif //JD_DEBUG_H
