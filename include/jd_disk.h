/* date = February 27th 2024 0:03 pm */

#ifndef JD_DISK_H
#define JD_DISK_H

#ifndef JD_UNITY_H
#include "jd_string.h"
#include "jd_memory.h"
#include "jd_file.h"
#endif

jd_ExportFn b32 jd_DiskPathExists(jd_String path);
jd_ExportFn b32 jd_DiskPathRename(jd_String old_path, jd_String new_path);
jd_ExportFn b32 jd_DiskPathCopy(jd_String dst_path, jd_String src_path, b32 fail_if_exists);
jd_ExportFn b32 jd_DiskPathDelete(jd_String path);
jd_ExportFn u64 jd_DiskGetFileLastMod(jd_String path);
//jd_StringList jd_DiskDirectoryListFiles(jd_String path, jd_String extension, b32 recursive);

jd_ExportFn jd_File jd_DiskFileReadFromPath(jd_Arena* arena, jd_String path, b32 null_terminate);
jd_ExportFn b32     jd_DiskWriteFileToPath(jd_File file, jd_String path);


#ifdef JD_IMPLEMENTATION

#ifdef JD_WINDOWS
#include "win32_jd_disk.c"
#endif

#endif


#endif //JD_DISK_H
