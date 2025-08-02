#include "randutils.h"

#include <QBrush>
#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QPolygon>
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

QIcon RandUtils::generateRandomIcon() {
    // Create a 16x16 pixmap for the tray icon
    QPixmap pixmap(16, 16);

    // Generate random colors
    QColor backgroundColor(
        QRandomGenerator::global()->bounded(256),
        QRandomGenerator::global()->bounded(256),
        QRandomGenerator::global()->bounded(256)
    );

    QColor foregroundColor(
        QRandomGenerator::global()->bounded(256),
        QRandomGenerator::global()->bounded(256),
        QRandomGenerator::global()->bounded(256)
    );

    // Fill background
    pixmap.fill(backgroundColor);

    // Create painter to draw on the pixmap
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(foregroundColor, 1));
    painter.setBrush(QBrush(foregroundColor));

    // Generate random simple shapes
    int shapeType = QRandomGenerator::global()->bounded(4);
    switch (shapeType) {
        case 0:  // Circle
            painter.drawEllipse(2, 2, 12, 12);
            break;
        case 1:  // Rectangle
            painter.drawRect(2, 2, 12, 12);
            break;
        case 2:  // Triangle
        {
            QPolygon triangle;
            triangle << QPoint(8, 2) << QPoint(2, 14) << QPoint(14, 14);
            painter.drawPolygon(triangle);
        } break;
        case 3:  // Diamond
        {
            QPolygon diamond;
            diamond << QPoint(8, 2) << QPoint(14, 8) << QPoint(8, 14) << QPoint(2, 8);
            painter.drawPolygon(diamond);
        } break;
    }

    painter.end();

    return QIcon(pixmap);
}
