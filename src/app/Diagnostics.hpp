// SPDX-License-Identifier: MIT
//
// Startup diagnostics.
//
// schnellerTyp-e is a Windows-subsystem binary with no console and no main
// window, which makes an early crash almost opaque: the process disappears and
// the only evidence is the host telling you it "terminated abnormally". This
// installs a Qt message handler that mirrors every message to a file that is
// flushed on every write, so the last line before a crash survives it.
//
// The log lives next to the settings, and its path is printed at startup.

#pragma once

#include <QString>

namespace st::diagnostics {

/// Install the message handler. Call once, immediately after the
/// QApplication exists and before anything else can log or crash.
void install();

/// Absolute path of the log file. Empty when it could not be opened.
[[nodiscard]] QString logFilePath();

/// Record a startup milestone. These are deliberately unconditional: the whole
/// point is that they are present in a build that crashes on a machine we
/// cannot attach a debugger to.
void milestone(const QString& what);

} // namespace st::diagnostics
