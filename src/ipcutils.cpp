#include "ipcutils.h"

namespace IpcUtils {

// Helper functions for flag operations
void setOperationFlag(uint32_t& flags, bool isUnhide) {
    if (isUnhide) {
        // Set bit (unhide = 1)
        flags |= kOperationMask;
    } else {
        // Clear bit (hide = 0)
        flags &= ~kOperationMask;
    }
}

void setHideTaskbarIconFlag(uint32_t& flags, bool hideTaskbarIcon) {
    if (hideTaskbarIcon) {
        // Set bit
        flags |= kHideTaskbarIconMask;
    } else {
        // Clear bit
        flags &= ~kHideTaskbarIconMask;
    }
}

bool getOperationFlag(uint32_t flags) {
    // true = unhide, false = hide
    return (flags & kOperationMask) != 0;
}

bool getHideTaskbarIconFlag(uint32_t flags) {
    return (flags & kHideTaskbarIconMask) != 0;
}

uint32_t getOperationType(uint32_t flags) {
    // Returns kOperationHide or kOperationUnhide
    return flags & kOperationMask;
}

// Utility function to validate flags
bool isValidFlags(uint32_t flags) {
    // Check if any unexpected bits are set (only bits 0 and 1 should be used)
    constexpr uint32_t kValidMask = kOperationMask | kHideTaskbarIconMask;
    return (flags & ~kValidMask) == 0;
}

// Utility function to create flags from boolean values
uint32_t createFlags(bool isUnhide, bool hideTaskbarIcon) {
    uint32_t flags = 0;
    setOperationFlag(flags, isUnhide);
    setHideTaskbarIconFlag(flags, hideTaskbarIcon);
    return flags;
}

// Debug utility to get a human-readable description of flags
std::string flagsToString(uint32_t flags) {
    std::string result;

    if (getOperationFlag(flags)) {
        result += "UNHIDE";
    } else {
        result += "HIDE";
    }

    if (getHideTaskbarIconFlag(flags)) {
        result += " | HIDE_TASKBAR_ICON";
    } else {
        result += " | SHOW_TASKBAR_ICON";
    }

    return result;
}

}  // namespace IpcUtils
