#pragma once

#include <QIcon>
#include <QString>
#include <string>

class RandUtils {
   public:
    // Random title generation
    static QString generateRandomTitle();

    // Random filename generation
    static std::string generateRandomFilename(const std::string& extension = ".dll");

    // Random icon generation
    static QIcon generateRandomIcon();

   private:
    static const QString kCharacters;
    static constexpr int kMinLength = 10;
    static constexpr int kMaxLength = 30;
    static constexpr int kFilenameMinLength = 8;
    static constexpr int kFilenameMaxLength = 16;

    // Abstracted random string generation
    static QString generateRandomString(int minLength, int maxLength, const QString& charset);
    static std::string generateRandomString(int minLength, int maxLength, const std::string& charset);
};
