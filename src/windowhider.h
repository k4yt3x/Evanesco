#pragma once

#include <string>

#include <windows.h>

#include <QString>

#include "ipcutils.h"

class WindowHider {
   public:
    // Simplified static interface
    static bool HideProcessWindows(DWORD processId, bool persistent = false, QString* errorMessage = nullptr);
    static bool HideWindow(HWND windowHandle, bool persistent = false, QString* errorMessage = nullptr);
    static bool UnhideProcessWindows(DWORD processId, bool persistent = false, QString* errorMessage = nullptr);
    static bool UnhideWindow(HWND windowHandle, bool persistent = false, QString* errorMessage = nullptr);

   private:
    // Helper structure for window enumeration
    struct EnumWindowsData {
        DWORD processId;
        HWND mainWindow;
    };

    // Use shared definitions
    using OperationParams = IpcUtils::OperationParams;

    // Core implementation methods
    static bool performWindowOperation(
        DWORD processId,
        HWND windowHandle,
        bool hideOperation,
        bool persistent,
        QString* errorMessage
    );
    static HWND findMainWindowForProcess(DWORD processId);
    static bool validateProcess(DWORD processId, QString* errorMessage);
    static std::string getInvisibilisDllPath(bool is64Bit, QString* errorMessage);
    static bool performInvisibilisDllInjection(
        DWORD processId,
        bool is64Bit,
        bool hideOperation,
        bool persistent,
        QString* errorMessage
    );
    static bool injectInvisibilisDll(
        DWORD processId,
        bool hideOperation,
        bool persistent,
        bool is64BitTarget,
        QString* errorMessage
    );
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
