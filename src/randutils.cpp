#include "randutils.h"

#include <QRandomGenerator>

const QString RandUtils::kCharacters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

QString RandUtils::generateRandomString(int minLength, int maxLength, const QString& charset) {
    int length = QRandomGenerator::global()->bounded(minLength, maxLength + 1);
    QString randomString;

    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->bounded(charset.length());
        randomString += charset[index];
    }

    return randomString;
}

std::string RandUtils::generateRandomString(int minLength, int maxLength, const std::string& charset) {
    int length = QRandomGenerator::global()->bounded(minLength, maxLength + 1);
    std::string randomString;

    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->bounded(static_cast<int>(charset.length()));
        randomString += charset[index];
    }

    return randomString;
}

QString RandUtils::generateRandomTitle() {
    return generateRandomString(kMinLength, kMaxLength, kCharacters);
}

std::string RandUtils::generateRandomFilename(const std::string& extension) {
    std::string charset = kCharacters.toStdString();
    std::string randomFilename = generateRandomString(kFilenameMinLength, kFilenameMaxLength, charset);
    return randomFilename + extension;
}
