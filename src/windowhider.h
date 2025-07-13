#pragma once

#include <vector>

#include <windows.h>

#include <QString>

// Structure for passing data to injected x64 code
struct InjectionData64 {
    HWND hwnd;
    DWORD affinity;
    FARPROC setWindowDisplayAffinityAddr;
    FARPROC getLastErrorAddr;
    DWORD result;
    DWORD lastError;
};

// Structure for passing data to injected x86 code (packed for 32-bit layout)
#pragma pack(push, 1)
struct InjectionData32 {
    DWORD hwnd;
    DWORD affinity;
    DWORD setWindowDisplayAffinityAddr;
    DWORD getLastErrorAddr;
    DWORD result;
    DWORD lastError;
};
#pragma pack(pop)

// Helper structure for window enumeration
struct EnumWindowsData {
    DWORD processId;
    HWND mainWindow;
};

// Helper structure for function addresses
struct FunctionAddresses {
    FARPROC setWindowDisplayAffinity;
    FARPROC getLastError;
};

class WindowHider {
   public:
    // Construction and destruction
    explicit WindowHider(DWORD processId);
    explicit WindowHider(HWND windowHandle);
    ~WindowHider();

    // Non-copyable and non-movable
    WindowHider(const WindowHider&) = delete;
    WindowHider& operator=(const WindowHider&) = delete;
    WindowHider(WindowHider&&) = delete;
    WindowHider& operator=(WindowHider&&) = delete;

    // Main interface
    bool Initialize();
    bool HideWindow(bool hide = true);
    void Cleanup();

    // Accessors
    HWND GetWindowHandle() const { return m_targetHwnd; }
    DWORD GetProcessId() const { return m_processId; }
    QString GetLastErrorMessage() const { return m_lastErrorMessage; }

   private:
    // Member variables
    DWORD m_processId;
    HWND m_targetHwnd;
    HANDLE m_hProcess;
    QString m_lastErrorMessage;

    // Initialization helper methods
    bool resolveWindowAndProcess();
    bool validateTargetProcess();
    bool openTargetProcess();
    bool validateInitialization() const;

    // Function address resolution
    bool resolveFunctionAddresses(FunctionAddresses& addresses) const;
    bool resolveAddresses32(InjectionData64& injData) const;

    // Injection data preparation
    InjectionData64 createInjectionData(bool hide, const FunctionAddresses& addresses) const;
    InjectionData32 convertToInjectionData32(const InjectionData64& injData) const;

    // Memory and injection management
    bool performInjection(const InjectionData64& injData, bool is64Bit);
    std::vector<unsigned char> createEmbeddedShellcode(const InjectionData64& injData, bool is64Bit) const;

    // Shellcode preparation and patching
    std::vector<unsigned char> createEmbeddedShellcode64(const InjectionData64& injData) const;
    std::vector<unsigned char> createEmbeddedShellcode32(const InjectionData64& injData) const;
    void patchDataOffset(std::vector<unsigned char>& code, size_t offset, bool is64Bit) const;

    // Memory operations
    bool writeShellcodeToTarget(LPVOID remoteMemory, const std::vector<unsigned char>& shellcode) const;

    // Shellcode execution and result validation
    bool executeShellcode(LPVOID remoteMemory, bool is64Bit);
    bool readAndValidateResults(LPVOID remoteMemory, bool is64Bit, DWORD exitCode, size_t dataOffset) const;

    // Utility methods
    HWND findMainWindowForProcess(DWORD processId) const;
    void verifyShellcodeLayout() const;
    void logLastError(const char* operation, DWORD errorCode = 0) const;
    void setLastErrorMessage(const QString& message);
    void setWindowsError(const QString& context, DWORD errorCode = 0);
    QString getWindowsErrorString(DWORD errorCode) const;
    QString formatErrorMessage(DWORD errorCode) const;

    bool revalidateTargets();

    // Window enumeration
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

    // Shellcode definitions - templates for creating mutable copies
    // clang-format off
    static inline const unsigned char kShellcodeX64[] = {
        // Reserve shadow space for function call
        0x48, 0x83, 0xEC, 0x20,       // sub rsp, 32

        // Calculate data address (skip past code to embedded data)
        0x48, 0x8D, 0x35, 0x2F, 0x00, 0x00, 0x00, // lea rsi, [rip+0x2F] (data offset will be patched)

        // Load parameters from embedded data
        0x48, 0x8B, 0x0E,             // mov rcx, [rsi] (hwnd - offset 0)
        0x8B, 0x56, 0x08,             // mov edx, [rsi+8] (affinity - offset 8)
        0x48, 0x8B, 0x46, 0x10,       // mov rax, [rsi+16] (SetWindowDisplayAffinity address - offset 16)

        // Call SetWindowDisplayAffinity(hwnd, affinity)
        0xFF, 0xD0,                   // call rax

        // Store result in embedded data
        0x89, 0x46, 0x20,             // mov [rsi+32], eax (result - offset 32)

        // Call GetLastError and store result
        0x48, 0x8B, 0x46, 0x18,       // mov rax, [rsi+24] (GetLastError address - offset 24)
        0xFF, 0xD0,                   // call rax
        0x89, 0x46, 0x24,             // mov [rsi+36], eax (lastError - offset 36)

        // Clean up and exit
        0x48, 0x83, 0xC4, 0x20,       // add rsp, 32
        0xC3,                         // ret

        // Padding to align data section
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90
    };
    // clang-format on

    static inline const size_t kShellcodeX64Size = sizeof(kShellcodeX64);

    // clang-format off
    static inline const unsigned char kShellcodeX86[] = {
        // Calculate data address (skip past code to embedded data)
        0xE8, 0x00, 0x00, 0x00, 0x00, // call $+5 (get current EIP)
        0x58,                         // pop eax (now eax = current address)
        0x83, 0xC0, 0x1E,             // add eax, 0x1E (data offset will be patched)
        0x89, 0xC6,                   // mov esi, eax (esi points to data)

        // Push parameters for SetWindowDisplayAffinity call
        0xFF, 0x76, 0x04,             // push [esi+4] (affinity - offset 4)
        0xFF, 0x36,                   // push [esi] (hwnd - offset 0)

        // Call SetWindowDisplayAffinity
        0xFF, 0x56, 0x08,             // call [esi+8] (SetWindowDisplayAffinity address - offset 8)

        // Store result in embedded data
        0x89, 0x46, 0x10,             // mov [esi+16], eax (result - offset 16)

        // Call GetLastError
        0xFF, 0x56, 0x0C,             // call [esi+12] (GetLastError address - offset 12)
        0x89, 0x46, 0x14,             // mov [esi+20], eax (lastError - offset 20)

        // Exit
        0xC3,                         // ret

        // Padding to align data section
        0x90, 0x90, 0x90, 0x90, 0x90
    };
    // clang-format on

    static inline const size_t kShellcodeX86Size = sizeof(kShellcodeX86);
};
