typedef struct jd_UserLock {
    CRITICAL_SECTION lock;
} jd_UserLock;


jd_UserLock* jd_UserLockCreate(jd_Arena* arena, u64 spin_count) {
    jd_UserLock* lock = jd_ArenaAlloc(arena, sizeof(CRITICAL_SECTION));
    if (spin_count == 0)
        InitializeCriticalSection(&lock->lock);
    else
        InitializeCriticalSectionAndSpinCount(&lock->lock, spin_count);
    return lock;
}

void jd_UserLockGet(jd_UserLock* lock) {
    EnterCriticalSection(&lock->lock);
}

void jd_UserLockRelease(jd_UserLock* lock) {
    LeaveCriticalSection(&lock->lock);
}

b32 jd_UserLockTryGet(jd_UserLock* lock) {
    if (TryEnterCriticalSection(&lock->lock) == 0) return false;
    else return true;
}

void jd_UserLockDelete(jd_UserLock* lock) {
    DeleteCriticalSection(&lock->lock);
}

typedef struct jd_RWLock {
    SRWLOCK srw;
} jd_RWLock;

jd_ForceInline jd_RWLock* jd_RWLockCreate(jd_Arena* arena) {
    jd_RWLock* lock = jd_ArenaAlloc(arena, sizeof(*lock));
    InitializeSRWLock(&lock->srw);
    return lock;
}

jd_ForceInline void jd_RWLockInitialize(jd_RWLock* lock) {
    InitializeSRWLock(&lock->srw);
}

jd_ForceInline jd_RWLock* jd_RWLockCreateHeap() {
    jd_RWLock* lock = jd_HeapAlloc(sizeof(*lock));
    InitializeSRWLock(&lock->srw);
    return lock;
}

jd_ForceInline void jd_RWLockGet(jd_RWLock* lock, jd_RWLockMode mode) {
    switch (mode) {
        case jd_RWLock_Read: {
            AcquireSRWLockShared(&lock->srw);
            return;
        } break;
        
        case jd_RWLock_Write: {
            AcquireSRWLockExclusive(&lock->srw);
            return;
        } break;
        
        default: return;
    }
}

jd_ForceInline b32 jd_RWLockTryGet(jd_RWLock* lock, jd_RWLockMode mode) {
    switch (mode) {
        case jd_RWLock_Read: {
            return TryAcquireSRWLockShared(&lock->srw);
        } break;
        
        case jd_RWLock_Write: {
            return TryAcquireSRWLockExclusive(&lock->srw);
        } break;
        
        default: return false;
    }
}

jd_ForceInline void jd_RWLockRelease(jd_RWLock* lock, jd_RWLockMode mode) {
    switch (mode) {
        case jd_RWLock_Read: {
            ReleaseSRWLockShared(&lock->srw);
        } break;
        
        case jd_RWLock_Write: {
            ReleaseSRWLockExclusive(&lock->srw);
        } break;
        
        default: return;
    }
}

