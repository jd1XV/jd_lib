/* date = February 22nd 2024 8:05 pm */

#ifndef JD__WINDOWS_H
#define JD__WINDOWS_H

#include <Windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shobjidl_core.h>
#include <intrin.h>
#include <shlwapi.h>
#include <vsstyle.h>
#include <uxtheme.h>

#define JD_DEFAULT_DPI_REFERENCE 96.0f

#define JD_WINDOWS

#define jd_ProcessExit(x) ExitProcess(x)
#define jd_DebugBreak()   DebugBreak();

#ifdef JD_CONSOLE
#define jd_AppMainFn i32 main(i32 argc, const c8** argv)
#else
#define jd_AppMainFn i32 APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hInstPrev, PSTR cmdline, i32 cmdshow)
#endif

#ifdef JD_APP_RELOADABLE
#define jd_AppWindowFunction(x) __declspec(dllexport) void __cdecl x (struct jd_Window* window)
#else
#define jd_AppWindowFunction(x) void x (struct jd_Window* window)
#endif

#endif //JD__WINDOWS_H
