#include "procutils.h"

#include <imagehlp.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <winnt.h>

namespace ProcUtils {

QString getProcessArchitecture(DWORD pid) {
    HANDLE hProcess = openProcess(pid);
    if (hProcess == nullptr) {
        return "Unknown";
    }

    bool result = is64BitProcess(hProcess);
    CloseHandle(hProcess);

    return result ? "x64" : "x86";
}

QString getProcessExecutablePath(DWORD pid) {
    HANDLE hProcess = openProcess(pid);
    if (hProcess == nullptr) {
        return QString();
    }

    wchar_t path[MAX_PATH];
    DWORD pathSize = MAX_PATH;

    if (QueryFullProcessImageNameW(hProcess, 0, path, &pathSize)) {
        CloseHandle(hProcess);
        return QString::fromWCharArray(path);
    }

    CloseHandle(hProcess);
    return QString();
}

HWND getMainWindowHandle(DWORD pid) {
    struct EnumData {
        DWORD targetPid;
        HWND mainWindow;
    };

    EnumData data = {pid, nullptr};

    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            EnumData* pData = reinterpret_cast<EnumData*>(lParam);

            DWORD windowPid;
            GetWindowThreadProcessId(hwnd, &windowPid);

            if (windowPid != pData->targetPid) {
                // Continue enumeration
                return TRUE;
            }

            // Check if window is visible and has a title
            if (!IsWindowVisible(hwnd)) {
                // Continue enumeration
                return TRUE;
            }

            wchar_t title[256];
            if (GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t)) <= 0) {
                // Continue enumeration
                return TRUE;
            }

            // Check if it's a main window (has no owner and is not a tool window)
            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
            LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

            if ((style & WS_VISIBLE) && !(exStyle & WS_EX_TOOLWINDOW) && GetWindow(hwnd, GW_OWNER) == nullptr &&
                !(style & WS_CHILD)) {
                pData->mainWindow = hwnd;
                // Stop enumeration
                return FALSE;
            }

            // Continue enumeration
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&data)
    );

    return data.mainWindow;
}

bool isProcessVisible(DWORD pid) {
    return getMainWindowHandle(pid) != nullptr;
}

bool is64BitProcess(DWORD pid) {
    HANDLE hProcess = openProcess(pid);
    if (hProcess == nullptr) {
        // Assume 64-bit if we can't determine
        return true;
    }

    bool result = is64BitProcess(hProcess);
    CloseHandle(hProcess);

    return result;
}

bool is64BitProcess(HANDLE hProcess) {
    BOOL isWow64 = FALSE;

    if (!IsWow64Process(hProcess, &isWow64)) {
        // Assume 64-bit if we can't determine
        return true;
    }

    return !isWow64;
}

HANDLE openProcess(DWORD pid, DWORD desiredAccess) {
    HANDLE hProcess = OpenProcess(desiredAccess, FALSE, pid);
    if (hProcess == nullptr) {
        // Try with reduced permissions
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    }
    return hProcess;
}

bool getExportRva32(LPCWSTR path32, LPCSTR functionName, DWORD* pRva) {
    *pRva = 0;
    // Map the file read-only, with SEC_IMAGE so sections are laid out exactly as at runtime
    HANDLE hFile = CreateFileW(path32, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    if (!hMap) {
        CloseHandle(hFile);
        return false;
    }

    BYTE* base = static_cast<BYTE*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
    if (!base) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    // Locate PE headers
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    // Export directory
    DWORD dirRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!dirRva) {
        UnmapViewOfFile(base);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    auto expDir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(base + dirRva);
    auto names = reinterpret_cast<DWORD*>(base + expDir->AddressOfNames);
    auto ordinals = reinterpret_cast<WORD*>(base + expDir->AddressOfNameOrdinals);
    auto funcs = reinterpret_cast<DWORD*>(base + expDir->AddressOfFunctions);

    for (DWORD i = 0; i < expDir->NumberOfNames; ++i) {
        LPCSTR name = reinterpret_cast<LPCSTR>(base + names[i]);
        if (lstrcmpiA(name, functionName) == 0) {
            DWORD rva = funcs[ordinals[i]];
            *pRva = rva;

            // detect forwarded export (points back into export dir)
            if (rva >= dirRva && rva < dirRva + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size) {
                // Forwarder string, e.g. "KERNELBASE.GetLastError"
                // Parse dll+func, recurse:
                LPCSTR fwd = reinterpret_cast<LPCSTR>(base + rva);
                char dll[64], sym[64];
                if (sscanf_s(fwd, "%63[^.].%63s", dll, (unsigned)_countof(dll), sym, (unsigned)_countof(sym)) == 2) {
                    strcat_s(dll, ".dll");
                    wchar_t path[MAX_PATH];
                    GetSystemWow64DirectoryW(path, MAX_PATH);
                    wcscat_s(path, L"\\");
                    MultiByteToWideChar(
                        CP_ACP, 0, dll, -1, path + lstrlenW(path), static_cast<int>(MAX_PATH - lstrlenW(path))
                    );

                    // Recursively get the RVA of the forwarded function
                    getExportRva32(path, sym, &rva);
                    *pRva = rva;
                }
            }

            UnmapViewOfFile(base);
            CloseHandle(hMap);
            CloseHandle(hFile);
            return true;
        }
    }

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return false;
}

bool getRemoteAddress32(DWORD pid, LPCSTR functionName, LPCSTR moduleName, DWORD* pAddr32) {
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) {
        return false;
    }

    // Locate 32-bit module base in the target
    HMODULE mods[512];
    DWORD cb = 0;
    if (!EnumProcessModulesEx(hProc, mods, sizeof(mods), &cb, LIST_MODULES_32BIT)) {
        CloseHandle(hProc);
        return false;
    }

    DWORD_PTR moduleRemoteBase = 0;
    for (DWORD i = 0; i < cb / sizeof(HMODULE); ++i) {
        char name[MAX_PATH];
        if (GetModuleBaseNameA(hProc, mods[i], name, MAX_PATH) && !_stricmp(name, moduleName)) {
            moduleRemoteBase = reinterpret_cast<DWORD_PTR>(mods[i]);
            break;
        }
    }
    if (!moduleRemoteBase) {
        CloseHandle(hProc);
        return false;
    }

    // Find RVA of the function in a 32-bit image file
    wchar_t path[MAX_PATH];
    GetSystemWow64DirectoryW(path, MAX_PATH);
    wcscat_s(path, L"\\");
    size_t pathLen = lstrlenW(path);
    MultiByteToWideChar(CP_ACP, 0, moduleName, -1, path + pathLen, static_cast<int>(MAX_PATH - pathLen));

    DWORD rva;
    if (!getExportRva32(path, functionName, &rva)) {
        CloseHandle(hProc);
        return false;
    }

    // RVA + remote base => absolute 32-bit address
    *pAddr32 = static_cast<DWORD>(moduleRemoteBase + rva);
    CloseHandle(hProc);
    return true;
}

bool isCurrentProcessAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = nullptr;

    if (AllocateAndInitializeSid(
            &ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup
        )) {
        if (!CheckTokenMembership(nullptr, adminGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
}

}  // namespace ProcUtils
