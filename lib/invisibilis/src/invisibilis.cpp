#include <cstdint>
#include <string>
#include <vector>

#ifdef _DEBUG
#include <fcntl.h>
#include <io.h>
#include <iostream>
#endif

// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on

#include "hashutils.h"
#include "ipcutils.h"

struct WindowEnumData {
    std::vector<DWORD> processIds;
    DWORD hiddenCount;
    DWORD operation;
    bool persistent;
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
    std::cout << "Total processes to process windows for: " << processIds.size() << std::endl;
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

            // Determine operation based on flags
            DWORD affinity = (data->operation == IpcUtils::kOperationHide) ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;

            if (SetWindowDisplayAffinity(hwnd, affinity)) {
                data->hiddenCount++;
#ifdef _DEBUG
                std::cout << " - " << (data->operation == IpcUtils::kOperationHide ? "HIDDEN" : "UNHIDDEN")
                          << std::endl;
#endif

                // Handle persistent flag (placeholder for now)
                if (data->persistent) {
                    // TODO: Implement persistent window hiding/unhiding
#ifdef _DEBUG
                    std::cout << "  (Persistent mode - not yet implemented)" << std::endl;
#endif
                }
            } else {
#ifdef _DEBUG
                std::cout << " - FAILED (Error: " << GetLastError() << ")" << std::endl;
#endif
            }
            break;
        }
    }

    // Continue enumeration
    return TRUE;
}

DWORD PerformWindowOperation(uint32_t operation, bool persistent) {
    DWORD currentProcessId = GetCurrentProcessId();

#ifdef _DEBUG
    std::cout << "Operation: " << (operation == IpcUtils::kOperationHide ? "HIDE" : "UNHIDE") << std::endl;
    std::cout << "Persistent: " << (persistent ? "YES" : "NO") << std::endl;
#endif

    WindowEnumData enumData;
    enumData.processIds = GetProcessAndChildren(currentProcessId);
    enumData.hiddenCount = 0;
    enumData.operation = operation;
    enumData.persistent = persistent;

    // Enumerate all top-level windows
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&enumData));

#ifdef _DEBUG
    std::cout << "Total windows processed: " << enumData.hiddenCount << std::endl;
#endif

    return enumData.hiddenCount;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
#ifdef _DEBUG
            if (AllocConsole()) {
                freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
                freopen_s(reinterpret_cast<FILE**>(stderr), "CONOUT$", "w", stderr);
            }
            std::cout << "Invisibilis DLL loaded" << std::endl;
#endif

            DisableThreadLibraryCalls(hModule);

            DWORD currentPid = GetCurrentProcessId();
            std::string mappingName = HashUtils::generateMappingName(currentPid);

#ifdef _DEBUG
            std::cout << "Looking for shared memory: " << mappingName << std::endl;
#endif

            HANDLE hMapFile = OpenFileMappingA(FILE_MAP_READ, FALSE, mappingName.c_str());
            if (!hMapFile) {
#ifdef _DEBUG
                std::cout << "Failed to open shared memory mapping (Error: " << GetLastError() << ")" << std::endl;
#endif
                return FALSE;
            }

            IpcUtils::OperationParams* params = static_cast<IpcUtils::OperationParams*>(
                MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, sizeof(IpcUtils::OperationParams))
            );

            DWORD hiddenCount = 0;
            if (params) {
                // Extract operation and persistent flag using bitwise operations
                uint32_t operation = IpcUtils::getOperationType(params->flags);
                bool persistent = IpcUtils::getPersistentFlag(params->flags);

                hiddenCount = PerformWindowOperation(operation, persistent);

                UnmapViewOfFile(params);
            } else {
#ifdef _DEBUG
                std::cout << "Failed to map view of file (Error: " << GetLastError() << ")" << std::endl;
#endif
            }

            CloseHandle(hMapFile);

#ifdef _DEBUG
            std::cout << "Successfully processed " << hiddenCount << " windows" << std::endl;
            std::cout << "Creating detached thread to unload DLL" << std::endl;
#endif

            // Self-unload the DLL in a detached thread
            HANDLE hThread = CreateThread(
                nullptr,
                0,
                [](LPVOID lpParam) -> DWORD {
                    HMODULE hModule = static_cast<HMODULE>(lpParam);
                    FreeLibraryAndExitThread(hModule, 0);
                },
                hModule,
                0,
                nullptr
            );

            if (hThread) {
                CloseHandle(hThread);
            }

            return hiddenCount > 0;
        }

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;

        case DLL_PROCESS_DETACH:
#ifdef _DEBUG
            std::cout << "Invisibilis DLL unloading" << std::endl;
            FreeConsole();
#endif
            break;
    }
    return TRUE;
}
