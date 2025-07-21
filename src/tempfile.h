#pragma once

#include <string>

// RAII class for temporary file management
class TempFile {
   public:
    // Create a temporary file by copying from source
    explicit TempFile(const std::string& sourcePath, const std::string& extension = ".dll");

    // Non-copyable but movable
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    TempFile(TempFile&& other) noexcept;
    TempFile& operator=(TempFile&& other) noexcept;

    // Destructor automatically cleans up the temporary file
    ~TempFile();

    // Get the path to the temporary file
    const std::string& path() const { return tempPath_; }

    // Check if the temporary file was created successfully
    bool isValid() const { return isValid_; }

    // Get error message if creation failed
    const std::string& errorMessage() const { return errorMessage_; }

    // Manually release the temporary file (prevents automatic cleanup)
    void release();

   private:
    std::string tempPath_;
    bool isValid_;
    bool released_;
    std::string errorMessage_;

    // Helper methods
    static std::string getTempDirectory();
    static std::string createTempFilePath(const std::string& extension);
    static bool copyFile(const std::string& sourcePath, const std::string& destPath);
    static bool deleteFile(const std::string& filePath);
};
