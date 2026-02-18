typedef struct jd_Thread {
    jd_ThreadFunctionPtr fn_ptr;
    void* param;
    u64 param_size;
    jd_ThreadState state;
    HANDLE handle;
} jd_Thread;

typedef struct jd_ThreadPool {
    jd_Arena*  arena;
    jd_Thread** threads;
    u64        count;
    u64        count_closed;
    jd_ThreadPoolJob* job_head;
    jd_ThreadPoolJob* job_tail;
    
    jd_UserLock* lock;
    jd_CondVar*  wake_cond;
    b32          shutting_down;
} jd_ThreadPool;

jd_Thread* jd_ThreadCreate(jd_Arena* arena, jd_ThreadFunctionPtr ptr, void* param, u64 param_size) {
    jd_Thread* t = jd_ArenaAlloc(arena, sizeof(*t));
    t->fn_ptr = ptr;
    t->param = param;
    t->param_size = param_size;
    t->handle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ptr, param, 0, 0);
    
    return t;
}

void jd_ThreadClose(jd_Thread* thread, u32 code) {
    TerminateThread(thread->handle, code);
}

jd_ThreadFunction(_jd_thread_internal_worker_thread, tpp) {
    jd_ThreadPool* tp = tpp;
    while (1) {
        jd_UserLockGet(tp->lock);
        while (!tp->shutting_down && !tp->job_head) {
            jd_CondVarWait(tp->wake_cond, tp->lock);
        }
        
        if (tp->shutting_down) {
            tp->count_closed++;
            jd_UserLockRelease(tp->lock);
            return 0;
        }
        
        jd_ThreadPoolJob* job = tp->job_head;
        jd_ThreadPoolJob* next = job->next;
        
        if (next) tp->job_head = next;
        else {
            tp->job_tail = 0;
            tp->job_head = 0;
        }
        jd_DLinksClear(job);
        
        jd_UserLockRelease(tp->lock);
        job->func(job->param);
        // TODO: generate report id and store it
    }
    
    return 1;
}

jd_ThreadPool* jd_ThreadPoolCreate(u64 thread_count) {
    jd_Arena* arena = jd_ArenaCreate(0, 0);
    jd_ThreadPool* tp = jd_ArenaAlloc(arena, sizeof(*tp));
    tp->arena = arena;
    tp->threads = jd_ArenaAlloc(arena, sizeof(jd_Thread) * thread_count);
    tp->count = thread_count;
    tp->lock = jd_UserLockCreate(arena, 0);
    tp->wake_cond = jd_CondVarCreate(arena);
    
    for (u64 i = 0; i < thread_count; i++) {
        tp->threads[i] = jd_ThreadCreate(arena, _jd_thread_internal_worker_thread, tp, sizeof(*tp));
    }
    
    return tp;
}

void jd_ThreadPoolAddWork(jd_ThreadPool* tp, jd_ThreadPoolJobFunctionPtr func, void* param, u64 param_size) {
    jd_UserLockGet(tp->lock);
    jd_ThreadPoolJob* job = jd_ArenaAlloc(tp->arena, sizeof(*job));
    job->func = func;
    job->param = param;
    job->param_size = param_size;
    if (tp->job_tail) {
        jd_DLinkNext(tp->job_tail, job);
    }
    else {
        tp->job_tail = job;
        tp->job_head = job;
    }
    jd_UserLockRelease(tp->lock);
    jd_CondVarWakeAll(tp->wake_cond);
}

void jd_ThreadPoolDestroy(jd_ThreadPool* tp) {
    jd_UserLockGet(tp->lock);
    tp->shutting_down = true;
    jd_UserLockRelease(tp->lock);
    
    jd_CondVarWakeAll(tp->wake_cond);
    
    while (tp->count_closed != tp->count);
    
    jd_UserLockDelete(tp->lock);
    jd_ArenaRelease(tp->arena);
}

jd_ThreadStatus jd_ThreadGetStatus(jd_Thread* thread) {
    u32 exit = 0;
    jd_ThreadStatus status = {0};
    GetExitCodeThread(thread->handle, &exit);
    status.still_running = (exit == STILL_ACTIVE);
    status.exit_code = exit;
    return status;
}