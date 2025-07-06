static jd_ErrorLog* _jd_internal_error_log = 0;

static const jd_String _jd_error_print_indicators[jd_Error_Severity_Count] = {
    jd_StrConst("[Fatal]    | "),
    jd_StrConst("[Critical] | "),
    jd_StrConst("[Common]   | "),
    jd_StrConst("[Warning]  | ")
};

void _jd_ErrorDebugPrint(u64 index, jd_DString* d_string) {
    jd_String str = {
        .mem = d_string->mem + index,
        .count = d_string->count - index
    };
    
    jd_DebugPrint(str); 
}

void _jd_ErrorToString(jd_Error* err, jd_DString* d_string) {
    u64 begin_count = d_string->count;
    jd_DStringAppend(d_string, _jd_error_print_indicators[err->severity]);
    jd_DStringAppend(d_string, err->msg);
    jd_DStringAppend(d_string, jd_StrLit("\n"));
    jd_DStringAppend(d_string, jd_StrLit("           | File: "));
    jd_DStringAppend(d_string, err->file);
    jd_DStringAppend(d_string, jd_StrLit("\n"));
    jd_DStringAppend(d_string, jd_StrLit("           | Line: "));
    jd_DStringAppendU32(d_string, err->line, 10);
    jd_DStringAppend(d_string, jd_StrLit("\n"));
    jd_DStringAppend(d_string, jd_StrLit("-------------------\n"));
#ifdef JD_DEBUG
    _jd_ErrorDebugPrint(begin_count, d_string);
#endif
}

void jd_ErrorLogFlushToDisk() {
    jd_DString* out = _jd_internal_error_log->output_string;
    jd_ArenaPopTo(_jd_internal_error_log->arena, _jd_internal_error_log->refresh_pos);
    _jd_internal_error_log->count = 0;
    jd_DStringClear(out);
}

void _jd_LogError(jd_String func, jd_String msg, jd_String filename, u32 line, jd_ErrorCode code, jd_ErrorSeverity severity) {
    jd_UserLockGet(_jd_internal_error_log->lock);
    if (_jd_internal_error_log->count >= _jd_internal_error_log->capacity) {
        jd_ErrorLogFlushToDisk();
    }
    
    jd_Error* error = &_jd_internal_error_log->errors[_jd_internal_error_log->count];
    error->func = jd_StringPush(_jd_internal_error_log->arena, func);
    error->msg = jd_StringPush(_jd_internal_error_log->arena, msg);
    error->file = jd_StringPush(_jd_internal_error_log->arena, filename);
    error->line = line;
    error->code = code;
    error->severity = severity;
    _jd_internal_error_log->count++;
    _jd_ErrorToString(error, _jd_internal_error_log->output_string);
    
    if (error->severity == jd_Error_Fatal) {
        u32 code = error->code;
        jd_ErrorLogFlushToDisk();
        jd_UserLockRelease(_jd_internal_error_log->lock);
#ifdef JD_DEBUG
        jd_DebugBreak();
#endif
        jd_ProcessExit(code);
    }
    
    jd_UserLockRelease(_jd_internal_error_log->lock);
}

b32 jd_ErrorLogInit(jd_String serialized_file_path, u64 max_errors) {
    if (max_errors == 0) max_errors = 1024;
    jd_Arena* arena = jd_ArenaCreate(0, 0);
    _jd_internal_error_log = jd_ArenaAlloc(arena, sizeof(*_jd_internal_error_log));
    _jd_internal_error_log->lock = jd_UserLockCreate(arena, 32);
    _jd_internal_error_log->arena = arena;
    _jd_internal_error_log->errors = jd_ArenaAlloc(arena, sizeof(jd_Error) * max_errors);
    _jd_internal_error_log->serialized_file_path = jd_StringPush(arena, serialized_file_path);
    _jd_internal_error_log->capacity = max_errors;
    _jd_internal_error_log->count = 0;
    _jd_internal_error_log->refresh_pos = arena->pos;
    _jd_internal_error_log->output_string = jd_DStringCreate(_jd_internal_error_log->capacity * 256);
    return true;
}