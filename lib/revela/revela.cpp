#include <vector>

// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on

#ifdef _DEBUG
#include <fcntl.h>
#include <io.h>
#include <iostream>
#endif

struct WindowEnumData {
    std::vector<DWORD> processIds;
    DWORD hiddenCount;
};

std::vector<DWORD> GetProcessAndChildren(DWORD parentProcessId) {
    std::vector<DWORD> processIds;
    processIds.push_back(parentProcessId);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
#ifdef _DEBUG
        std::cout << "Failed to create process snapshot" << std::endl;
#endif
        return processIds;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ParentProcessID == parentProcessId) {
                processIds.push_back(pe32.th32ProcessID);
#ifdef _DEBUG
                std::cout << "Found child process: " << pe32.th32ProcessID << " (" << pe32.szExeFile << ")"
                          << std::endl;
#endif
                // Recursively get children of this child process
                std::vector<DWORD> childProcessIds = GetProcessAndChildren(pe32.th32ProcessID);
                for (size_t i = 1; i < childProcessIds.size(); ++i) {
                    processIds.push_back(childProcessIds[i]);
                }
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

#ifdef _DEBUG
    std::cout << "Total processes to unhide windows for: " << processIds.size() << std::endl;
    for (DWORD pid : processIds) {
        std::cout << "  Process ID: " << pid << std::endl;
    }
#endif

    return processIds;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    WindowEnumData* data = reinterpret_cast<WindowEnumData*>(lParam);

    DWORD windowProcessId;
    GetWindowThreadProcessId(hwnd, &windowProcessId);

    // Check if this window belongs to any of our target processes
    for (DWORD pid : data->processIds) {
        if (windowProcessId == pid) {
            // Skip invisible windows
            if (!IsWindowVisible(hwnd)) {
#ifdef _DEBUG
                char windowTitle[256];
                GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
                std::cout << "Skipping invisible window: " << windowTitle << " (PID: " << windowProcessId
                          << ", HWND: " << hwnd << ")" << std::endl;
#endif
                break;
            }

            // Get window title for debug output
#ifdef _DEBUG
            char windowTitle[256];
            GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
            std::cout << "Processing window: " << windowTitle << " (PID: " << windowProcessId << ", HWND: " << hwnd
                      << ")";
#endif

            if (SetWindowDisplayAffinity(hwnd, WDA_NONE)) {
                data->hiddenCount++;
#ifdef _DEBUG
                std::cout << " - UNHIDDEN" << std::endl;
#endif
            } else {
#ifdef _DEBUG
                std::cout << " - FAILED TO UNHIDE (Error: " << GetLastError() << ")" << std::endl;
#endif
            }
            break;
        }
    }

    // Continue enumeration
    return TRUE;
}

DWORD UnhideAllWindows() {
    DWORD currentProcessId = GetCurrentProcessId();

#ifdef _DEBUG
    std::cout << "Current process ID: " << currentProcessId << std::endl;
#endif

    WindowEnumData enumData;
    enumData.processIds = GetProcessAndChildren(currentProcessId);
    enumData.hiddenCount = 0;

    // Enumerate all top-level windows
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&enumData));

#ifdef _DEBUG
    std::cout << "Total windows unhidden: " << enumData.hiddenCount << std::endl;
#endif

    return enumData.hiddenCount;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
#ifdef _DEBUG
            if (AllocConsole()) {
                freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
                freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
            }
            std::cout << "Revela DLL loaded" << std::endl;
#endif

            DisableThreadLibraryCalls(hModule);

            // Unhide all windows of the current process and its children
            // If no windows were unhidden, return FALSE to indicate failure
            DWORD unhiddenCount = UnhideAllWindows();
            if (unhiddenCount == 0) {
#ifdef _DEBUG
                std::cout << "No windows were unhidden" << std::endl;
#endif
                return FALSE;
            }

#ifdef _DEBUG
            std::cout << "Successfully unhidden " << unhiddenCount << " windows" << std::endl;
            std::cout << "Creating detached thread to unload DLL" << std::endl;
#endif

            // Unload the DLL in a detached thread
            HANDLE hThread = CreateThread(
                NULL,
                0,
                [](LPVOID lpParam) -> DWORD {
                    HMODULE hModule = (HMODULE)lpParam;
                    FreeLibraryAndExitThread(hModule, 0);
                },
                hModule,
                0,
                NULL
            );

            if (hThread) {
                CloseHandle(hThread);
            }
        }

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;

        case DLL_PROCESS_DETACH:
#ifdef _DEBUG
            std::cout << "Revela DLL unloading" << std::endl;
            FreeConsole();
#endif
            break;
    }
    return TRUE;
}
