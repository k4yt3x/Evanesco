#pragma once

#include <cstdint>
#include <string>

namespace IpcUtils {

// Operation parameters structure for shared memory
struct OperationParams {
    // Bit 0: operation (0=hide, 1=unhide), Bit 1: persistent (0=false, 1=true)
    uint32_t flags;
};

// Bit masks for flags
// Bit 0
constexpr uint32_t kOperationMask = 0x1;
// Bit 1
constexpr uint32_t kPersistentMask = 0x2;

// Operation types
constexpr uint32_t kOperationHide = 0;
constexpr uint32_t kOperationUnhide = 1;

// Helper functions for flag operations
void setOperationFlag(uint32_t& flags, bool isUnhide);
void setPersistentFlag(uint32_t& flags, bool persistent);
bool getOperationFlag(uint32_t flags);
bool getPersistentFlag(uint32_t flags);
uint32_t getOperationType(uint32_t flags);

// Utility functions
bool isValidFlags(uint32_t flags);
uint32_t createFlags(bool isUnhide, bool persistent);
std::string flagsToString(uint32_t flags);

}  // namespace IpcUtils
