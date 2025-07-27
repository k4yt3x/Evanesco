#pragma once

#include <windows.h>

#include <QString>

namespace ProcUtils {
QString getProcessArchitecture(DWORD pid);
QString getProcessExecutablePath(DWORD pid);
HWND getMainWindowHandle(DWORD pid);
bool isProcessVisible(DWORD pid);
bool is64BitProcess(DWORD pid);
bool is64BitProcess(HANDLE hProcess);
HANDLE openProcess(DWORD pid, DWORD desiredAccess = PROCESS_QUERY_LIMITED_INFORMATION);
bool getExportRva32(LPCWSTR path32, LPCSTR functionName, DWORD* pRva);
bool getRemoteAddress32(DWORD pid, LPCSTR functionName, LPCSTR moduleName, DWORD* pAddr32);
bool isCurrentProcessAdmin();
}  // namespace ProcUtils
