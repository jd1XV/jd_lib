/* date = February 22nd 2024 11:50 pm */

#ifndef JD_ERROR_H
#define JD_ERROR_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_locks.h"
#include "jd_string.h"
#include "jd_memory.h"
#endif

typedef enum jd_ErrorCode {
    jd_Error_AccessDenied = 1,
    jd_Error_FileNotFound,
    jd_Error_EmptyPointer,
    jd_Error_OutOfMemory,
    jd_Error_APIMisuse,
    jd_Error_MissingResource,
    jd_Error_BadInput,
    jd_Error_Code_Count
} jd_ErrorCode;

typedef enum jd_ErrorSeverity {
    jd_Error_Fatal,
    jd_Error_Critical,
    jd_Error_Common,
    jd_Error_Warning,
    jd_Error_Severity_Count
} jd_ErrorSeverity;

typedef struct jd_Error {
    jd_String        func;
    jd_String        msg;
    jd_String        file;
    u32              line;
    jd_ErrorCode     code;
    jd_ErrorSeverity severity;
} jd_Error;

typedef struct jd_ErrorLog {
    jd_UserLock* lock;
    jd_Arena*    arena;
    jd_Error*    errors;
    jd_String    serialized_file_path;
    jd_DString*  output_string;
    u64 capacity;
    u64 count;
    u64 refresh_pos;
} jd_ErrorLog;

void _jd_LogError(jd_String func, jd_String msg, jd_String filename, u32 line, jd_ErrorCode code, jd_ErrorSeverity severity);
b32 jd_ErrorLogInit(jd_String serialized_file_path, u64 max_errors);

#define jd_LogError(cmsg, code, severity) _jd_LogError(jd_StrLit(__func__), jd_StrLit(cmsg), jd_StrLit(__FILE__), __LINE__, code, severity)

#ifdef JD_IMPLEMENTATION
#include "jd_error.c"
#endif

#endif //JD_ERROR_H
