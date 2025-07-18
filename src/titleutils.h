#pragma once

#include <QString>

class TitleUtils {
   public:
    static QString generateRandomTitle();

   private:
    static const QString kCharacters;
    static constexpr int kMinLength = 10;
    static constexpr int kMaxLength = 30;
};
