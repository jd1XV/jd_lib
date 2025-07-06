void jd_DebugPrint(jd_String string) {
    OutputDebugStringA(string.mem);
}