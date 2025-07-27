#include "windowhider.h"

#include <memory>
#include <string>

#include <Psapi.h>
#include <winternl.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>

#include "hashutils.h"
#include "ipcutils.h"
#include "procutils.h"
#include "settings.h"
#include "tempfile.h"

namespace {
constexpr DWORD kThreadTimeoutMs = 2000;
}

bool WindowHider::HideProcessWindows(DWORD processId, bool hideTaskbarIcon, QString* errorMessage) {
    HWND mainWindow = findMainWindowForProcess(processId);
    if (!mainWindow) {
        setErrorMessage(errorMessage, "Could not find main window for process");
        return false;
    }

    return performWindowOperation(processId, mainWindow, true, hideTaskbarIcon, errorMessage);
}

bool WindowHider::HideWindow(HWND windowHandle, bool hideTaskbarIcon, QString* errorMessage) {
    if (!IsWindow(windowHandle)) {
        setErrorMessage(errorMessage, "Window handle is not valid");
        return false;
    }

    DWORD processId;
    if (!GetWindowThreadProcessId(windowHandle, &processId)) {
        setErrorMessage(
            errorMessage,
            QString("Failed to get process ID from window handle: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    return performWindowOperation(processId, windowHandle, true, hideTaskbarIcon, errorMessage);
}

bool WindowHider::UnhideProcessWindows(DWORD processId, bool hideTaskbarIcon, QString* errorMessage) {
    HWND mainWindow = findMainWindowForProcess(processId);
    if (!mainWindow) {
        setErrorMessage(errorMessage, "Could not find main window for process");
        return false;
    }

    return performWindowOperation(processId, mainWindow, false, hideTaskbarIcon, errorMessage);
}

bool WindowHider::UnhideWindow(HWND windowHandle, bool hideTaskbarIcon, QString* errorMessage) {
    if (!IsWindow(windowHandle)) {
        setErrorMessage(errorMessage, "Window handle is not valid");
        return false;
    }

    DWORD processId;
    if (!GetWindowThreadProcessId(windowHandle, &processId)) {
        setErrorMessage(
            errorMessage,
            QString("Failed to get process ID from window handle: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    return performWindowOperation(processId, windowHandle, false, hideTaskbarIcon, errorMessage);
}

bool WindowHider::performWindowOperation(
    DWORD processId,
    HWND windowHandle,
    bool hideOperation,
    bool hideTaskbarIcon,
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
        setErrorMessage(
            errorMessage,
            QString("Failed to determine target process architecture: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    bool is64BitSystem = (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
    bool is64BitTarget = is64BitSystem && !isWow64;

    bool result =
        performInvisibilisDllInjection(processId, is64BitTarget, hideOperation, hideTaskbarIcon, errorMessage);
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
        setErrorMessage(
            errorMessage,
            QString("Failed to open target process for validation: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode)) {
        CloseHandle(hProcess);
        setErrorMessage(
            errorMessage, QString("Failed to get process exit code: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    bool isRunning = (exitCode == STILL_ACTIVE);
    CloseHandle(hProcess);

    if (!isRunning) {
        setErrorMessage(errorMessage, "Target process is not running");
        return false;
    }

    return true;
}

std::string WindowHider::getInvisibilisDllPath(bool is64Bit, QString* errorMessage) {
    QString appDir = QCoreApplication::applicationDirPath();
    std::string dllPath = (appDir + (is64Bit ? "/Invisibilis64.dll" : "/Invisibilis32.dll")).toStdString();

    if (!QFile::exists(QString::fromStdString(dllPath))) {
        setErrorMessage(errorMessage, QString("Invisibilis DLL not found: %1").arg(QString::fromStdString(dllPath)));
        return "";
    }

    return dllPath;
}

bool WindowHider::performInvisibilisDllInjection(
    DWORD processId,
    bool is64Bit,
    bool hideOperation,
    bool hideTaskbarIcon,
    QString* errorMessage
) {
    std::string dllPath = getInvisibilisDllPath(is64Bit, errorMessage);
    if (dllPath.empty()) {
        return false;
    }

    return injectInvisibilisDll(processId, hideOperation, hideTaskbarIcon, is64Bit, errorMessage);
}

bool WindowHider::injectInvisibilisDll(
    DWORD processId,
    bool hideOperation,
    bool hideTaskbarIcon,
    bool is64BitTarget,
    QString* errorMessage
) {
    // Create named shared memory with operation parameters
    std::string mappingName = HashUtils::generateMappingName(static_cast<uint32_t>(processId));
    qDebug() << "Creating shared memory mapping:" << QString::fromStdString(mappingName);

    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(OperationParams), mappingName.c_str()
    );

    if (!hMapFile) {
        setErrorMessage(
            errorMessage,
            QString("Failed to create shared memory mapping: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    OperationParams* params =
        static_cast<OperationParams*>(MapViewOfFile(hMapFile, FILE_MAP_WRITE, 0, 0, sizeof(OperationParams)));

    if (!params) {
        CloseHandle(hMapFile);
        setErrorMessage(
            errorMessage, QString("Failed to map view of shared memory: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    qDebug() << "Setting operation parameters - Hide:" << hideOperation << "HideTaskbarIcon:" << hideTaskbarIcon;

    // Set operation parameters using bitwise operations
    params->flags = 0;
    // false=hide, true=unhide
    IpcUtils::setOperationFlag(params->flags, !hideOperation);
    IpcUtils::setHideTaskbarIconFlag(params->flags, hideTaskbarIcon);

    qDebug() << "Final flags value:" << params->flags;
    UnmapViewOfFile(params);

    // Get original DLL path
    std::string originalDllPath = getInvisibilisDllPath(is64BitTarget, errorMessage);
    if (originalDllPath.empty()) {
        CloseHandle(hMapFile);
        return false;
    }

    // Check if we should randomize the DLL filename
    Settings* settings = Settings::instance();
    std::string dllPath = originalDllPath;
    std::unique_ptr<TempFile> tempFile;
    bool useRandomizedName = settings->randomizeDllFileName();

    if (useRandomizedName) {
        // Create a temporary file with randomized name using RAII
        tempFile = std::make_unique<TempFile>(originalDllPath, ".dll");
        if (!tempFile->isValid()) {
            CloseHandle(hMapFile);
            setErrorMessage(
                errorMessage,
                QString("Failed to create temporary DLL file: %1").arg(QString::fromStdString(tempFile->errorMessage()))
            );
            return false;
        }

        dllPath = tempFile->path();
        qDebug() << "Using randomized DLL path:" << QString::fromStdString(dllPath);
    } else {
        qDebug() << "Using original DLL path:" << QString::fromStdString(dllPath);
    }

    // Standard DLL injection
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        processId
    );

    if (!hProcess) {
        CloseHandle(hMapFile);
        setErrorMessage(
            errorMessage,
            QString("Failed to open target process for injection: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    std::wstring wideDllPath(dllPath.begin(), dllPath.end());

    SIZE_T pathSize = (wideDllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remotePathAddr = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!remotePathAddr) {
        CloseHandle(hProcess);
        CloseHandle(hMapFile);
        setErrorMessage(
            errorMessage,
            QString("Failed to allocate memory for DLL path in target process: %1")
                .arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    if (!WriteProcessMemory(hProcess, remotePathAddr, wideDllPath.c_str(), pathSize, nullptr)) {
        VirtualFreeEx(hProcess, remotePathAddr, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        CloseHandle(hMapFile);
        setErrorMessage(
            errorMessage,
            QString("Failed to write DLL path to target process: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    FARPROC loadLibraryAddr = nullptr;
    if (!getRemoteLoadLibraryAddress(processId, is64BitTarget, loadLibraryAddr, errorMessage)) {
        VirtualFreeEx(hProcess, remotePathAddr, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        CloseHandle(hMapFile);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(
        hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryAddr), remotePathAddr, 0, nullptr
    );

    if (!hThread) {
        VirtualFreeEx(hProcess, remotePathAddr, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        CloseHandle(hMapFile);
        setErrorMessage(
            errorMessage, QString("Failed to create remote thread: %1").arg(getWindowsErrorString(GetLastError()))
        );
        return false;
    }

    DWORD exitCode = 0;
    bool success = waitForInjectionCompletion(hThread, exitCode, errorMessage);

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remotePathAddr, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    CloseHandle(hMapFile);

    // TempFile destructor will automatically clean up the temporary file

    if (success && exitCode != 0) {
        qDebug() << "Invisibilis DLL injection successful";
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
            setErrorMessage(
                errorMessage,
                QString("Failed to get kernel32.dll handle: %1").arg(getWindowsErrorString(GetLastError()))
            );
            return false;
        }
        address = GetProcAddress(hKernel32, "LoadLibraryW");
        if (!address) {
            setErrorMessage(
                errorMessage,
                QString("Failed to get LoadLibraryW address: %1").arg(getWindowsErrorString(GetLastError()))
            );
            return false;
        }
    } else {
        DWORD addr32 = 0;
        if (!ProcUtils::getRemoteAddress32(processId, "LoadLibraryW", "kernel32.dll", &addr32)) {
            setErrorMessage(errorMessage, "Failed to get LoadLibraryW address in 32-bit target process");
            return false;
        }
        address = reinterpret_cast<FARPROC>(static_cast<uintptr_t>(addr32));
        if (!address) {
            setErrorMessage(errorMessage, "Failed to get LoadLibraryW address in 32-bit target process");
            return false;
        }
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
