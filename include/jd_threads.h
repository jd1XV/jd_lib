/* date = July 4th 2024 4:52 pm */

#ifndef JD_THREADS_H
#define JD_THREADS_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_app.h"
#endif

typedef enum jd_ThreadState {
    jd_ThreadStateInactive,
    jd_ThreadStateActive,
    jd_ThreadStateWaiting,
    jd_ThreadState_Count,
} jd_ThreadState;

typedef struct jd_Thread jd_Thread;
typedef struct jd_ThreadPool jd_ThreadPool;

typedef void (*jd_ThreadFunctionPtr)(void* param);
#define jd_ThreadFunction(x, param_name) void x (void* param_name)

jd_Thread* jd_ThreadCreate(jd_Arena* arena, jd_ThreadFunctionPtr* ptr, void* param, u64 param_size);
void       jd_ThreadClose(jd_Thread* thread, u32 code);


#ifdef JD_IMPLEMENTATION
#ifdef JD_WINDOWS
#include "win32_jd_threads.c"
#endif
#endif

#endif //JD_THREADS_H
