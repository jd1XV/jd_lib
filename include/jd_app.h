
/* date = February 27th 2024 4:11 pm */

#ifndef JD_APP_H
#define JD_APP_H

#ifndef JD_UNITY_H
#include "jd_defs.h"
#include "jd_sysinfo.h"
#include "jd_error.h"
#include "jd_math.h"
#include "jd_helpers.h"
#include "jd_memory.h"
#include "jd_string.h"
#include "jd_hash.h"
#include "jd_locks.h"
#include "jd_unicode.h"
#include "jd_data_structures.h"
#include "jd_file.h"
#include "jd_disk.h"
#include "jd_input.h"
#include "jd_renderer.h"
#include "jd_timer.h"
#include "jd_input.h"
#endif

/*

Reloadable functions:

#ifdef JD_APP_RELOADABLE
jd_ReloadableFunction
#endif

*/

#define JD_APP_MAX_WINDOWS 2048

typedef struct jd_Window jd_Window;
jd_ExportFn jd_V2F jd_WindowGetDrawSize(jd_Window* window);
jd_ExportFn jd_V2F jd_WindowGetDrawOrigin(jd_Window* window);
jd_ExportFn jd_V2F jd_WindowGetScaledSize(jd_Window* window);
jd_ExportFn f64 jd_WindowGetDPIScale(jd_Window* window);

typedef struct jd_App jd_App;
jd_App* jd_AppCreate(struct jd_AppConfig* config);

typedef void (*jd_AppWindowFunctionPtr)(struct jd_Window* window);

typedef enum jd_AppMode {
    JD_AM_STATIC,
    JD_AM_RELOADABLE,
    JD_AM_COUNT
} jd_AppMode;

typedef struct jd_AppConfig {
    jd_AppMode mode;
    jd_String package_name;
} jd_AppConfig;

typedef enum jd_TitleBarStyle {
    jd_TitleBarStyle_Platform,
    jd_TitleBarStyle_Right,
    jd_TitleBarStyle_Left,
    jd_TitleBarStyle_Count
} jd_TitleBarStyle;

typedef struct jd_TitleBarResult {
    b8 caption_clicked;
    b8 caption_right_clicked;
    b8 maximize_clicked;
    b8 maximize_hovered;
    b8 minimize_clicked;
    b8 close_clicked;
    b8 top_clicked;
    
    jd_V2F size;
} jd_TitleBarResult;

typedef enum jd_Cursor {
    jd_Cursor_Default,
    jd_Cursor_Resize_H,
    jd_Cursor_Resize_V,
    jd_Cursor_Resize_Diagonal_TL_BR,
    jd_Cursor_Resize_Diagonal_TR_BL,
    jd_Cursor_Hand,
    jd_Cursor_Text,
    jd_Cursor_Loading,
    jd_Cursor_Count
} jd_Cursor;

typedef void (*jd_TitleBarFunctionPtr)(struct jd_Window* window);
#define jd_TitleBarFunction(x) jd_ExportFn void x (struct jd_Window* window)

typedef struct jd_WindowConfig {
    jd_App* app;
    jd_String title;
    jd_String id_str;
    jd_AppWindowFunctionPtr function_ptr;
    jd_String function_name;
    
    jd_TitleBarStyle titlebar_style;
    jd_TitleBarFunctionPtr titlebar_function_ptr;
} jd_WindowConfig;

typedef struct jd_Window jd_Window;

void       jd_AppUpdatePlatformWindows(jd_App* app);
jd_Window* jd_AppCreateWindow(jd_WindowConfig* config);
b32        jd_AppWindowIsActive(jd_Window* window);
void       jd_AppPlatformCloseWindow(jd_Window* window);
b32        jd_AppPlatformUpdate(jd_App* app);
void       jd_AppSetCursor(jd_Cursor cursor);
jd_ExportFn void jd_WindowDrawFPS(jd_Window* window, jd_TextOrigin origin, jd_V2F pos);
jd_ExportFn void jd_AppDefaultTitlebar(jd_Window* window);

#ifdef JD_WINDOWS

#ifdef JD_IMPLEMENTATION
#include "win32_jd_app.c"
#endif

#endif // JD_WINDOWS

#endif //JD_APP_H
