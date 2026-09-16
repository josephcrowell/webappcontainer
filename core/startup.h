// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

namespace Startup {

struct WidevineOptions {
  bool enabled = false;
  bool searchFallbacks = true;
  QString configuredLibraryPath;
};

struct WidevineResult {
  QString libraryPath;
  bool alreadyConfigured = false;
  bool environmentChanged = false;
  QString error;
};

QString resolveExecutablePath(const QString &argv0);
QString applicationNameFromArguments(int argc, char *argv[],
                                     const QString &fallback);
QString findWidevineCdm(const QString &argv0, const QString &configuredLibraryPath = {});
WidevineResult configureWidevine(const QString &argv0, const WidevineOptions &options);

} // namespace Startup
