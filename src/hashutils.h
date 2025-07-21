#pragma once

#include <cstdint>
#include <string>

class HashUtils {
   public:
    // FNV-1a hash implementation for 32-bit values
    static uint32_t fnv1a_hash(const uint8_t* data, size_t length);

    // Convenience function for hashing strings
    static uint32_t fnv1a_hash(const std::string& str);

    // Generate mapping name using FNV-1a hash of "evanesco" + PID
    static std::string generateMappingName(uint32_t processId);

   private:
    // FNV-1a hash constants
    static constexpr uint32_t kFnvOffsetBasis = 2166136261u;
    static constexpr uint32_t kFnvPrime = 16777619u;
};
