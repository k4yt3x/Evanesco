#include "windowhider.h"

#include <cstddef>
#include <cstdio>

#include <Psapi.h>
#include <Windows.h>
#include <winternl.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QRegularExpression>

#include "procutils.h"

#pragma comment(lib, "psapi.lib")

namespace {
constexpr DWORD kThreadTimeoutMs = 5000;
constexpr SIZE_T kAlignmentBoundary = 16;
constexpr SIZE_T kExtraMemoryPadding = 64;
}  // namespace

// Constructor for initializing with a process ID
WindowHider::WindowHider(DWORD processId) : m_processId(processId), m_targetHwnd(nullptr), m_hProcess(nullptr) {}

// Constructor for initializing with a window handle
WindowHider::WindowHider(HWND windowHandle) : m_processId(0), m_targetHwnd(windowHandle), m_hProcess(nullptr) {}

WindowHider::~WindowHider() {
    Cleanup();
}

bool WindowHider::Initialize() {
    qDebug() << "Initializing WindowHider...";
    m_lastErrorMessage.clear();

    if (!resolveWindowAndProcess()) {
        return false;
    }

    if (!validateTargetProcess()) {
        return false;
    }

    if (!openTargetProcess()) {
        return false;
    }

    qDebug() << "Successfully initialized - Process:" << m_processId
             << "Window:" << QString("0x%1").arg(reinterpret_cast<uintptr_t>(m_targetHwnd), 0, 16);
    return true;
}

void WindowHider::Cleanup() {
    if (m_hProcess) {
        qDebug() << "Cleaning up process handle";
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }
}

bool WindowHider::HideWindow(bool hide) {
    qDebug() << "Setting window visibility - hide:" << hide;
    m_lastErrorMessage.clear();

    // Re-validate that window and process are still valid at operation time
    if (!revalidateTargets()) {
        return false;
    }

    if (!validateInitialization()) {
        return false;
    }

    // Prepare function addresses
    FunctionAddresses addresses;
    if (!resolveFunctionAddresses(addresses)) {
        return false;
    }

    // Prepare injection data
    InjectionData64 injData = createInjectionData(hide, addresses);

    // Determine target process architecture
    const bool is64Bit = ProcUtils::is64BitProcess(m_hProcess);
    qDebug() << "Target process architecture:" << (is64Bit ? "x64" : "x86");

    // Resolve addresses for 32-bit processes if needed
    if (!is64Bit && !resolveAddresses32(injData)) {
        setLastErrorMessage("Failed to resolve function addresses for x86 process");
        return false;
    }

    // Perform the injection
    bool success = performInjection(injData, is64Bit);

    if (success) {
        qDebug() << "Successfully" << (hide ? "hid" : "showed") << "window";
    } else {
        if (m_lastErrorMessage.isEmpty()) {
            setLastErrorMessage("Injection operation failed");
        }
        qDebug() << "Failed to" << (hide ? "hide" : "show") << "window";
    }

    return success;
}

bool WindowHider::resolveWindowAndProcess() {
    // If we have a window handle but no process ID, resolve the process ID from the window handle
    if (m_targetHwnd && !m_processId) {
        if (!IsWindow(m_targetHwnd)) {
            setLastErrorMessage("Window has been closed or is invalid");
            return false;
        }

        // Check if window is still visible
        if (!IsWindowVisible(m_targetHwnd)) {
            setLastErrorMessage("Window is no longer visible (may have been closed)");
            return false;
        }

        DWORD processId;
        if (GetWindowThreadProcessId(m_targetHwnd, &processId) == 0) {
            setWindowsError("Could not get process ID from window handle (window may have been closed)");
            return false;
        }

        // Validate that the process associated with this window is still running
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
        if (!hProcess) {
            setWindowsError(QString("Cannot access process %1 associated with window").arg(processId));
            return false;
        }

        DWORD exitCode;
        bool isRunning = GetExitCodeProcess(hProcess, &exitCode) && (exitCode == STILL_ACTIVE);
        CloseHandle(hProcess);

        if (!isRunning) {
            setLastErrorMessage(QString("Process %1 associated with window has terminated").arg(processId));
            return false;
        }

        m_processId = processId;
        qDebug() << "Resolved process ID from window handle:" << processId;
    }

    // If we have a process ID but no window handle, find the main window
    if (m_processId && !m_targetHwnd) {
        // Get the main window handle for the process
        EnumWindowsData data = {m_processId, nullptr};
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
        m_targetHwnd = data.mainWindow;

        if (!m_targetHwnd) {
            setLastErrorMessage(QString("Could not find main window for process %1").arg(m_processId));
            return false;
        }
        qDebug() << "Found main window for process:"
                 << QString("0x%1").arg(reinterpret_cast<uintptr_t>(m_targetHwnd), 0, 16);
    }

    // Validate we have both process ID and window handle
    if (!m_processId || !m_targetHwnd) {
        setLastErrorMessage("Missing process ID or window handle after resolution");
        return false;
    }

    return true;
}

BOOL CALLBACK WindowHider::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* data = reinterpret_cast<EnumWindowsData*>(lParam);

    DWORD windowProcessId;
    if (GetWindowThreadProcessId(hwnd, &windowProcessId) == 0) {
        return TRUE;
    }

    if (windowProcessId == data->processId && IsWindowVisible(hwnd)) {
        // Find main window - one with no owner
        if (GetWindow(hwnd, GW_OWNER) == nullptr) {
            data->mainWindow = hwnd;
            return FALSE;
        }
    }

    return TRUE;
}

bool WindowHider::validateTargetProcess() {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, m_processId);
    if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
        setWindowsError(QString("Cannot access process %1 for validation").arg(m_processId));
        return false;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(hProcess, &exitCode)) {
        CloseHandle(hProcess);
        setWindowsError("Failed to get process exit code");
        return false;
    }

    CloseHandle(hProcess);

    if (exitCode != STILL_ACTIVE) {
        setLastErrorMessage(QString("Process %1 is not running (exit code: %2)").arg(m_processId).arg(exitCode));
        return false;
    }

    qDebug() << "Target process validation successful";
    return true;
}

bool WindowHider::openTargetProcess() {
    m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_processId);
    if (!m_hProcess) {
        setWindowsError(QString("Failed to open process %1 for injection").arg(m_processId));
        return false;
    }

    qDebug() << "Successfully opened target process with full access";
    return true;
}

bool WindowHider::revalidateTargets() {
    // Check if window is still valid
    if (m_targetHwnd) {
        if (!IsWindow(m_targetHwnd)) {
            setLastErrorMessage("Target window has been closed or destroyed");
            return false;
        }

        if (!IsWindowVisible(m_targetHwnd)) {
            setLastErrorMessage("Target window is no longer visible (may have been minimized or closed)");
            return false;
        }
    }

    // Check if process is still running
    if (m_processId > 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, m_processId);
        if (!hProcess) {
            DWORD error = GetLastError();
            if (error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_FOUND) {
                setLastErrorMessage("Target process no longer exists");
            } else {
                setWindowsError("Cannot access target process for validation");
            }
            return false;
        }

        DWORD exitCode;
        bool isRunning = GetExitCodeProcess(hProcess, &exitCode) && (exitCode == STILL_ACTIVE);
        CloseHandle(hProcess);

        if (!isRunning) {
            setLastErrorMessage("Target process has terminated");
            return false;
        }
    }

    return true;
}

bool WindowHider::validateInitialization() const {
    if (!m_targetHwnd) {
        const_cast<WindowHider*>(this)->setLastErrorMessage("No target window found");
        return false;
    }

    // Re-validate that the window is still valid before performing operations
    if (!IsWindow(m_targetHwnd)) {
        const_cast<WindowHider*>(this)->setLastErrorMessage("Target window has been closed or destroyed");
        return false;
    }

    if (!IsWindowVisible(m_targetHwnd)) {
        const_cast<WindowHider*>(this)->setLastErrorMessage(
            "Target window is no longer visible (may have been minimized or closed)"
        );
        return false;
    }

    // Additional check: verify window still belongs to the expected process
    DWORD currentProcessId = 0;
    if (GetWindowThreadProcessId(m_targetHwnd, &currentProcessId) == 0) {
        const_cast<WindowHider*>(this)->setWindowsError("Failed to get process ID from window (window may be invalid)");
        return false;
    }

    if (currentProcessId != m_processId) {
        const_cast<WindowHider*>(this)->setLastErrorMessage(
            QString("Window now belongs to different process (expected %1, found %2)")
                .arg(m_processId)
                .arg(currentProcessId)
        );
        return false;
    }

    if (!m_hProcess) {
        const_cast<WindowHider*>(this)->setLastErrorMessage("Target process not open");
        return false;
    }

    return true;
}

bool WindowHider::resolveFunctionAddresses(FunctionAddresses& addresses) const {
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (!hUser32) {
        const_cast<WindowHider*>(this)->setWindowsError("Failed to get handle to user32.dll");
        return false;
    }

    addresses.setWindowDisplayAffinity = GetProcAddress(hUser32, "SetWindowDisplayAffinity");
    if (!addresses.setWindowDisplayAffinity) {
        const_cast<WindowHider*>(this)->setWindowsError("Failed to resolve SetWindowDisplayAffinity function");
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        const_cast<WindowHider*>(this)->setWindowsError("Failed to get handle to kernel32.dll");
        return false;
    }

    addresses.getLastError = GetProcAddress(hKernel32, "GetLastError");
    if (!addresses.getLastError) {
        const_cast<WindowHider*>(this)->setWindowsError("Failed to resolve GetLastError function");
        return false;
    }

    qDebug() << "Successfully resolved function addresses";
    return true;
}

bool WindowHider::resolveAddresses32(InjectionData64& injData) const {
    DWORD getLastErrorAddr32 = 0;
    if (!ProcUtils::getRemoteAddress32(m_processId, "GetLastError", "kernel32.dll", &getLastErrorAddr32)) {
        const_cast<WindowHider*>(this)->setLastErrorMessage(
            "Failed to resolve GetLastError address in target x86 process"
        );
        return false;
    }

    DWORD setWindowDisplayAffinityAddr32 = 0;
    if (!ProcUtils::getRemoteAddress32(
            m_processId, "SetWindowDisplayAffinity", "user32.dll", &setWindowDisplayAffinityAddr32
        )) {
        const_cast<WindowHider*>(this)->setLastErrorMessage(
            "Failed to resolve SetWindowDisplayAffinity address in target x86 process"
        );
        return false;
    }

    // Validate that addresses are within 32-bit range and non-zero
    if (getLastErrorAddr32 == 0 || setWindowDisplayAffinityAddr32 == 0) {
        const_cast<WindowHider*>(this)->setLastErrorMessage("Invalid 32-bit function addresses resolved");
        return false;
    }

    // Validate addresses fit in 32-bit space (should not exceed 0xFFFFFFFF)
    if (getLastErrorAddr32 > 0xFFFFFFFF || setWindowDisplayAffinityAddr32 > 0xFFFFFFFF) {
        const_cast<WindowHider*>(this)->setLastErrorMessage(
            QString("Function addresses exceed 32-bit range: GetLastError=0x%1, SetWindowDisplayAffinity=0x%2")
                .arg(getLastErrorAddr32, 0, 16)
                .arg(setWindowDisplayAffinityAddr32, 0, 16)
        );
        return false;
    }

    injData.getLastErrorAddr = reinterpret_cast<FARPROC>(static_cast<uintptr_t>(getLastErrorAddr32));
    injData.setWindowDisplayAffinityAddr =
        reinterpret_cast<FARPROC>(static_cast<uintptr_t>(setWindowDisplayAffinityAddr32));

    qDebug() << "Resolved x86 addresses - GetLastError:" << QString("0x%1").arg(getLastErrorAddr32, 0, 16)
             << ", SetWindowDisplayAffinity:" << QString("0x%1").arg(setWindowDisplayAffinityAddr32, 0, 16);

    return true;
}

InjectionData64 WindowHider::createInjectionData(bool hide, const FunctionAddresses& addresses) const {
    InjectionData64 injData = {};
    injData.hwnd = m_targetHwnd;
    injData.affinity = hide ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    injData.setWindowDisplayAffinityAddr = addresses.setWindowDisplayAffinity;
    injData.result = 0;
    injData.lastError = 0;
    injData.getLastErrorAddr = addresses.getLastError;
    return injData;
}

std::vector<unsigned char> WindowHider::createEmbeddedShellcode(const InjectionData64& injData, bool is64Bit) const {
    return is64Bit ? createEmbeddedShellcode64(injData) : createEmbeddedShellcode32(injData);
}

std::vector<unsigned char> WindowHider::createEmbeddedShellcode64(const InjectionData64& injData) const {
    qDebug() << "Creating x64 embedded shellcode";

    // Start with the base shellcode
    std::vector<unsigned char> shellcode(kShellcodeX64, kShellcodeX64 + kShellcodeX64Size);

    // Calculate data offset (distance from lea instruction to data)
    const size_t dataOffset = shellcode.size();

    // Patch the data offset in the lea instruction
    patchDataOffset(shellcode, dataOffset, true);

    // Append the injection data at the end of the shellcode
    const unsigned char* dataPtr = reinterpret_cast<const unsigned char*>(&injData);
    shellcode.insert(shellcode.end(), dataPtr, dataPtr + sizeof(InjectionData64));

    return shellcode;
}

std::vector<unsigned char> WindowHider::createEmbeddedShellcode32(const InjectionData64& injData) const {
    qDebug() << "Creating x86 embedded shellcode";

    // Start with the base shellcode
    std::vector<unsigned char> shellcode(kShellcodeX86, kShellcodeX86 + kShellcodeX86Size);

    // Convert to x86 format first for validation
    InjectionData32 injDataX86 = convertToInjectionData32(injData);

    // Calculate data offset (distance from add instruction to data)
    const size_t dataOffset = shellcode.size();

    // Patch the data offset in the add instruction
    patchDataOffset(shellcode, dataOffset, false);

    // Append the injection data
    const unsigned char* dataPtr = reinterpret_cast<const unsigned char*>(&injDataX86);
    shellcode.insert(shellcode.end(), dataPtr, dataPtr + sizeof(InjectionData32));

    return shellcode;
}

void WindowHider::patchDataOffset(std::vector<unsigned char>& code, size_t offset, bool is64Bit) const {
    if (is64Bit) {
        // For x64: patch the lea instruction with relative offset
        // Looking for: lea rsi, [rip+0x2F] (0x48, 0x8D, 0x35, 0x2F, 0x00, 0x00, 0x00)
        for (size_t i = 0; i <= code.size() - 7; ++i) {
            if (code[i] == 0x48 && code[i + 1] == 0x8D && code[i + 2] == 0x35) {
                // Calculate relative offset from the end of this instruction to data
                const uint32_t relativeOffset = static_cast<uint32_t>(offset - (i + 7));
                *reinterpret_cast<uint32_t*>(&code[i + 3]) = relativeOffset;
                break;
            }
        }
    } else {
        // For x86: patch the add instruction with offset
        // Looking for: add eax, 0x1E (0x83, 0xC0, 0x1E)
        for (size_t i = 0; i <= code.size() - 3; ++i) {
            if (code[i] == 0x83 && code[i + 1] == 0xC0) {
                // Validate offset fits in single byte
                size_t calculatedOffset = offset - 5;
                if (calculatedOffset > 255) {
                    qDebug() << "x86 offset too large:" << calculatedOffset << "truncating to"
                             << (calculatedOffset & 0xFF);
                }
                const uint8_t offsetValue = static_cast<uint8_t>(calculatedOffset);
                code[i + 2] = offsetValue;
                break;
            }
        }
    }
}

InjectionData32 WindowHider::convertToInjectionData32(const InjectionData64& injData) const {
    InjectionData32 injDataX86 = {};
    injDataX86.hwnd = static_cast<DWORD>(reinterpret_cast<uintptr_t>(injData.hwnd));
    injDataX86.affinity = injData.affinity;
    injDataX86.setWindowDisplayAffinityAddr =
        static_cast<DWORD>(reinterpret_cast<uintptr_t>(injData.setWindowDisplayAffinityAddr));
    injDataX86.result = 0;
    injDataX86.lastError = 0;
    injDataX86.getLastErrorAddr = static_cast<DWORD>(reinterpret_cast<uintptr_t>(injData.getLastErrorAddr));
    return injDataX86;
}

bool WindowHider::performInjection(const InjectionData64& injData, bool is64Bit) {
    // Create embedded shellcode with data
    std::vector<unsigned char> shellcode = createEmbeddedShellcode(injData, is64Bit);

    // Calculate total memory requirements
    const SIZE_T totalSize = shellcode.size() + kExtraMemoryPadding;

    // Allocate memory in target process
    LPVOID remoteMemory =
        VirtualAllocEx(m_hProcess, nullptr, totalSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!remoteMemory) {
        setWindowsError("Failed to allocate memory in target process");
        return false;
    }

    qDebug() << "Allocated" << totalSize << "bytes at"
             << QString("0x%1").arg(reinterpret_cast<uintptr_t>(remoteMemory), 0, 16);

    // Write the complete shellcode with embedded data in a single operation
    if (!writeShellcodeToTarget(remoteMemory, shellcode)) {
        VirtualFreeEx(m_hProcess, remoteMemory, 0, MEM_RELEASE);
        return false;
    }

    // Execute the shellcode
    bool result = executeShellcode(remoteMemory, is64Bit);

    // Clean up allocated memory
    VirtualFreeEx(m_hProcess, remoteMemory, 0, MEM_RELEASE);

    return result;
}

bool WindowHider::writeShellcodeToTarget(LPVOID remoteMemory, const std::vector<unsigned char>& shellcode) const {
    SIZE_T bytesWritten;
    if (!WriteProcessMemory(m_hProcess, remoteMemory, shellcode.data(), shellcode.size(), &bytesWritten)) {
        const_cast<WindowHider*>(this)->setWindowsError("Failed to write shellcode to target process memory");
        return false;
    }

    qDebug() << "Written" << bytesWritten << "bytes of embedded shellcode in single operation";
    return true;
}

bool WindowHider::executeShellcode(LPVOID remoteMemory, bool is64Bit) {
    qDebug() << "Executing embedded shellcode...";

    // Create remote thread
    HANDLE hThread = CreateRemoteThread(
        m_hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteMemory), nullptr, 0, nullptr
    );

    if (!hThread || hThread == INVALID_HANDLE_VALUE) {
        setWindowsError("Failed to create remote thread for shellcode execution");
        return false;
    }

    // Wait for completion with timeout
    const DWORD waitResult = WaitForSingleObject(hThread, kThreadTimeoutMs);
    if (waitResult != WAIT_OBJECT_0) {
        setLastErrorMessage(QString("Remote thread execution timed out (wait result: %1)").arg(waitResult));
        TerminateThread(hThread, 1);
        CloseHandle(hThread);
        return false;
    }

    // Get thread exit code
    DWORD exitCode;
    if (!GetExitCodeThread(hThread, &exitCode)) {
        CloseHandle(hThread);
        setWindowsError("Failed to get thread exit code");
        return false;
    }

    CloseHandle(hThread);

    // Calculate data offset based on architecture
    // For embedded shellcode, data starts immediately after the base shellcode
    const size_t dataOffset = is64Bit ? kShellcodeX64Size : kShellcodeX86Size;

    qDebug() << "Shellcode execution completed:";
    qDebug() << "  Architecture:" << (is64Bit ? "x64" : "x86");
    qDebug() << "  Thread exit code:" << exitCode;
    qDebug() << "  Data offset:" << dataOffset;
    qDebug() << "  Remote memory base:" << QString("0x%1").arg(reinterpret_cast<uintptr_t>(remoteMemory), 0, 16);

    return readAndValidateResults(remoteMemory, is64Bit, exitCode, dataOffset);
}

bool WindowHider::readAndValidateResults(LPVOID remoteMemory, bool is64Bit, DWORD exitCode, size_t dataOffset) const {
    // Calculate address of embedded data
    LPBYTE dataAddr = static_cast<LPBYTE>(remoteMemory) + dataOffset;

    SIZE_T bytesRead;
    bool success = false;

    if (is64Bit) {
        InjectionData64 resultData;
        if (ReadProcessMemory(m_hProcess, dataAddr, &resultData, sizeof(InjectionData64), &bytesRead)) {
            QString lastErrorStr = getWindowsErrorString(resultData.lastError);
            qDebug() << "x64 result - Return value:" << resultData.result << ", LastError:" << lastErrorStr
                     << ", Thread exit code:" << exitCode;
            success = (exitCode == 0) && (resultData.result != 0);
            if (!success && resultData.lastError != 0) {
                const_cast<WindowHider*>(this)->setLastErrorMessage(
                    QString("SetWindowDisplayAffinity failed with error: %1").arg(lastErrorStr)
                );
            }
        } else {
            const_cast<WindowHider*>(this)->setLastErrorMessage(
                "Failed to read x64 injection results from target process"
            );
        }
    } else {
        InjectionData32 resultDataX86;
        if (ReadProcessMemory(m_hProcess, dataAddr, &resultDataX86, sizeof(InjectionData32), &bytesRead)) {
            QString lastErrorStr = getWindowsErrorString(resultDataX86.lastError);
            qDebug() << "x86 result - Return value:" << resultDataX86.result << ", LastError:" << lastErrorStr
                     << ", Thread exit code:" << exitCode;
            success = (exitCode == 0) && (resultDataX86.result != 0);
            if (!success && resultDataX86.lastError != 0) {
                const_cast<WindowHider*>(this)->setLastErrorMessage(
                    QString("SetWindowDisplayAffinity failed with error: %1").arg(lastErrorStr)
                );
            }
        } else {
            const_cast<WindowHider*>(this)->setLastErrorMessage(
                "Failed to read x86 injection results from target process"
            );
        }
    }

    return success;
}

void WindowHider::setLastErrorMessage(const QString& message) {
    m_lastErrorMessage = message;
    qDebug() << "WindowHider Error:" << message;
}

void WindowHider::setWindowsError(const QString& context, DWORD errorCode) {
    if (errorCode == 0) {
        errorCode = GetLastError();
    }

    QString errorMessage = getWindowsErrorString(errorCode);
    m_lastErrorMessage = QString("%1: %2").arg(context, errorMessage);

    qDebug() << "WindowHider Windows Error:" << m_lastErrorMessage;
}

QString WindowHider::formatErrorMessage(DWORD errorCode) const {
    QString errorString;
    LPSTR messageBuffer = nullptr;

    // Try to get error message from system first
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        nullptr
    );

    if (size && messageBuffer) {
        errorString = QString::fromLocal8Bit(messageBuffer).trimmed();
        LocalFree(messageBuffer);
    } else {
        // If system lookup failed, try network messages
        messageBuffer = nullptr;
        size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
            GetModuleHandleA("netmsg.dll"),
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&messageBuffer,
            0,
            nullptr
        );

        if (size && messageBuffer) {
            errorString = QString::fromLocal8Bit(messageBuffer).trimmed();
            LocalFree(messageBuffer);
        }
    }

    // If all lookups failed, provide a descriptive fallback
    if (errorString.isEmpty()) {
        errorString = QString("System error %1").arg(errorCode);
    }

    // Remove any trailing periods and newlines for cleaner display
    errorString = errorString.replace(QRegularExpression("[\\.\\r\\n]+$"), "");

    return errorString;
}

QString WindowHider::getWindowsErrorString(DWORD errorCode) const {
    if (errorCode == 0) {
        return "No error";
    }

    QString errorString = formatErrorMessage(errorCode);
    return QString("%1 (0x%2)").arg(errorString).arg(errorCode, 8, 16, QChar('0'));
}

void WindowHider::logLastError(const char* operation, DWORD errorCode) const {
    if (errorCode == 0) {
        errorCode = GetLastError();
    }

    qDebug() << "Error in" << operation << ":" << errorCode;
    qDebug() << "  ->" << formatErrorMessage(errorCode);
}
