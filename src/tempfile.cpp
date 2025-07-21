#include "tempfile.h"

#include <windows.h>

#include "randutils.h"

// TempFile implementation

TempFile::TempFile(const std::string& sourcePath, const std::string& extension) : isValid_(false), released_(false) {
    tempPath_ = createTempFilePath(extension);
    if (tempPath_.empty()) {
        errorMessage_ = "Failed to create temporary file path";
        return;
    }

    if (!copyFile(sourcePath, tempPath_)) {
        errorMessage_ = "Failed to copy file to temporary location";
        tempPath_.clear();
        return;
    }

    isValid_ = true;
}

TempFile::TempFile(TempFile&& other) noexcept
    : tempPath_(std::move(other.tempPath_)),
      isValid_(other.isValid_),
      released_(other.released_),
      errorMessage_(std::move(other.errorMessage_)) {
    // Mark the moved-from object as released to prevent cleanup
    other.released_ = true;
}

TempFile& TempFile::operator=(TempFile&& other) noexcept {
    if (this != &other) {
        // Clean up current file if needed
        if (isValid_ && !released_ && !tempPath_.empty()) {
            deleteFile(tempPath_);
        }

        // Move data from other
        tempPath_ = std::move(other.tempPath_);
        isValid_ = other.isValid_;
        released_ = other.released_;
        errorMessage_ = std::move(other.errorMessage_);

        // Mark the moved-from object as released
        other.released_ = true;
    }
    return *this;
}

TempFile::~TempFile() {
    if (isValid_ && !released_ && !tempPath_.empty()) {
        deleteFile(tempPath_);
    }
}

void TempFile::release() {
    released_ = true;
}

std::string TempFile::getTempDirectory() {
    char tempPath[MAX_PATH];
    DWORD result = GetTempPathA(MAX_PATH, tempPath);
    if (result == 0 || result > MAX_PATH) {
        return "";
    }
    return std::string(tempPath);
}

std::string TempFile::createTempFilePath(const std::string& extension) {
    std::string tempDir = getTempDirectory();
    if (tempDir.empty()) {
        return "";
    }

    std::string filename = RandUtils::generateRandomFilename(extension);
    return tempDir + filename;
}

bool TempFile::copyFile(const std::string& sourcePath, const std::string& destPath) {
    return CopyFileA(sourcePath.c_str(), destPath.c_str(), FALSE) != 0;
}

bool TempFile::deleteFile(const std::string& filePath) {
    return DeleteFileA(filePath.c_str()) != 0;
}
