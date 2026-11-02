// SPDX-License-Identifier: MIT
// Scratch editor for opt-in desktop verification. Never opens user documents.
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc != 2)
        return 1;
    app.setApplicationName(QStringLiteral("schnellertype-qt-probe"));
    QWidget window;
    window.setWindowTitle(QStringLiteral("schnellerTyp-e scratch editor"));
    QVBoxLayout layout(&window);
    QLineEdit normal, second, password;
    password.setEchoMode(QLineEdit::Password);
    for (auto *field : {&normal, &second, &password})
        layout.addWidget(field);
    window.resize(600, 220);
    window.show();
    normal.setFocus();
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        QJsonObject state{{"normal", normal.text()}, {"second", second.text()},
                          {"password", password.text()},
                          {"focus", normal.hasFocus() ? "normal" : second.hasFocus() ? "second" : "password"}};
        QFile file(QString::fromLocal8Bit(argv[1]));
        if (file.open(QIODevice::WriteOnly))
            file.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
    });
    timer.start(30);
    return app.exec();
}
