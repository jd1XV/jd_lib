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

typedef struct jd_ThreadStatus {
    u32 exit_code;
    b8  still_running;
} jd_ThreadStatus;

typedef u64 (*jd_ThreadPoolJobFunctionPtr)(void* param);
#define jd_ThreadPoolJobFunction(name, param_name) u64 name (void* param_name)

typedef struct jd_ThreadPoolJob {
    jd_ThreadPoolJobFunctionPtr func;
    void* param;
    u64 param_size;
    
    u64 report_id;
    
    jd_Node(jd_ThreadPoolJob);
} jd_ThreadPoolJob;

typedef u64 (*jd_ThreadFunctionPtr)(void* param);
#define jd_ThreadFunction(x, param_name) u64 x (void* param_name)

jd_Thread*      jd_ThreadCreate(jd_Arena* arena, jd_ThreadFunctionPtr ptr, void* param, u64 param_size);
void            jd_ThreadClose(jd_Thread* thread, u32 code);
jd_ThreadStatus jd_ThreadGetStatus(jd_Thread* thread);

jd_ThreadPool* jd_ThreadPoolCreate(u64 thread_count);
void           jd_ThreadPoolAddWork(jd_ThreadPool* tp, jd_ThreadPoolJobFunctionPtr func, void* param, u64 param_size);
void           jd_ThreadPoolDestroy(jd_ThreadPool* tp);
u32            jd_ThreadGetExitCode(jd_Thread* thread);


#ifdef JD_IMPLEMENTATION
#ifdef JD_WINDOWS
#include "win32_jd_threads.c"
#endif
#endif


/*

Thread pool status can be managed by a structure passed to and by the threadpool's addwork function -- where it can then be updated after the function ptr returns. It should have a RW lock.

*/

#endif //JD_THREADS_H
