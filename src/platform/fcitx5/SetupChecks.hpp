// SPDX-License-Identifier: MIT
#pragma once
#include <QJsonObject>
#include <QString>
#include <QList>

namespace st::fcitx5 {
struct SetupCheck {
    QString title;
    QString state;
    QString detail;
};
struct SetupReport {
    QString status;
    QList<SetupCheck> checks;
    QJsonObject toJson() const;
};
SetupReport collectSetupReport();
// Only the caller-supplied config directory is changed; existing files are backed up.
bool repairSetupFiles(const QString& configDirectory, QString& message);
} // namespace st::fcitx5
