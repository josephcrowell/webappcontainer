// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "startup.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

QString canonicalFile(const QString &path) {
  const QFileInfo info(path);
  if (!info.exists() || !info.isFile() || !info.isReadable())
    return {};
  const QString canonical = info.canonicalFilePath();
  return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

bool containsWidevinePath(const QStringList &arguments) {
  for (qsizetype index = 0; index < arguments.size(); ++index) {
    const QString &argument = arguments.at(index);
    if (argument == QStringLiteral("--widevine-path") ||
        argument.startsWith(QStringLiteral("--widevine-path="))) {
      return true;
    }
  }
  return false;
}

QString joinCommandArguments(const QStringList &arguments) {
  QStringList encoded;
  encoded.reserve(arguments.size());
  for (QString argument : arguments) {
    if (argument.contains(QLatin1Char(' ')) || argument.contains(QLatin1Char('\t')) ||
        argument.contains(QLatin1Char('"'))) {
      argument.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
      argument.replace(QStringLiteral("\""), QStringLiteral("\\\""));
      argument.prepend(QLatin1Char('"'));
      argument.append(QLatin1Char('"'));
    }
    encoded.append(argument);
  }
  return encoded.join(QLatin1Char(' '));
}

} // namespace

namespace Startup {

QString applicationNameFromArguments(int argc, char *argv[],
                                     const QString &fallback) {
  for (int index = 1; index < argc; ++index) {
    const QString argument = QString::fromLocal8Bit(argv[index]);
    if ((argument == QStringLiteral("--name") ||
         argument == QStringLiteral("--n") ||
         argument == QStringLiteral("-n")) && index + 1 < argc) {
      const QString value = QString::fromLocal8Bit(argv[index + 1]);
      return value.isEmpty() ? fallback : value;
    }
    if (argument.startsWith(QStringLiteral("--name="))) {
      const QString value = argument.mid(7);
      return value.isEmpty() ? fallback : value;
    }
    if (argument.startsWith(QStringLiteral("--n=")) ||
        argument.startsWith(QStringLiteral("-n="))) {
      const int equals = argument.indexOf(QLatin1Char('='));
      const QString value = argument.mid(equals + 1);
      return value.isEmpty() ? fallback : value;
    }
  }
  return fallback;
}

QString resolveExecutablePath(const QString &argv0) {
  if (argv0.isEmpty())
    return {};

  QString path = argv0;
  if (!QFileInfo(path).isAbsolute() && !path.contains(QDir::separator())) {
    const QString found = QStandardPaths::findExecutable(path);
    if (!found.isEmpty())
      path = found;
  }

  QFileInfo info(path);
  if (!info.isAbsolute())
    info.setFile(QDir::current().absoluteFilePath(path));
  const QString canonical = info.canonicalFilePath();
  return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

QString findWidevineCdm(const QString &argv0, const QString &configuredLibraryPath) {
  if (const QString configured = canonicalFile(configuredLibraryPath); !configured.isEmpty())
    return configured;

  const QString executablePath = resolveExecutablePath(argv0);
  const QString appDir = QFileInfo(executablePath).absolutePath();
  const QStringList searchPaths = {
      appDir + QStringLiteral("/widevine/libwidevinecdm.so"),
      appDir + QStringLiteral("/../lib/webappcontainer/libwidevinecdm.so"),
      appDir + QStringLiteral("/lib/libwidevinecdm.so"),
      appDir + QStringLiteral("/libwidevinecdm.so"),
      QStringLiteral("/usr/lib/webappcontainer/libwidevinecdm.so"),
      QStringLiteral("/usr/local/lib/webappcontainer/libwidevinecdm.so"),
      QStringLiteral("/usr/lib64/chromium-browser/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"),
      QStringLiteral("/usr/lib/chromium-browser/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"),
      QStringLiteral("/opt/google/chrome/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"),
  };

  for (const QString &path : searchPaths) {
    if (const QString library = canonicalFile(path); !library.isEmpty())
      return library;
  }
  return {};
}

WidevineResult configureWidevine(const QString &argv0, const WidevineOptions &options) {
  WidevineResult result;
  if (!options.enabled)
    return result;

  const QByteArray existingEnvironment = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
  const QStringList existingArguments =
      QProcess::splitCommand(QString::fromLocal8Bit(existingEnvironment));
  if (containsWidevinePath(existingArguments)) {
    result.alreadyConfigured = true;
    return result;
  }

  result.libraryPath = options.searchFallbacks
                           ? findWidevineCdm(argv0, options.configuredLibraryPath)
                           : canonicalFile(options.configuredLibraryPath);
  if (result.libraryPath.isEmpty()) {
    result.error = QStringLiteral("No readable Widevine CDM library was found");
    return result;
  }

  QStringList arguments = existingArguments;
  arguments.append(QStringLiteral("--widevine-path=%1").arg(result.libraryPath));
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS", joinCommandArguments(arguments).toLocal8Bit());
  result.environmentChanged = true;
  return result;
}

} // namespace Startup
