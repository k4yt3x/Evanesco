#include "titleutils.h"

#include <QRandomGenerator>

const QString TitleUtils::kCharacters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

QString TitleUtils::generateRandomTitle() {
    int length = QRandomGenerator::global()->bounded(kMinLength, kMaxLength + 1);
    QString randomTitle;

    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->bounded(kCharacters.length());
        randomTitle.append(kCharacters.at(index));
    }

    return randomTitle;
}
