/* date = March 26th 2024 2:24 am */

#ifndef JD_UNITY_H
#define JD_UNITY_H

//-
// This file can be used to do a fast unity build of the library by defining JD_IMPLEMENTATION. 
// Alternatively, you can use this as a catch-all include for the entire library, though I imagine
// that would make the preprocessor do a bunch of unnecessary work.
//-

#define JD_UNITY_IMP_CHECK JD_IMPLEMENTATION

#ifdef JD_UNITY_IMP_CHECK
#undef JD_IMPLEMENTATION
#endif

#include "jd_defs.h"
#include "jd_math.h"
#include "jd_helpers.h"
#include "jd_memory.h"
#include "jd_string.h"
#include "jd_hash.h"
#include "jd_locks.h"
#include "jd_error.h"
#include "jd_sysinfo.h"
#include "jd_unicode.h"
#include "jd_data_structures.h"
#include "jd_file.h"
#include "jd_disk.h"
#include "jd_input.h"
#include "jd_renderer.h"
#include "jd_app.h"
#include "jd_ui.h"
#include "jd_timer.h"
#include "jd_databank.h"
#include "jd_threads.h"
#include "jd_xml.h"

#ifdef JD_DEBUG
#include "jd_debug.h"
#endif

#ifdef JD_UNITY_IMP_CHECK

#ifdef JD_WINDOWS
#include "win32_jd_locks.c"
#include "win32_jd_memory.c"
#include "win32_jd_sysinfo.c"
#include "win32_jd_disk.c"
#include "win32_jd_app.c"
#include "win32_jd_timer.c"
#include "win32_jd_threads.c"

#ifdef JD_DEBUG
#include "win32_jd_debug.c"
#endif

#include "jd_string.c"
#include "jd_file.c"
#include "jd_error.c"
#include "jd_unicode.c"
#include "jd_data_structures.c"
#include "jd_renderer.c"
#include "jd_math.c"
#include "jd_ui.c"
#include "jd_hash.c"
#include "jd_databank.c"

#endif

#endif



#endif //JD_UNITY_H
