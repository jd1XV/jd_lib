b32 jd_DiskPathExists(jd_String path) {
    return PathFileExistsA(path.mem);
}

b32 jd_DiskPathRename(jd_String old_path, jd_String new_path) {
    return MoveFile(old_path.mem, new_path.mem);
}

b32 jd_DiskPathCopy(jd_String dst_path, jd_String src_path, b32 fail_if_exists) {
    return CopyFile(src_path.mem, dst_path.mem, fail_if_exists);
}

b32 jd_DiskPathDelete(jd_String path) {
    return DeleteFileA(path.mem);
}

u64 jd_DiskGetFileLastMod(jd_String path) {
    WIN32_FILE_ATTRIBUTE_DATA data = {0};
    
    b32 success = GetFileAttributesExA(path.mem, GetFileExInfoStandard, &data);
    if (!success) {
        jd_LogError("Could not get file attributes!", jd_Error_FileNotFound, jd_Error_Fatal);
    }
    
    ULARGE_INTEGER u;
    u.LowPart = data.ftLastWriteTime.dwLowDateTime;
    u.HighPart = data.ftLastWriteTime.dwHighDateTime;
    
    u.QuadPart -= 116444736000000000LL;
    u.QuadPart /= 10000000LL;
    
    return u.QuadPart;
}

//jd_StringList jd_DiskDirectoryListFiles(jd_String path, jd_String extension, b32 recursive);

jd_File jd_DiskFileReadFromPath(jd_Arena* arena, jd_String path, b32 null_terminate) {
    jd_File file = {0};
    
    SYSTEM_INFO sysinfo = {0};
    GetSystemInfo(&sysinfo);
    
    u32 granularity = sysinfo.dwAllocationGranularity;
    
    HANDLE handle = CreateFileA(path.mem, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        // error
        return file;
    }
    
    LARGE_INTEGER size = {0};
    
    if (GetFileSizeEx(handle, &size) == 0) {
        return file;
    }
    
    file.size = size.QuadPart;
    
    HANDLE fmo_handle = CreateFileMappingA(handle, NULL, PAGE_READONLY, 0, 0, NULL);
    
    u8* view = MapViewOfFile(fmo_handle, FILE_MAP_READ, 0, 0, 0);
    
    if (null_terminate)
        file.size += 1;
    
    file.mem = jd_ArenaAlloc(arena, sizeof(u8) * file.size);
    jd_MemCpy(file.mem, view, file.size);
    
    UnmapViewOfFile(view);
    CloseHandle(fmo_handle);
    CloseHandle(handle);
    
    return file;
}

b32 jd_DiskWriteFileToPath(jd_File file, jd_String path) {
    HANDLE handle = CreateFileA(path.mem, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        // err
        return false;
    }
    
    u32 size_high = jd_GetHiWordU64(file.size);
    u32 size_low = jd_GetLoWordU64(file.size);
    
    HANDLE fmo_handle = CreateFileMappingA(handle, NULL, PAGE_READWRITE, jd_GetHiWordU64(file.size), jd_GetLoWordU64(file.size), NULL);
    if (fmo_handle == INVALID_HANDLE_VALUE) {
        // err
        return false;
    }
    u8* view = (u8*)MapViewOfFile(fmo_handle, FILE_MAP_WRITE, 0, 0, 0);
    jd_MemCpy(view, file.mem, file.size);
    FlushViewOfFile(view, file.size);
    UnmapViewOfFile(view);
    
    CloseHandle(fmo_handle);
    CloseHandle(handle);
    return 0;
}
