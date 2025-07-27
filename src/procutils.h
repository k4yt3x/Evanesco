#pragma once

#include <windows.h>

#include <QString>

namespace ProcUtils {
QString getProcessArchitecture(DWORD pid);
QString getProcessExecutablePath(DWORD pid);
HWND getProcessMainWindowHandle(DWORD pid);
bool is64BitProcess(HANDLE hProcess);
bool getExportRva32(LPCWSTR path32, LPCSTR functionName, DWORD* pRva);
bool getRemoteAddress32(DWORD pid, LPCSTR functionName, LPCSTR moduleName, DWORD* pAddr32);
bool isProcessElevated(HANDLE hProcess);
}  // namespace ProcUtils
