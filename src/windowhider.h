#pragma once

#include <string>

#include <windows.h>

#include <QString>

class WindowHider {
   public:
    // Simplified static interface
    static bool HideProcessWindows(DWORD processId, QString* errorMessage = nullptr);
    static bool HideWindow(HWND windowHandle, QString* errorMessage = nullptr);
    static bool UnhideProcessWindows(DWORD processId, QString* errorMessage = nullptr);
    static bool UnhideWindow(HWND windowHandle, QString* errorMessage = nullptr);

   private:
    // Helper structure for window enumeration
    struct EnumWindowsData {
        DWORD processId;
        HWND mainWindow;
    };

    // Helper structure for DLL paths
    struct DllPaths {
        std::string evanesce64Path;
        std::string evanesce32Path;
        std::string revela64Path;
        std::string revela32Path;
    };

    // Core implementation methods
    static bool performWindowOperation(DWORD processId, HWND windowHandle, bool hideOperation, QString* errorMessage);
    static HWND findMainWindowForProcess(DWORD processId);
    static bool validateProcess(DWORD processId, QString* errorMessage);
    static bool resolveDllPaths(DllPaths& paths, QString* errorMessage);
    static std::string getDllPath(bool is64Bit, bool hideOperation, QString* errorMessage);
    static bool performDllInjection(DWORD processId, bool is64Bit, bool hideOperation, QString* errorMessage);
    static bool injectDll(DWORD processId, const std::string& dllPath, bool is64BitTarget, QString* errorMessage);
    static bool waitForInjectionCompletion(HANDLE hThread, DWORD& exitCode, QString* errorMessage);

    // Utility methods
    static bool
    getRemoteLoadLibraryAddress(DWORD processId, bool is64BitTarget, FARPROC& address, QString* errorMessage);
    static QString formatErrorMessage(DWORD errorCode);
    static QString getWindowsErrorString(DWORD errorCode);
    static void setErrorMessage(QString* errorMessage, const QString& message);

    // Window enumeration callback
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
};
