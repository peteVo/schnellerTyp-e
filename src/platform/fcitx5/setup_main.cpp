// SPDX-License-Identifier: MIT
#include "SetupChecks.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <cstdio>
#include <memory>

namespace {
bool saveReport(const QString& path, const st::fcitx5::SetupReport& report) {
    QSaveFile file(path);
    const auto bytes = QJsonDocument(report.toJson()).toJson(QJsonDocument::Indented);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
}

int main(int argc, char** argv)
{
    bool console = false;
    for (int i = 1; i < argc; ++i)
        console |= QString::fromLocal8Bit(argv[i]) == QStringLiteral("--json") || QString::fromLocal8Bit(argv[i]) == QStringLiteral("--export");
    std::unique_ptr<QCoreApplication> app;
    if (console) app = std::make_unique<QCoreApplication>(argc, argv);
    else app = std::make_unique<QApplication>(argc, argv);
    app->setApplicationName(QStringLiteral("schnellertype-setup"));
    app->setApplicationVersion(QStringLiteral(SCHNELLERTYPE_VERSION));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("schnellerTyp-e status, setup checks and privacy-preserving diagnostics"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("json"), QStringLiteral("Print a diagnostic report; includes no typing or personal words.")});
    parser.addOption({QStringLiteral("export"), QStringLiteral("Save a diagnostic report."), QStringLiteral("file")});
    parser.addOption({QStringLiteral("snapshot"), QStringLiteral("Save a setup-window preview and exit."), QStringLiteral("file")});
    parser.addOption({QStringLiteral("compact"), QStringLiteral("Use the minimum window size for preview.")});
    parser.process(*app);
    auto report = st::fcitx5::collectSetupReport();
    if (console) {
        if (parser.isSet(QStringLiteral("export")) && !saveReport(parser.value(QStringLiteral("export")), report)) {
            std::fputs("Could not save diagnostic report.\n", stderr);
            return 1;
        }
        if (parser.isSet(QStringLiteral("json")))
            std::fputs(QJsonDocument(report.toJson()).toJson(QJsonDocument::Indented).constData(), stdout);
        return 0;
    }
    QWidget window;
    window.setWindowTitle(QStringLiteral("schnellerTyp-e · Status & Setup"));
    window.setMinimumSize(640, 540);
    window.resize(parser.isSet(QStringLiteral("compact")) ? QSize(640, 540) : QSize(900, 700));
    QVBoxLayout layout(&window);
    layout.setContentsMargins(24, 24, 24, 24);
    layout.setSpacing(16);
    QLabel title(QStringLiteral("Status & Setup"));
    auto heading = title.font(); heading.setPointSize(heading.pointSize() + 6); heading.setBold(true); title.setFont(heading);
    layout.addWidget(&title);
    QLabel status;
    status.setWordWrap(true);
    status.setAccessibleName(QStringLiteral("Typing assistance status"));
    layout.addWidget(&status);
    QLabel context(QStringLiteral("This is a snapshot of the last focused field. For the current application's mode, check the Fcitx status menu. Use Check again after changing input sources."));
    context.setWordWrap(true);
    layout.addWidget(&context);
    QTreeWidget checks;
    checks.setHeaderLabels({QStringLiteral("Check"), QStringLiteral("Result")});
    checks.setRootIsDecorated(false);
    checks.setAccessibleName(QStringLiteral("Setup checks; select a row to see its recovery instructions"));
    checks.header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    checks.header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout.addWidget(&checks, 1);
    QLabel detail;
    detail.setWordWrap(true);
    detail.setMinimumHeight(56);
    detail.setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout.addWidget(&detail);
    const auto render = [&] {
        status.setText(report.status);
        checks.clear();
        for (const auto& check : report.checks) {
            auto* item = new QTreeWidgetItem(&checks, {check.title, check.state});
            item->setData(0, Qt::UserRole, check.detail);
        }
        checks.setCurrentItem(checks.topLevelItem(0));
    };
    QObject::connect(&checks, &QTreeWidget::currentItemChanged, [&](QTreeWidgetItem* item, QTreeWidgetItem*) {
        detail.setText(item ? item->data(0, Qt::UserRole).toString() : QString());
    });
    QHBoxLayout actions;
    QPushButton refresh(QStringLiteral("Check again")), settings(QStringLiteral("Fcitx settings")), repair(QStringLiteral("Repair setup files"));
    actions.addWidget(&refresh); actions.addWidget(&settings); actions.addWidget(&repair); actions.addStretch();
    layout.addLayout(&actions);
    QPushButton exportButton(QStringLiteral("Export diagnostic report…"));
    exportButton.setAccessibleDescription(QStringLiteral("Exports versions and check results. No typed text, app IDs, personal words, paths or logs."));
    layout.addWidget(&exportButton, 0, Qt::AlignLeft);
    QLabel privacy(QStringLiteral("Reports contain versions and check results only. Typed text, personal words, app IDs, paths and logs are excluded."));
    privacy.setWordWrap(true);
    layout.addWidget(&privacy);
    QObject::connect(&refresh, &QPushButton::clicked, [&] { report = st::fcitx5::collectSetupReport(); render(); });
    QObject::connect(&settings, &QPushButton::clicked, [&] {
        if (!QProcess::startDetached(QStringLiteral("fcitx5-configtool"), {}))
            detail.setText(QStringLiteral("Could not open Fcitx settings. Install the Fcitx configuration tool."));
    });
    QObject::connect(&repair, &QPushButton::clicked, [&] {
        QString message;
        st::fcitx5::repairSetupFiles(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation), message);
        report = st::fcitx5::collectSetupReport(); render(); detail.setText(message);
    });
    QObject::connect(&exportButton, &QPushButton::clicked, [&] {
        const auto path = QFileDialog::getSaveFileName(&window, QStringLiteral("Export diagnostic report"), QStringLiteral("schnellertype-diagnostics.json"), QStringLiteral("JSON (*.json)"));
        if (!path.isEmpty()) detail.setText(saveReport(path, report) ? QStringLiteral("Diagnostic report saved.") : QStringLiteral("Could not save report. Choose a writable location."));
    });
    render();
    window.show();
    if (parser.isSet(QStringLiteral("snapshot"))) {
        QTimer::singleShot(300, [&] {
            const bool saved = window.grab().save(parser.value(QStringLiteral("snapshot")));
            app->exit(saved ? 0 : 1);
        });
    }
    return app->exec();
}
