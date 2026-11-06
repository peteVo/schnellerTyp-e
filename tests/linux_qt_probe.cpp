// SPDX-License-Identifier: MIT
// Scratch editor for opt-in desktop verification. Never opens user documents.
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QInputMethodEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class Field final : public QLineEdit {
public:
    QString preedit;
protected:
    void inputMethodEvent(QInputMethodEvent* event) override {
        preedit = event->preeditString();
        QLineEdit::inputMethodEvent(event);
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc != 2 && argc != 3)
        return 1;
    app.setApplicationName(QStringLiteral("schnellertype-qt-probe"));
    QWidget window;
    window.setWindowTitle(QStringLiteral("schnellerTyp-e scratch editor"));
    QVBoxLayout layout(&window);
    Field normal, second, password;
    password.setEchoMode(QLineEdit::Password);
    for (auto *field : {&normal, &second, &password})
        layout.addWidget(field);
    if (argc == 3 && QString::fromLocal8Bit(argv[2]) == QStringLiteral("--manual")) {
        window.setWindowTitle(QStringLiteral("Physical typing test — type each example, then Tab"));
        normal.setPlaceholderText(QStringLiteral("Type: tooi ddeens Berlin und moechte coffee [Space, Tab]"));
        second.setPlaceholderText(QStringLiteral("Type: hoom [Space, Tab]"));
        password.setPlaceholderText(QStringLiteral("Synthetic password only: hoom nuawx"));
    }
    window.resize(760, 220);
    window.show();
    normal.setFocus();
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        QJsonObject state{{"normal", normal.text()}, {"second", second.text()},
                          {"normalPreedit", normal.preedit},
                          {"password", password.text()},
                          {"focus", normal.hasFocus() ? "normal" : second.hasFocus() ? "second" : "password"}};
        const auto position = normal.mapTo(&window, QPoint(0, 0));
        state.insert(QStringLiteral("normalRect"), QJsonObject{{"x", position.x()}, {"y", position.y()},
                     {"width", normal.width()}, {"height", normal.height()}});
        state.insert(QStringLiteral("cursor"), normal.cursorPosition());
        state.insert(QStringLiteral("undoAvailable"), normal.isUndoAvailable());
        state.insert(QStringLiteral("redoAvailable"), normal.isRedoAvailable());
        QFile file(QString::fromLocal8Bit(argv[1]));
        if (file.open(QIODevice::WriteOnly))
            file.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
    });
    timer.start(30);
    return app.exec();
}
