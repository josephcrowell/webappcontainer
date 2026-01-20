// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "publicsuffixlist.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>

PublicSuffixList *PublicSuffixList::s_instance = nullptr;

PublicSuffixList *PublicSuffixList::instance() {
  if (!s_instance) {
    s_instance = new PublicSuffixList();
  }
  return s_instance;
}

PublicSuffixList::PublicSuffixList() { load(); }

void PublicSuffixList::load() {
  // Try to load from resources first, then from file
  QFile file(":/data/public_suffix_list.dat");
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    // Fallback to local file
    file.setFileName("public_suffix_list.dat");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qWarning() << "Could not load public suffix list";
      return;
    }
  }

  QTextStream in(&file);
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();

    // Skip comments and empty lines
    if (line.isEmpty() || line.startsWith("//")) {
      continue;
    }

    // Handle exception rules (start with !)
    if (line.startsWith("!")) {
      m_exceptions.insert(line.mid(1).toLower());
    }
    // Handle wildcard rules (start with *)
    else if (line.startsWith("*.")) {
      m_wildcards.insert(line.mid(2).toLower());
    }
    // Normal rules
    else {
      m_suffixes.insert(line.toLower());
    }
  }

  file.close();
  qDebug() << "Loaded public suffix list:" << m_suffixes.size() << "suffixes,"
           << m_wildcards.size() << "wildcards," << m_exceptions.size()
           << "exceptions";
}

QString PublicSuffixList::getBaseDomain(const QString &host) const {
  QString hostname = host.toLower();
  QStringList parts = hostname.split('.');

  if (parts.size() < 2) {
    return hostname;
  }

  // Find the longest matching suffix
  int suffixLength = 0;

  for (int i = 0; i < parts.size(); ++i) {
    QString candidate = parts.mid(i).join('.');
    QString wildcardCandidate = parts.mid(i + 1).join('.');

    // Check for exception rules first (they take priority)
    if (m_exceptions.contains(candidate)) {
      // Exception found - the suffix is everything after the first part of the
      // exception
      suffixLength = parts.size() - i - 1;
      break;
    }

    // Check for exact match
    if (m_suffixes.contains(candidate)) {
      suffixLength = parts.size() - i;
      break;
    }

    // Check for wildcard match
    if (m_wildcards.contains(wildcardCandidate)) {
      suffixLength = parts.size() - i;
      break;
    }
  }

  // Default to 1 if no suffix found (treat last part as TLD)
  if (suffixLength == 0) {
    suffixLength = 1;
  }

  // Base domain is suffix + 1 part (the registered domain)
  int baseDomainParts = suffixLength + 1;

  if (baseDomainParts > parts.size()) {
    return hostname;
  }

  return parts.mid(parts.size() - baseDomainParts).join('.');
}

bool PublicSuffixList::isSameDomain(const QString &host1,
                                    const QString &host2) const {
  return getBaseDomain(host1) == getBaseDomain(host2);
}
