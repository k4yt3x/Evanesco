#include "windowhider.h"

#include <string>

#include <Psapi.h>
#include <winternl.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>

#include "procutils.h"

#pragma comment(lib, "psapi.lib")

namespace {
constexpr DWORD kThreadTimeoutMs = 2000;
}

bool WindowHider::HideProcessWindows(DWORD processId, QString* errorMessage) {
    HWND mainWindow = findMainWindowForProcess(processId);
    if (!mainWindow) {
        setErrorMessage(errorMessage, "Could not find main window for process");
        return false;
    }

    return performWindowOperation(processId, mainWindow, true, errorMessage);
}

bool WindowHider::HideWindow(HWND windowHandle, QString* errorMessage) {
    if (!IsWindow(windowHandle)) {
        setErrorMessage(errorMessage, "Window handle is not valid");
        return false;
    }

    DWORD processId;
    if (!GetWindowThreadProcessId(windowHandle, &processId)) {
        setErrorMessage(errorMessage, "Failed to get process ID from window handle");
        return false;
    }

    return performWindowOperation(processId, windowHandle, true, errorMessage);
}

bool WindowHider::UnhideProcessWindows(DWORD processId, QString* errorMessage) {
    HWND mainWindow = findMainWindowForProcess(processId);
    if (!mainWindow) {
        setErrorMessage(errorMessage, "Could not find main window for process");
        return false;
    }

    return performWindowOperation(processId, mainWindow, false, errorMessage);
}

bool WindowHider::UnhideWindow(HWND windowHandle, QString* errorMessage) {
    if (!IsWindow(windowHandle)) {
        setErrorMessage(errorMessage, "Window handle is not valid");
        return false;
    }

    DWORD processId;
    if (!GetWindowThreadProcessId(windowHandle, &processId)) {
        setErrorMessage(errorMessage, "Failed to get process ID from window handle");
        return false;
    }

    return performWindowOperation(processId, windowHandle, false, errorMessage);
}

bool WindowHider::performWindowOperation(
    DWORD processId,
    HWND windowHandle,
    bool hideOperation,
    QString* errorMessage
) {
    Q_UNUSED(windowHandle);

    if (!validateProcess(processId, errorMessage)) {
        return false;
    }

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        processId
    );

    if (!hProcess) {
        setErrorMessage(
            errorMessage, QString("Failed to open target process: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    BOOL isWow64 = FALSE;
    if (!IsWow64Process(hProcess, &isWow64)) {
        CloseHandle(hProcess);
        setErrorMessage(errorMessage, "Failed to determine target process architecture");
        return false;
    }

    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    bool is64BitSystem = (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    bool is64BitTarget = is64BitSystem && !isWow64;

    bool result = performDllInjection(processId, is64BitTarget, hideOperation, errorMessage);
    CloseHandle(hProcess);

    return result;
}

HWND WindowHider::findMainWindowForProcess(DWORD processId) {
    EnumWindowsData data;
    data.processId = processId;
    data.mainWindow = nullptr;

    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data.mainWindow;
}

BOOL CALLBACK WindowHider::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumWindowsData* data = reinterpret_cast<EnumWindowsData*>(lParam);

    DWORD windowProcessId;
    GetWindowThreadProcessId(hwnd, &windowProcessId);

    if (windowProcessId == data->processId) {
        if (IsWindowVisible(hwnd) && GetParent(hwnd) == nullptr) {
            data->mainWindow = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

bool WindowHider::validateProcess(DWORD processId, QString* errorMessage) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (!hProcess) {
        setErrorMessage(errorMessage, "Failed to open target process for validation");
        return false;
    }

    DWORD exitCode;
    bool isRunning = GetExitCodeProcess(hProcess, &exitCode) && (exitCode == STILL_ACTIVE);
    CloseHandle(hProcess);

    if (!isRunning) {
        setErrorMessage(errorMessage, "Target process is not running");
        return false;
    }

    return true;
}

bool WindowHider::resolveDllPaths(DllPaths& paths, QString* errorMessage) {
    QString appDir = QCoreApplication::applicationDirPath();

    paths.evanesce64Path = (appDir + "/Evanesce64.dll").toStdString();
    paths.evanesce32Path = (appDir + "/Evanesce32.dll").toStdString();
    paths.revela64Path = (appDir + "/Revela64.dll").toStdString();
    paths.revela32Path = (appDir + "/Revela32.dll").toStdString();

    if (!QFile::exists(QString::fromStdString(paths.evanesce64Path))) {
        setErrorMessage(errorMessage, "64-bit Evanesce DLL not found: " + QString::fromStdString(paths.evanesce64Path));
        return false;
    }

    if (!QFile::exists(QString::fromStdString(paths.evanesce32Path))) {
        setErrorMessage(errorMessage, "32-bit Evanesce DLL not found: " + QString::fromStdString(paths.evanesce32Path));
        return false;
    }

    if (!QFile::exists(QString::fromStdString(paths.revela64Path))) {
        setErrorMessage(errorMessage, "64-bit Revela DLL not found: " + QString::fromStdString(paths.revela64Path));
        return false;
    }

    if (!QFile::exists(QString::fromStdString(paths.revela32Path))) {
        setErrorMessage(errorMessage, "32-bit Revela DLL not found: " + QString::fromStdString(paths.revela32Path));
        return false;
    }

    return true;
}

std::string WindowHider::getDllPath(bool is64Bit, bool hideOperation, QString* errorMessage) {
    DllPaths paths;
    if (!resolveDllPaths(paths, errorMessage)) {
        return "";
    }

    if (hideOperation) {
        return is64Bit ? paths.evanesce64Path : paths.evanesce32Path;
    } else {
        return is64Bit ? paths.revela64Path : paths.revela32Path;
    }
}

bool WindowHider::performDllInjection(DWORD processId, bool is64Bit, bool hideOperation, QString* errorMessage) {
    std::string dllPath = getDllPath(is64Bit, hideOperation, errorMessage);
    if (dllPath.empty()) {
        return false;
    }

    return injectDll(processId, dllPath, is64Bit, errorMessage);
}

bool WindowHider::injectDll(DWORD processId, const std::string& dllPath, bool is64BitTarget, QString* errorMessage) {
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        processId
    );

    if (!hProcess) {
        setErrorMessage(errorMessage, "Failed to open target process for injection");
        return false;
    }

    std::wstring wideDllPath(dllPath.begin(), dllPath.end());

    SIZE_T pathSize = (wideDllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remotePathAddr = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!remotePathAddr) {
        CloseHandle(hProcess);
        setErrorMessage(errorMessage, "Failed to allocate memory for DLL path in target process");
        return false;
    }

    if (!WriteProcessMemory(hProcess, remotePathAddr, wideDllPath.c_str(), pathSize, nullptr)) {
        VirtualFreeEx(hProcess, remotePathAddr, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        setErrorMessage(errorMessage, "Failed to write DLL path to target process");
        return false;
    }

    FARPROC loadLibraryAddr = nullptr;
    if (!getRemoteLoadLibraryAddress(processId, is64BitTarget, loadLibraryAddr, errorMessage)) {
        VirtualFreeEx(hProcess, remotePathAddr, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(
        hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryAddr), remotePathAddr, 0, nullptr
    );

    if (!hThread) {
        VirtualFreeEx(hProcess, remotePathAddr, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        setErrorMessage(errorMessage, "Failed to create remote thread");
        return false;
    }

    DWORD exitCode = 0;
    bool success = waitForInjectionCompletion(hThread, exitCode, errorMessage);

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remotePathAddr, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    if (success && exitCode != 0) {
        qDebug() << "DLL injection successful";
        return true;
    } else if (success && exitCode == 0) {
        setErrorMessage(errorMessage, "DLL injection failed - no windows were processed or LoadLibrary failed");
        return false;
    }

    return false;
}

bool WindowHider::getRemoteLoadLibraryAddress(
    DWORD processId,
    bool is64BitTarget,
    FARPROC& address,
    QString* errorMessage
) {
    if (is64BitTarget) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (!hKernel32) {
            setErrorMessage(errorMessage, "Failed to get kernel32.dll handle");
            return false;
        }
        address = GetProcAddress(hKernel32, "LoadLibraryW");
    } else {
        DWORD addr32 = 0;
        if (!ProcUtils::getRemoteAddress32(processId, "LoadLibraryW", "kernel32.dll", &addr32)) {
            setErrorMessage(errorMessage, "Failed to get LoadLibraryW address in 32-bit target process");
            return false;
        }
        address = reinterpret_cast<FARPROC>(static_cast<uintptr_t>(addr32));
    }

    if (!address) {
        setErrorMessage(errorMessage, "Failed to get LoadLibraryW address");
        return false;
    }

    return true;
}

bool WindowHider::waitForInjectionCompletion(HANDLE hThread, DWORD& exitCode, QString* errorMessage) {
    DWORD waitResult = WaitForSingleObject(hThread, kThreadTimeoutMs);

    if (waitResult == WAIT_TIMEOUT) {
        setErrorMessage(errorMessage, "DLL injection timed out");
        return false;
    } else if (waitResult != WAIT_OBJECT_0) {
        setErrorMessage(
            errorMessage,
            QString("Failed to wait for injection completion: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    if (!GetExitCodeThread(hThread, &exitCode)) {
        setErrorMessage(
            errorMessage, QString("Failed to get thread exit code: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    return true;
}

QString WindowHider::formatErrorMessage(DWORD errorCode) {
    QString winError = getWindowsErrorString(errorCode);
    return QString("Error %1: %2").arg(static_cast<qulonglong>(errorCode)).arg(winError);
}

QString WindowHider::getWindowsErrorString(DWORD errorCode) {
    wchar_t* buffer = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr
    );

    QString result = QString::fromWCharArray(buffer).trimmed();
    LocalFree(buffer);
    return result;
}

void WindowHider::setErrorMessage(QString* errorMessage, const QString& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
    qWarning() << "WindowHider:" << message;
}
