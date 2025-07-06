typedef struct jd_Thread {
    jd_ThreadFunctionPtr* fn_ptr;
    void* param;
    u64 param_size;
    jd_ThreadState state;
    HANDLE handle;
} jd_Thread;

jd_Thread* jd_ThreadCreate(jd_Arena* arena, jd_ThreadFunctionPtr* ptr, void* param, u64 param_size) {
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