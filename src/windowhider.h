#pragma once

#include <vector>

#include <windows.h>

#include <QString>

// Structure for passing data to injected x64 code
struct InjectionData64 {
    HWND hwnd;
    DWORD affinity;
    FARPROC setWindowDisplayAffinityAddr;
    FARPROC findWindowExAddr;
    DWORD successCount;
};

// Structure for passing data to injected x86 code (packed for 32-bit layout)
#pragma pack(push, 1)
struct InjectionData32 {
    DWORD hwnd;
    DWORD affinity;
    DWORD setWindowDisplayAffinityAddr;
    DWORD findWindowExAddr;
    DWORD successCount;
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
    FARPROC findWindowEx;
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
    void logLastError(const char* operation, DWORD errorCode = 0) const;
    void setLastErrorMessage(const QString& message);
    void setWindowsError(const QString& context, DWORD errorCode = 0);
    QString getWindowsErrorString(DWORD errorCode) const;
    QString formatErrorMessage(DWORD errorCode) const;

    bool revalidateTargets();

    // Window enumeration
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
};
