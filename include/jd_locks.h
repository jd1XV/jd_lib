/* date = February 23rd 2024 11:27 pm */

#ifndef JD_LOCKS_H
#define JD_LOCKS_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_memory.h"
#endif

typedef struct jd_UserLock jd_UserLock;
jd_UserLock* jd_UserLockCreate(jd_Arena* arena, u64 spin_count);
void jd_UserLockGet(jd_UserLock* lock);
void jd_UserLockRelease(jd_UserLock* lock);
b32 jd_UserLockTryGet(jd_UserLock* lock);
void jd_UserLockDelete(jd_UserLock* lock);

typedef enum jd_RWLockMode {
    jd_RWLock_Read,
    jd_RWLock_Write,
    jd_RWLock_Count
} jd_RWLockMode;

typedef struct jd_RWLock  jd_RWLock;
jd_ForceInline jd_RWLock* jd_RWLockCreate(jd_Arena* arena);
jd_ForceInline jd_RWLock* jd_RWLockCreateHeap();
jd_ForceInline void       jd_RWLockInitialize(jd_RWLock* lock);
jd_ForceInline void       jd_RWLockGet(jd_RWLock* lock, jd_RWLockMode mode);
jd_ForceInline b32        jd_RWLockTryGet(jd_RWLock* lock, jd_RWLockMode mode);
jd_ForceInline void       jd_RWLockRelease(jd_RWLock* lock, jd_RWLockMode mode);

typedef struct jd_CondVar jd_CondVar;

jd_ForceInline jd_CondVar* jd_CondVarCreate(jd_Arena* arena);
jd_ForceInline void jd_CondVarWait(jd_CondVar* cv, jd_UserLock* lock);
jd_ForceInline void jd_CondVarWakeAll(jd_CondVar* cv);

#ifdef JD_IMPLEMENTATION

#ifdef JD_WINDOWS
#include "win32_jd_locks.c"
#endif

#endif

#endif //JD_LOCKS_H
