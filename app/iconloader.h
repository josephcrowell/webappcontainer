// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QIcon>
#include <QString>

namespace IconLoader {

QIcon load(const QString &path);
bool isUsable(const QString &path);

} // namespace IconLoader
