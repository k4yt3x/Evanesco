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
constexpr DWORD kThreadTimeoutMs = 2000;
constexpr SIZE_T kAlignmentBoundary = 16;
constexpr SIZE_T kExtraMemoryPadding = 64;
}  // namespace

// Shellcode definitions - templates for creating mutable copies
// clang-format off
static const unsigned char kShellcodeX64[] = {
    // Reserve shadow space for function call
    0x48, 0x83, 0xEC, 0x20,       // sub rsp, 32

    // Calculate data address (skip past code to embedded data)
    0x48, 0x8D, 0x35, 0x2F, 0x00, 0x00, 0x00, // lea rsi, [rip+0x2F] (data offset will be patched)

    // Initialize: hwnd = NULL, successCount = 0
    0x31, 0xDB,                   // xor ebx, ebx (hwnd = NULL, use rbx to preserve across calls)
    0x89, 0x5E, 0x20,             // mov [rsi+32], ebx (successCount = 0)

    // Loop start - offset 16 (0x10)
    // loop_start:
    0x4D, 0x31, 0xC0,             // xor r8, r8 (lpClassName = NULL)
    0x4D, 0x31, 0xC9,             // xor r9, r9 (lpWindowName = NULL)
    0x48, 0x31, 0xC9,             // xor rcx, rcx (hwndParent = NULL)
    0x48, 0x89, 0xDA,             // mov rdx, rbx (hwndChildAfter)
    0x48, 0x8B, 0x46, 0x18,       // mov rax, [rsi+24] (FindWindowEx address - offset 24)
    0xFF, 0xD0,                   // call rax
    0x48, 0x89, 0xC3,             // mov rbx, rax (store hwnd in rbx)

    // Check if hwnd is NULL (end of enumeration)
    0x48, 0x85, 0xDB,             // test rbx, rbx
    0x74, 0x15,                   // jz end_loop

    // Call SetWindowDisplayAffinity(hwnd, affinity)
    0x48, 0x89, 0xD9,             // mov rcx, rbx (hwnd parameter)
    0x8B, 0x56, 0x08,             // mov edx, [rsi+8] (affinity - offset 8)
    0x48, 0x8B, 0x46, 0x10,       // mov rax, [rsi+16] (SetWindowDisplayAffinity address - offset 16)
    0xFF, 0xD0,                   // call rax

    // Check if call succeeded
    0x85, 0xC0,                   // test eax, eax
    0x74, 0x03,                   // jz continue_loop (jump if failed)

    // Increment success count
    0xFF, 0x46, 0x20,             // inc dword ptr [rsi+32] (successCount++)

    // continue_loop:
    0xEB, 0xD1,                   // jmp loop_start

    // end_loop:
    // Clean up and exit
    0x48, 0x83, 0xC4, 0x20,       // add rsp, 32
    0xC3,                         // ret
};

static const unsigned char kShellcodeX86[] = {
    // Calculate data address (skip past code to embedded data)
    0xE8, 0x00, 0x00, 0x00, 0x00, // call $+5 (get current EIP)
    0x58,                         // pop eax (now eax = current address)
    0x83, 0xC0, 0x1E,             // add eax, 0x1E (data offset will be patched)
    0x89, 0xC6,                   // mov esi, eax (esi points to data)

    // Initialize successCount = 0
    0x31, 0xC0,                   // xor eax, eax
    0x89, 0x46, 0x10,             // mov [esi+16], eax (successCount = 0 - offset 16)

    // Save original stack pointer
    0x89, 0xE3,                   // mov ebx, esp (save original ESP)

    // Initialize hwnd for first iteration
    0x31, 0xFF,                   // xor edi, edi (hwnd = NULL for first call)

    // Loop start
    // loop_start:
    0x89, 0xDC,                   // mov esp, ebx (restore ESP to original value)

    // Call FindWindowEx with proper stack management
    0x6A, 0x00,                   // push 0 (lpWindowName = NULL)
    0x6A, 0x00,                   // push 0 (lpClassName = NULL)
    0x57,                         // push edi (hwndChildAfter)
    0x6A, 0x00,                   // push 0 (hwndParent = NULL)
    0xFF, 0x56, 0x0C,             // call [esi+12] (FindWindowEx address - offset 12)
    0x89, 0xDC,                   // mov esp, ebx (restore ESP after call)
    0x89, 0xC7,                   // mov edi, eax (hwnd = result)

    // Check if hwnd is NULL (end of enumeration)
    0x85, 0xFF,                   // test edi, edi
    0x74, 0x12,                   // jz end_loop

    // Call SetWindowDisplayAffinity with proper stack management
    0xFF, 0x76, 0x04,             // push [esi+4] (affinity - offset 4)
    0x57,                         // push edi (hwnd)
    0xFF, 0x56, 0x08,             // call [esi+8] (SetWindowDisplayAffinity address - offset 8)
    0x89, 0xDC,                   // mov esp, ebx (restore ESP after call)

    // Check if call succeeded
    0x85, 0xC0,                   // test eax, eax
    0x74, 0x03,                   // jz continue_loop (jump if failed)

    // Increment success count
    0xFF, 0x46, 0x10,             // inc dword ptr [esi+16] (successCount++)

    // continue_loop:
    0xEB, 0xDA,                   // jmp loop_start

    // end_loop:
    // Exit
    0xC3,                         // ret
};
// clang-format on

static const size_t kShellcodeX64Size = sizeof(kShellcodeX64);
static const size_t kShellcodeX86Size = sizeof(kShellcodeX86);

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

    addresses.findWindowEx = GetProcAddress(hUser32, "FindWindowExA");
    if (!addresses.findWindowEx) {
        const_cast<WindowHider*>(this)->setWindowsError("Failed to resolve FindWindowExA function");
        return false;
    }

    qDebug() << "Successfully resolved function addresses:"
             << "SetWindowDisplayAffinity="
             << QString("0x%1").arg(reinterpret_cast<uintptr_t>(addresses.setWindowDisplayAffinity), 0, 16)
             << "FindWindowEx=" << QString("0x%1").arg(reinterpret_cast<uintptr_t>(addresses.findWindowEx), 0, 16);
    return true;
}

bool WindowHider::resolveAddresses32(InjectionData64& injData) const {
    DWORD setWindowDisplayAffinityAddr32 = 0;
    if (!ProcUtils::getRemoteAddress32(
            m_processId, "SetWindowDisplayAffinity", "user32.dll", &setWindowDisplayAffinityAddr32
        )) {
        const_cast<WindowHider*>(this)->setLastErrorMessage(
            "Failed to resolve SetWindowDisplayAffinity address in target x86 process"
        );
        return false;
    }

    DWORD findWindowExAddr32 = 0;
    if (!ProcUtils::getRemoteAddress32(m_processId, "FindWindowExA", "user32.dll", &findWindowExAddr32)) {
        const_cast<WindowHider*>(this)->setLastErrorMessage(
            "Failed to resolve FindWindowExA address in target x86 process"
        );
        return false;
    }

    // Validate that addresses are within 32-bit range and non-zero
    if (setWindowDisplayAffinityAddr32 == 0 || findWindowExAddr32 == 0) {
        const_cast<WindowHider*>(this)->setLastErrorMessage("Invalid 32-bit function addresses resolved");
        return false;
    }

    // Store as raw DWORD values to avoid pointer size issues
    // We'll cast them properly during data structure creation
    injData.setWindowDisplayAffinityAddr =
        reinterpret_cast<FARPROC>(static_cast<uintptr_t>(setWindowDisplayAffinityAddr32));
    injData.findWindowExAddr = reinterpret_cast<FARPROC>(static_cast<uintptr_t>(findWindowExAddr32));

    qDebug() << "Resolved x86 addresses - SetWindowDisplayAffinity:"
             << QString("0x%1").arg(setWindowDisplayAffinityAddr32, 0, 16)
             << "FindWindowEx:" << QString("0x%1").arg(findWindowExAddr32, 0, 16);

    return true;
}

InjectionData64 WindowHider::createInjectionData(bool hide, const FunctionAddresses& addresses) const {
    // Validate function addresses
    if (!addresses.setWindowDisplayAffinity || !addresses.findWindowEx) {
        qDebug() << "Invalid function addresses detected";
        return {};
    }

    InjectionData64 injData = {};
    injData.hwnd = NULL;  // Not used anymore as we enumerate all windows
    injData.affinity = hide ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    injData.setWindowDisplayAffinityAddr = addresses.setWindowDisplayAffinity;
    injData.findWindowExAddr = addresses.findWindowEx;
    injData.successCount = 0;

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

    // Extract the 32-bit addresses that were stored as uintptr_t values
    injDataX86.setWindowDisplayAffinityAddr =
        static_cast<DWORD>(reinterpret_cast<uintptr_t>(injData.setWindowDisplayAffinityAddr));
    injDataX86.findWindowExAddr = static_cast<DWORD>(reinterpret_cast<uintptr_t>(injData.findWindowExAddr));
    injDataX86.successCount = 0;

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
    qDebug() << "Executing embedded shellcode at address"
             << QString("0x%1").arg(reinterpret_cast<uintptr_t>(remoteMemory), 0, 16);

    // Create remote thread
    HANDLE hThread = CreateRemoteThread(
        m_hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteMemory), nullptr, 0, nullptr
    );

    if (!hThread || hThread == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        setWindowsError("Failed to create remote thread for shellcode execution", error);
        qDebug() << "CreateRemoteThread failed with error:" << error;
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
            qDebug() << "x64 successCount:" << resultData.successCount << "exitCode:" << exitCode;
            success = (exitCode == 0) && (resultData.successCount > 0);
            if (!success) {
                const_cast<WindowHider*>(this)->setLastErrorMessage(
                    QString("No windows were successfully hidden (success count: %1)").arg(resultData.successCount)
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
            qDebug() << "x86 successCount:" << resultDataX86.successCount << "exitCode:" << exitCode;
            qDebug() << "x86 successCount offset:" << offsetof(InjectionData32, successCount);
            success = (exitCode == 0) && (resultDataX86.successCount > 0);
            if (!success) {
                const_cast<WindowHider*>(this)->setLastErrorMessage(
                    QString("No windows were successfully hidden (success count: %1)").arg(resultDataX86.successCount)
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
