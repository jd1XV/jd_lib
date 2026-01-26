#include "dep/glad/glad_wgl.h"
#include "jd_icon_font.h"

static const jd_String app_manifest = jd_StrConst("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>" \
                                                  "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\" xmlns:asmv3=\"urn:schemas-microsoft-com:asm.v3\">" \
                                                  "<asmv3:application>" \
                                                  "<asmv3:windowsSettings>" \
                                                  "<dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">true</dpiAware>" \
                                                  "<dpiAwareness xmlns=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">PerMonitorV2</dpiAwareness>" \
                                                  "</asmv3:windowsSettings>" \
                                                  "</asmv3:application>" \
                                                  "</assembly>");

#define JD_APP_MAX_PACKAGE_NAME_LENGTH KILOBYTES(1)

LRESULT CALLBACK jd_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

typedef struct jd_App {
    jd_Arena* arena;
    jd_UserLock* lock;
    jd_String package_name;
    
    HINSTANCE instance;
    
    struct jd_Window* windows[JD_APP_MAX_WINDOWS];
    u64 window_count;
    
    b32 renderer_initialized;
    
    jd_AppMode mode;
    HMODULE reloadable_dll;
    jd_Cursor cursor;
    jd_String lib_file_name;
    jd_String lib_copied_file_name;
    u64 reloadable_dll_file_time;
    
    HGLRC ogl_context;
    
    PIXELFORMATDESCRIPTOR pixel_format_descriptor;
} jd_App;

struct jd_Window {
    jd_App* app;
    jd_Arena* arena;
    jd_Arena* frame_arena;
    jd_V2F pos;
    jd_V2F size;
    jd_V2F titlebar_size;
    jd_V2I pos_i;
    jd_V2I size_i;                                                                      
    jd_DArray* input_events; // type: jd_InputEvent (jd_input.h)
    
    jd_TitleBarStyle titlebar_style;
    jd_TitleBarFunctionPtr titlebar_function_ptr;
    jd_TitleBarResult titlebar_result;
    
    jd_AppWindowFunctionPtr func;
    jd_String function_name;
    
    HWND handle;
    WNDCLASSA wndclass;
    jd_String wndclass_str;
    
    HDC device_context;
    
    f32 dpi_scaling;
    
    f32 frame_counter;
    f32 frame_time;
    jd_Timer frame_watch;
    jd_DString* fps_counter_string;
    
    jd_UIViewport* titlebar_viewport;
    
    jd_String title;
    b8 closed;
};

b32 jd_AppWindowIsMaximized(jd_Window* window) {
    WINDOWPLACEMENT placement = {0};
    placement.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement(window->handle, &placement)) {
        return (placement.showCmd == SW_SHOWMAXIMIZED);
    } else return false;
}

void jd_AppSetCursor(jd_Cursor cursor) {
    switch (cursor) {
        case jd_Cursor_Default: {
            HCURSOR win_cursor = LoadCursorA(NULL, IDC_ARROW);
            SetCursor(win_cursor);
            break;
        }
        
        case jd_Cursor_Resize_H: {
            HCURSOR win_cursor = LoadCursorA(NULL, IDC_SIZEWE);
            SetCursor(win_cursor);
            break;
        }
        
        case jd_Cursor_Resize_V: {
            HCURSOR win_cursor = LoadCursorA(NULL, IDC_SIZENS);
            SetCursor(win_cursor);
            break;
        }
        
        case jd_Cursor_Resize_Diagonal_TL_BR: {
            HCURSOR win_cursor = LoadCursorA(NULL, IDC_SIZENWSE);
            SetCursor(win_cursor);
            break;
        }
        
        case jd_Cursor_Resize_Diagonal_TR_BL: {
            HCURSOR win_cursor = LoadCursorA(NULL, IDC_SIZENESW);
            SetCursor(win_cursor);
            break;
        }
        
        case jd_Cursor_Hand: {
            HCURSOR win_cursor = LoadCursorA(NULL, IDC_HAND);
            SetCursor(win_cursor);
            break;
        }
        
        case jd_Cursor_Text: {
            HCURSOR win_cursor = LoadCursorA(NULL, IDC_IBEAM);
            SetCursor(win_cursor);
            break;
        }
        
        case jd_Cursor_Loading: {
            HCURSOR win_cursor = LoadCursorA(NULL, IDC_WAIT);
            SetCursor(win_cursor);
            break;
        }
        
        case jd_Cursor_Count: {
            return;
        }
    }
}

jd_V2F jd_AppGetMousePos(jd_Window* window) {
    POINT p = {0};
    GetCursorPos(&p);
    ScreenToClient(window->handle, &p);
    
    jd_V2F v = {(f32)p.x, (f32)p.y};
    return v;
}

void jd_AppFreeLib(jd_App* app) {
    FreeLibrary(app->reloadable_dll);
}

void jd_AppLoadLib(jd_App* app) {
    jd_DString* package_name_str = jd_DStringCreate(JD_APP_MAX_PACKAGE_NAME_LENGTH);
    if (app->package_name.count + sizeof("jd_app_pkg/_lib.dll.active") > JD_APP_MAX_PACKAGE_NAME_LENGTH) {
        jd_LogError("App name is too long!", jd_Error_OutOfMemory, jd_Error_Fatal);
    }
    
    jd_DStringAppend(package_name_str, jd_StrLit("jd_app_pkg/"));
    jd_DStringAppend(package_name_str, app->package_name);
    jd_DStringAppend(package_name_str, jd_StrLit("_lib"));
    jd_DStringAppend(package_name_str, jd_StrLit(".dll"));
    
    if (app->lib_file_name.count == 0) 
        app->lib_file_name = jd_StringPush(app->arena, jd_DStringGet(package_name_str));
    
    if (!jd_DiskPathExists(app->lib_file_name)) {
        jd_LogError("Could not find specified .dll!", jd_Error_FileNotFound, jd_Error_Fatal);
    }
    
    jd_DStringAppend(package_name_str, jd_StrLit(".active"));
    jd_DiskPathCopy(jd_DStringGet(package_name_str), app->lib_file_name, false);
    
    if (app->lib_copied_file_name.count == 0) 
        app->lib_copied_file_name = jd_StringPush(app->arena, jd_DStringGet(package_name_str));
    
    app->reloadable_dll = LoadLibraryExA(package_name_str->mem, NULL, 0);
    
    for (u64 i = 0; i < app->window_count; i++) {
        jd_Window* window = app->windows[i];
        window->func = (jd_AppWindowFunctionPtr)GetProcAddress(app->reloadable_dll, window->function_name.mem);
    }
    
    jd_DStringRelease(package_name_str);
}

void jd_AppUpdatePlatformWindow(jd_Window* window) {
    jd_ArenaPopTo(window->frame_arena, 0);
    
    window->frame_watch = jd_TimerStop(window->frame_watch);
    window->frame_time = window->frame_watch.stop;
    window->frame_watch = jd_TimerStart();
    
    // get the size
    RECT client_rect = {0};
    GetClientRect(window->handle, &client_rect);
    window->size.w = client_rect.right;
    window->size.h = client_rect.bottom;
    window->size_i.w = (i32)client_rect.right;
    window->size_i.h = (i32)client_rect.bottom;
    
    RECT window_rect = {0};
    GetWindowRect(window->handle, &window_rect);
    window->pos.x = window_rect.left;
    window->pos.y = window_rect.top;
    window->pos_i.x = (i32)window_rect.left;
    window->pos_i.y = (i32)window_rect.top;
    
    u32 dpi = GetDpiForWindow(window->handle);
    i32 frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
    i32 padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    
    wglMakeCurrent(window->device_context, window->app->ogl_context);
    jd_RendererGet()->current_window = window;
    jd_RendererBegin(window->size);
    window->func(window);
    jd_RendererDraw();
    jd_ArenaPopTo(jd_RendererGet()->frame_arena, 0);
    SwapBuffers(window->device_context);
}

void jd_AppUpdatePlatformWindows(jd_App* app) {
    jd_UserLockGet(app->lock);
    static u64 reload_delay = 0;
    if (app->mode == JD_AM_RELOADABLE) {
        u64 last_reload_time = app->reloadable_dll_file_time;
        u64 current_reload_time = jd_DiskGetFileLastMod(app->lib_file_name);
        
        if (last_reload_time < current_reload_time) {
            if (reload_delay && reload_delay % 32 == 0) {
                jd_AppFreeLib(app);
                jd_AppLoadLib(app);
                app->reloadable_dll_file_time = current_reload_time;
                reload_delay = 0;
            } else {
                reload_delay++;
            }
            
        }
    }
    
    for (u64 i = 0; i < app->window_count; i++) {
        jd_Window* window = app->windows[i];
        
#if 0 // TODO: Need to support snap layout by hovering maximize
        if (window->titlebar_result.maximize_hovered) {
            SendMessage(window->handle, WM_NCHITTEST, HTMAXBUTTON, MAKELPARAM(jd_AppGetMousePos(window).x, jd_AppGetMousePos(window).y));
        }
#endif
        
        
        MSG msg = {0};
        while (PeekMessage(&msg, window->handle, 0, 0, PM_REMOVE) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } 
        
        if (window->closed) {
            jd_AppPlatformCloseWindow(window);
        }
    }
    
    for (u64 i = 0; i < app->window_count; i++) {
        jd_Window* window = app->windows[i];
        jd_AppUpdatePlatformWindow(window);
    }
    
    jd_UserLockRelease(app->lock);
}

void jd_AppPlatformCloseWindow(jd_Window* window) {
    jd_App* app = window->app;
    u64 window_index = 0;
    for (window_index; window_index < app->window_count; window_index++) {
        if (app->windows[window_index] == window) {
            jd_ZeroMemory(&app->windows[window_index], sizeof(jd_Window*));
            if (window_index != app->window_count - 1) {
                jd_MemMove(&app->windows[window_index], &app->windows[window_index] + 1, (app->window_count - (window_index + 1)) * sizeof(jd_Window*)); }
            break;
        }
    }
    
    app->window_count--;
    DestroyWindow(window->handle);
    jd_ArenaRelease(window->arena);
    
}

jd_TitleBarFunction(_jd_default_titlebar_function_platform) {
    
}


void jd_AppDefaultTitlebar(jd_Window* window) {
    jd_UIBoxRec* titlebar_parent = 0;
    u32 titlebar_height = 45.0f;
    
    window->titlebar_size.y = 0.0;
    
    {
        const f32 borders = 2.0f;
        SIZE titlebar_size = {0};
        
        HTHEME theme = OpenThemeData(window->handle, L"WINDOW");
        if (GetThemePartSize(theme, NULL, WP_CAPTION, CS_ACTIVE, NULL, TS_TRUE, &titlebar_size) != S_OK) {
            jd_LogError("Couldn't open theme!", jd_Error_MissingResource, jd_Error_Critical);
        } else {
            titlebar_height = titlebar_size.cy + borders;
        }
        
        CloseThemeData(theme);
    }
    
    f32 dpi = GetDpiForWindow(window->handle);
    i32 frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
    i32 padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    
    titlebar_height *= jd_WindowGetDPIScale(window);
    
    jd_UIStyle menu_style = {
        .bg_color = {0.08f, .07f, .07f, 1.0f},
        .bg_color_hovered = {0.12f, 0.12f, 0.12f, 1.0f},
        .bg_color_active  = {0.04f, .04f, .04f, 1.0f},
        .label_color      = {1.0f, 1.0f, 1.0f, 1.0f}
    };
    
    jd_UISize size = {
        .rule = {jd_UISizeRule_Grow, jd_UISizeRule_Fixed},
        .fixed_size = {0.0f, titlebar_height}
    };
    
    jd_UIRegionBegin(jd_StrLit("##jd_default_custom_titlebar"), &menu_style, size, jd_UILayout_LeftToRight, 0, true);
    
    jd_UISize label_size = {
        .rule = {jd_UISizeRule_Grow, jd_UISizeRule_Fixed},
        .fixed_size = {0.0f, titlebar_height}
    };
    
    if (jd_UIButton(window->title, label_size, jd_UIBoxFlags_StaticColor|jd_UIBoxFlags_ActOnClick).l_clicked) {
        SendMessage(window->handle, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(jd_AppGetMousePos(window).x, jd_AppGetMousePos(window).y));
    }
    
    if (jd_UIFixedSizeButton(jd_StrLit(jd_FontIcon_Minus), (jd_V2F){titlebar_height * 1.5f, titlebar_height}, (jd_V2F){0.5, 0.5}).l_clicked) {
        ShowWindow(window->handle, SW_MINIMIZE);
    } 
    
    {
        jd_String label = (jd_AppWindowIsMaximized(window)) ? jd_StrLit(jd_FontIcon_Popup) : jd_StrLit(jd_FontIcon_Plus);
        if (jd_UIFixedSizeButton(label, (jd_V2F){titlebar_height * 1.5f, titlebar_height}, (jd_V2F){0.5, 0.5}).l_clicked) {
            if (jd_AppWindowIsMaximized(window)) {
                ShowWindow(window->handle, SW_RESTORE);
                
            } else {
                ShowWindow(window->handle, SW_MAXIMIZE);
                //SendMessage(window->handle, WM_SIZE, SIZE_MAXIMIZED, 0);
            }
        }
        
    }
    
    if (jd_UIFixedSizeButton(jd_StrLit(jd_FontIcon_Cancel), (jd_V2F){titlebar_height * 1.5f, titlebar_height}, (jd_V2F){0.5, 0.5}).l_clicked) {
        window->closed = true;
    }
    
    jd_UIRegionEnd();
    
#if 0    
    {
        jd_UIBoxConfig config = {0};
        config.style = &menu_style;
        config.string_id = jd_StrLit("titlebar");
        config.label = window->app->package_name;
        config.label_alignment = (jd_V2F){0.5, 0.5};
        config.fixed_position  = (jd_V2F){0.0, 0.0};
        config.size.rule.x = jd_UISizeRule_Grow;
        config.size.rule.y = jd_UISizeRule_Fixed;
        config.size.fixed_size.y = titlebar_height;
        config.act_on_click = true;
        config.clickable = true;
        
        jd_UIResult result = jd_UIBoxBegin(&config);
        if (result.l_clicked) {
            res.caption_clicked = true;
        }
    }
    
    
    b8 left = (window->titlebar_style == jd_TitleBarStyle_Left);
    jd_V2F button_size = {.x = 40.0f, .y = titlebar_height};
    
    {
        jd_UIBoxConfig config = {0};
        config.style = &menu_style;
        config.parent = titlebar_parent;
        config.string_id = jd_StrLit("titlebar_closebutton");
        config.label = jd_StrLit(jd_FontIcon_Cancel);
        config.label_alignment = jd_V2F(0.5, 0.5);
        config.clickable = true;
        jd_UIResult result = jd_UIBoxBegin(&config);
        if (result.l_clicked) {
            res.close_clicked = true;
        }
        
        jd_UIBoxEnd();
        
    }
    
    {
        jd_UIBoxConfig config = {0};
        config.style = &menu_style;
        config.parent = titlebar_parent;
        config.string_id = jd_StrLit("titlebar_maxbutton");
        config.label_alignment = jd_V2F(0.5, 0.5);
        config.label = (jd_AppWindowIsMaximized(window)) ? jd_StrLit(jd_FontIcon_Popup) : jd_StrLit(jd_FontIcon_Plus);
        config.rect.max = button_size;
        config.clickable = true;
        
        if (left)
            config.rect.min.x  = button_size.x;
        else
            config.rect.min.x  = jd_WindowGetScaledSize(window).x - (button_size.x * 2);
        
        config.rect.min.y = 0.0f;
        
        jd_UIResult result = jd_UIBox(&config);
        if (result.l_clicked) {
            res.maximize_clicked = true;
        }
        
    }
    
    {
        jd_UIBoxConfig config = {0};
        config.style = &menu_style;
        config.parent = titlebar_parent;
        config.string_id = jd_StrLit("titlebar_minbutton");
        config.label_alignment = jd_V2F(0.5, 0.5);
        config.label = jd_StrLit(jd_FontIcon_Minus);
        config.rect.max = button_size;
        config.clickable = true;
        
        if (left)
            config.rect.min.x  = button_size.x * 2;
        else
            config.rect.min.x  = jd_WindowGetScaledSize(window).x - (button_size.x * 3);
        
        config.rect.min.y = 0.0f;
        
        jd_UIResult result = jd_UIBox(&config);
        if (result.l_clicked) {
            res.minimize_clicked = true;
        }
    }
    
    
    if (!jd_AppWindowIsMaximized(window)) {
        jd_UIBoxConfig config = {0};
        config.style = &menu_style;
        config.style->bg_color = (jd_V4F){0.0f, 0.0f, 0.0f, 0.0f};
        config.parent = titlebar_parent;
        config.string_id = jd_StrLit("fake_shadow_titlebar");
        config.rect.max.x = jd_WindowGetScaledSize(window).x;
        config.rect.max.y = padding + frame_y;
        config.rect.min.x  = 0.0f;
        config.rect.min.y  = 0.0f;
        config.act_on_click = true;
        config.static_color = true;
        config.cursor = jd_Cursor_Resize_V;
        config.clickable = true;
        
        jd_UIResult result = jd_UIBox(&config);
    }
    
#endif
    
}

void jd_AppLoadSystemFont(jd_Arena* arena) {
    jd_File segoe_ui = jd_DiskFileReadFromPath(arena, jd_StrLit("C:\\Windows\\Fonts\\segoeui.ttf"), false);
    jd_File icons    = jd_DiskFileReadFromPath(arena, jd_StrLit("assets/jd_font_custom.ttf"), false);
    
    jd_FontCreateEmpty(jd_StrLit("OS_BaseFontWindows"), MEGABYTES(32), 16);
    jd_FontAddTypefaceFromMemory(jd_StrLit("OS_BaseFontWindows"), segoe_ui, &jd_unicode_range_all, 11, 192);
    jd_FontAddTypefaceFromMemory(jd_StrLit("OS_BaseFontWindows"), icons, &jd_unicode_range_all, 11, 192);
}

jd_Window* jd_AppCreateWindow(jd_WindowConfig* config) {
    if (!config) {
        jd_LogError("Window initialized without jd_WindowConfig*", jd_Error_APIMisuse, jd_Error_Fatal);
        return 0;
    }
    
    if (!config->app) {
        jd_LogError("Window initialized without jd_App*", jd_Error_APIMisuse, jd_Error_Fatal);
        return 0;
    }
    
    if (config->app->window_count >= JD_APP_MAX_WINDOWS) {
        jd_LogError("Window count exceeds JD_APP_MAX_WINDOWS", jd_Error_APIMisuse, jd_Error_Critical);
        return 0;
    }
    
    // Register the window class.
    jd_Arena* arena = jd_ArenaCreate(0, 0);
    jd_Window* window = jd_ArenaAlloc(arena, sizeof(*window));
    window->app = config->app;
    window->wndclass_str = jd_StringPush(arena, config->id_str);
    window->title = jd_StringPush(arena, config->title);
    window->arena = arena;
    window->frame_arena = jd_ArenaCreate(0, 0);
    window->input_events = jd_DArrayCreate(MEGABYTES(256) / sizeof(jd_InputEvent), sizeof(jd_InputEvent));
    window->titlebar_style = config->titlebar_style;
    
    switch (config->titlebar_style) {
        case jd_TitleBarStyle_Platform: {
            window->titlebar_function_ptr = _jd_default_titlebar_function_platform;
            break;
        }
        
        case jd_TitleBarStyle_Left:
        case jd_TitleBarStyle_Right: {
            window->titlebar_function_ptr = jd_AppDefaultTitlebar;
            break;
        }
    }
    
    if (config->titlebar_function_ptr) {
        window->titlebar_function_ptr = config->titlebar_function_ptr;
    }
    
    switch (config->app->mode) {
        default: break;
        
        case JD_AM_STATIC: {
            if (!config->function_ptr) {
                jd_LogError("App mode set to JD_AM_STATIC, but no Jd_AppWindowFunctionPtr supplied in jd_WindowConfig", jd_Error_APIMisuse, jd_Error_Fatal);
                return 0;
            }
            
            window->func = config->function_ptr;
        } break;
        
        case JD_AM_RELOADABLE: {
            if (config->function_name.count == 0) {
                jd_LogError("App mode set to JD_AM_RELOADABLE, but no function name supplied in jd_WindowConfig", jd_Error_APIMisuse, jd_Error_Fatal);
                return 0;
            }
            
            window->function_name = jd_StringPush(arena, config->function_name);
            window->func = (jd_AppWindowFunctionPtr)GetProcAddress(config->app->reloadable_dll, window->function_name.mem);
            if (!window->func) {
                jd_LogError("Could not find specified function in .dll!", jd_Error_FileNotFound, jd_Error_Fatal);
            }
            
        } break;
    }
    
    window->dpi_scaling = 1.0f; // TODO: Handle HiDPI
    
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc   = jd_WindowProc;
    wc.hInstance     = config->app->instance;
    wc.lpszClassName = window->wndclass_str.mem;
    wc.cbWndExtra    = sizeof(jd_Window*);
    wc.hbrBackground = NULL;
    wc.hCursor       = NULL;
    
    RegisterClassEx(&wc);
    
    // Create the window
    u32 win_style = 0;
    if (config->titlebar_style == jd_TitleBarStyle_Platform) {
        win_style = WS_OVERLAPPEDWINDOW;
    } else {
        win_style = WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    }
    
    window->handle = CreateWindowExA(
                                     CS_OWNDC|CS_HREDRAW|CS_VREDRAW,                              // Optional window styles.
                                     window->wndclass_str.mem,               // Window class
                                     window->title.mem,                      // Window textc
                                     win_style,                   // Window style
                                     
                                     // Size and position
                                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                     
                                     NULL,       // Parent window    
                                     NULL,       // Menu
                                     config->app->instance,  // Instance handle
                                     NULL // Additional application data
                                     );
    
    
    if (window->handle == NULL) {
        // err
    }
    
    SetWindowLongPtrA(window->handle, 0, (LONG_PTR)window);
    
    if (window->titlebar_style != jd_TitleBarStyle_Platform) {
        RECT size_rect;
        GetWindowRect(window->handle, &size_rect);
        
        // Inform the application of the frame change to force redrawing with the new
        // client area that is extended into the title bar
        
        SetWindowPos(window->handle, NULL,
                     size_rect.left, size_rect.top,
                     size_rect.right - size_rect.left, size_rect.bottom - size_rect.top,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE
                     );
        
    }
    
    window->device_context = GetDC(window->handle);
    
    if (!window->device_context) {
        // TODO: err
    }
    
    HWND dummy_win = 0;
    HDC dummy_hdc = 0;
    HGLRC fake_context = 0;
    
    const i32 pixel_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        0
    };
    
    const i32 ctx_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        0
    };
    
    if (!config->app->renderer_initialized) {
        WNDCLASSEX dummy_wc = {0};
        dummy_wc.cbSize = sizeof(WNDCLASSEX);
        dummy_wc.lpfnWndProc   = DefWindowProc;
        dummy_wc.hInstance     = GetModuleHandle(NULL);
        dummy_wc.lpszClassName = "dummy_wndclass";
        dummy_wc.cbWndExtra    = sizeof(jd_Window*);
        RegisterClassEx(&dummy_wc);
        u32 win_style = WS_OVERLAPPEDWINDOW;
        
        dummy_win = CreateWindowExA(
                                    CS_OWNDC,                              // Optional window styles.
                                    "dummy_wndclass",                      // Window class
                                    "dummy_window",                      // Window textc
                                    win_style,                   // Window style
                                    
                                    // Size and position
                                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                    
                                    NULL,       // Parent window    
                                    NULL,       // Menu
                                    GetModuleHandle(NULL),  // Instance handle
                                    NULL        // Additional application data
                                    );
        
        if (dummy_win == NULL) {
            // err
        }
        
        dummy_hdc = GetDC(dummy_win);
        if (!dummy_hdc) {
            // err
        }
        
        PIXELFORMATDESCRIPTOR pfd = {
            sizeof(PIXELFORMATDESCRIPTOR),
            1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
            PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
            32,                   // Colordepth of the framebuffer.
            0, 0, 0, 0, 0, 0,
            0,
            0,
            0,
            0, 0, 0, 0,
            24,                   // Number of bits for the depthbuffer
            8,                    // Number of bits for the stencilbuffer
            0,                    // Number of Aux buffers in the framebuffer.
            PFD_MAIN_PLANE,
            0,
            0, 0, 0
        };
        
        i32 pf = ChoosePixelFormat(dummy_hdc, &pfd);
        if (pf == 0) {
            // TODO: err
        }
        
        if (!(SetPixelFormat(dummy_hdc, pf, &pfd))) {
            // TODO: err
        }
        
        fake_context = wglCreateContext(dummy_hdc);
        wglMakeCurrent(dummy_hdc, fake_context);
        
        gladLoadGL();
        gladLoadWGL(dummy_hdc);
        
        u32 num_formats = 0;
        
        wglChoosePixelFormatARB(window->device_context, pixel_attribs, 0, 1, &pf, &num_formats);
        SetPixelFormat(window->device_context, pf, &window->app->pixel_format_descriptor);
        config->app->ogl_context = wglCreateContextAttribsARB(window->device_context, 0, ctx_attribs);
        
        wglMakeCurrent(dummy_hdc, 0);
        wglDeleteContext(fake_context);
        
    } else {
        u32 num_formats = 0;
        i32 pf = 0;
        wglChoosePixelFormatARB(window->device_context, pixel_attribs, 0, 1, &pf, &num_formats);
        SetPixelFormat(window->device_context, pf, &window->app->pixel_format_descriptor);
    }
    
    wglMakeCurrent(window->device_context, config->app->ogl_context);
    wglSwapIntervalEXT(0);
    ShowWindow(window->handle, SW_SHOW);
    
    if (!config->app->renderer_initialized) {
        jd_RendererInit();
        jd_AppLoadSystemFont(config->app->arena);
        config->app->renderer_initialized = true;
        DestroyWindow(dummy_win);
    }
    
    jd_UserLockGet(config->app->lock);
    config->app->windows[config->app->window_count] = window;
    config->app->window_count++;
    jd_UserLockRelease(config->app->lock);
    
    return window;
}

b32 jd_AppPlatformUpdate(jd_App* app) {
    jd_AppUpdatePlatformWindows(app);
    return true;
}

b32 jd_AppIsRunning(jd_App* app) {
    return (app->window_count > 0);
}

jd_ForceInline void _jd_GetMods(jd_InputEvent* e) {
    u16 toggled = (1 << 0);
    
    e->mods = 0;
    
    {
        u16 res = GetKeyState(VK_LCONTROL);
        if (jd_BitIsSet(res, 15)) {
            e->mods |= jd_KeyMod_Ctrl;
            e->mods |= jd_KeyMod_LCtrl;
        }
        
    }
    
    {
        u16 res = GetKeyState(VK_RCONTROL);
        if (jd_BitIsSet(res, 15)) {
            e->mods |= jd_KeyMod_Ctrl;
            e->mods |= jd_KeyMod_RCtrl;
        }
    }
    
    {
        u16 res = GetKeyState(VK_LMENU);
        if (jd_BitIsSet(res, 15)) {
            e->mods |= jd_KeyMod_Alt;
            e->mods |= jd_KeyMod_LAlt;
        }
        
    }
    
    {
        u16 res = GetKeyState(VK_RMENU);
        if (jd_BitIsSet(res, 15)) {
            e->mods |= jd_KeyMod_Alt;
            e->mods |= jd_KeyMod_RAlt;
        }
        
    }
    
    {
        u16 res = GetKeyState(VK_LSHIFT);
        if (jd_BitIsSet(res, 15)) {
            e->mods |= jd_KeyMod_Shift;
            e->mods |= jd_KeyMod_LShift;
        }
    }
    
    {
        u16 res = GetKeyState(VK_RSHIFT);
        if (jd_BitIsSet(res, 15)) {
            e->mods |= jd_KeyMod_Shift;
            e->mods |= jd_KeyMod_RShift;
        }
    }
    
}

LRESULT CALLBACK jd_WindowProc(HWND window_handle, UINT msg, WPARAM w_param, LPARAM l_param) {
    static u32 count = 0;
    jd_InputEvent e = {0};
    jd_Window* window = (jd_Window*)GetWindowLongPtrA(window_handle, 0);
    
    switch (msg) {
        
        case WM_SETCURSOR: {
            if (LOWORD(l_param) == HTTOP) {
                return 0;
            }
            break;
        }
        
        case WM_NCCALCSIZE: {
            if (!w_param) return DefWindowProc(window_handle, msg, w_param, l_param);
            if (window->titlebar_style == jd_TitleBarStyle_Platform) {
                return DefWindowProc(window_handle, msg, w_param, l_param);;
            }
            u32 dpi = GetDpiForWindow(window_handle);
            
            i32 frame_x = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
            i32 frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
            i32 padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            
            NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)l_param;
            RECT* requested_client_rect = params->rgrc;
            
            requested_client_rect->right -= frame_x + padding;
            requested_client_rect->left += frame_x + padding;
            requested_client_rect->bottom -= frame_y + padding;
            
            if (jd_AppWindowIsMaximized(window))
                requested_client_rect->top += padding;
            
            return 0;
        }
        
        case WM_NCHITTEST: {
            // Let the default procedure handle resizing areas
            LRESULT hit = DefWindowProc(window_handle, msg, w_param, l_param);
            if (window->titlebar_style == jd_TitleBarStyle_Platform) {
                return hit;
            }
            
            switch (hit) {
                case HTNOWHERE:
                case HTRIGHT:
                case HTLEFT:
                case HTTOPLEFT:
                case HTTOP:
                case HTTOPRIGHT:
                case HTBOTTOMRIGHT:
                case HTBOTTOM:
                case HTBOTTOMLEFT: {
                    return hit;
                }
            }
            
            u32 dpi = GetDpiForWindow(window->handle);
            i32 frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
            i32 padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            POINT cursor_point = {0};
            cursor_point.x = GET_X_LPARAM(l_param);
            cursor_point.y = GET_Y_LPARAM(l_param);
            ScreenToClient(window->handle, &cursor_point);
            if (!jd_AppWindowIsMaximized(window) && cursor_point.y > 0 && cursor_point.y < frame_y + padding) {
                return HTTOP;
            }
            
            
            return HTCLIENT;
        }
        
        case WM_DESTROY: {
            window->closed = true;
            break;
        }
        
        
        case WM_SIZE: {
            if (window && window->app->renderer_initialized) {
                jd_AppUpdatePlatformWindow(window);
            }
            break;
        }
        
        
        case WM_ERASEBKGND: {
            return 0;
        }
        
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP: {
            e.release_event = true;
        }
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN: {
            e.key = jd_MB_Left; 
            switch (msg) {
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP: {
                    e.key = jd_MB_Middle;
                    break;
                }
                
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP: {
                    e.key = jd_MB_Right;
                    break;
                }
            }
            
            _jd_GetMods(&e);
            
            POINT p = {0};
            GetCursorPos(&p);
            ScreenToClient(window_handle, &p);
            
            e.mouse_pos.x = (f32)p.x;
            e.mouse_pos.y = (f32)p.y;
            jd_DArrayPushBack(window->input_events, &e);
            break;
        }
        
        case WM_KEYDOWN:
        case WM_KEYUP: {
            
            if (w_param  < jd_Key_Back) break;
            if (w_param >= jd_Key_Shift && w_param <= jd_Key_Menu) break;
            if (w_param >= jd_Key_LShift && w_param <= jd_Key_RMenu) break;
            
            e.release_event = (l_param & (1 << 31));
            
            if (w_param < jd_Key_Count) {
                
                if (msg == WM_KEYDOWN) {
                    u16 uni[3] = {0};
                    u32 flags = (1 << 2);
                    
                    u16 key_flags = HIWORD(l_param);
                    u8  scancode = LOBYTE(key_flags);
                    u8 kb_state[256] = {0};
                    
                    GetKeyboardState(kb_state);
                    
                    e,scancode = scancode;
                    e.key = w_param;
                    
                    u32 count = ToUnicodeEx(e.key, e.scancode, kb_state, uni, 3, flags, 0);
                    u32 codepoint = jd_UnicodeDecodeUTF16Codepoint(uni, count);
                    e.codepoint = codepoint;
                }
                
            }
            
            _jd_GetMods(&e);
            jd_DArrayPushBack(window->input_events, &e);
            
            break;
        }
        
        case WM_MOUSEMOVE: {
            
            break;
        }
        
        case WM_MOUSEWHEEL: {
            e.scroll_delta.y -= (f32)((f32)GET_WHEEL_DELTA_WPARAM(w_param) / WHEEL_DELTA);
            jd_DArrayPushBack(window->input_events, &e);
            break;
        }
        
        
        case WM_MOUSEHWHEEL: {
            e.scroll_delta.x -= (f32)((f32)GET_WHEEL_DELTA_WPARAM(w_param) / WHEEL_DELTA);
            jd_DArrayPushBack(window->input_events, &e);
            break;
        }
        
        case WM_CHAR: {
            break;
        }
        
    }
    
    return DefWindowProc(window_handle, msg, w_param, l_param);
}

jd_App* jd_AppCreate(jd_AppConfig* config) {
    jd_Arena* arena = jd_ArenaCreate(0, 0);
    jd_App* app = jd_ArenaAlloc(arena, sizeof(*app));
    app->lock = jd_UserLockCreate(arena, 16);
    app->arena = arena;
    app->instance = GetModuleHandle(NULL); 
    app->mode = config->mode;
    app->package_name = jd_StringPush(arena, config->package_name);
    
    if (app->mode == JD_AM_RELOADABLE) {
        if (app->package_name.count + sizeof("jd_app_pkg/.dll.active") > JD_APP_MAX_PACKAGE_NAME_LENGTH) {
            jd_LogError("App name is too long!", jd_Error_OutOfMemory, jd_Error_Fatal);
        }
        
        jd_AppLoadLib(app);
    }
    
    return app;
}

jd_V2F jd_WindowGetDrawSize(jd_Window* window) {
    // get the size
    RECT client_rect = {0};
    GetClientRect(window->handle, &client_rect);
    window->size.w = client_rect.right;
    window->size.h = client_rect.bottom;
    window->size_i.w = (i32)client_rect.right;
    window->size_i.h = (i32)client_rect.bottom;
    
    RECT window_rect = {0};
    GetWindowRect(window->handle, &window_rect);
    window->pos.x = window_rect.left;
    window->pos.y = window_rect.top;
    window->pos_i.x = (i32)window_rect.left;
    window->pos_i.y = (i32)window_rect.top;
    
    jd_V2F draw_size = {window->size.x, window->size.y - window->titlebar_size.y};
    
    return draw_size;
}

jd_V2F jd_WindowGetDrawOrigin(jd_Window* window) {
    return (jd_V2F){0.0, window->titlebar_size.y};
}

jd_ExportFn jd_V2F jd_WindowGetScaledSize(jd_Window* window) {
    f64 scale_factor = jd_WindowGetDPIScale(window);
    return (jd_V2F){window->size.w / scale_factor, window->size.h / scale_factor};
}

u32 jd_WindowGetDPI(jd_Window* window) {
    u32 dpi = GetDpiForWindow(window->handle);
    return dpi;
}

f64 jd_WindowGetDPIScale(jd_Window* window) {
    HMONITOR mon = MonitorFromWindow(window->handle, MONITOR_DEFAULTTONEAREST);
    u32 scale_factor = 100;
    u32 result = 0;
    if (result = GetScaleFactorForMonitor(mon, &scale_factor) != 0) {
        return 1.0;
    } else return (f64)scale_factor / 100.f;
}

b32 jd_AppWindowIsActive(jd_Window* window) {
    HWND handle = GetActiveWindow();
    if (window->handle != handle) return false;
    else return true;
}

f32 jd_WindowGetFrameTime(jd_Window* window) {
    return window->frame_time;
}