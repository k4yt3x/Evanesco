#include "hashutils.h"

#include <cstdint>
#include <string>

uint32_t HashUtils::fnv1a_hash(const uint8_t* data, size_t length) {
    uint32_t hash = kFnvOffsetBasis;
    for (size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= kFnvPrime;
    }
    return hash;
}

uint32_t HashUtils::fnv1a_hash(const std::string& str) {
    return fnv1a_hash(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
}

std::string HashUtils::generateMappingName(uint32_t processId) {
    std::string input = "evanesco" + std::to_string(processId);
    uint32_t hash = fnv1a_hash(input);

    char buffer[32];
    sprintf_s(buffer, "%08X", hash);
    return std::string(buffer);
}
