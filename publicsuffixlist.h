// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#ifndef PUBLICSUFFIXLIST_H
#define PUBLICSUFFIXLIST_H

#include <QSet>
#include <QString>

class PublicSuffixList {
public:
  static PublicSuffixList *instance();

  QString getBaseDomain(const QString &host) const;
  bool isSameDomain(const QString &host1, const QString &host2) const;

private:
  PublicSuffixList();
  void load();

  static PublicSuffixList *s_instance;

  QSet<QString> m_suffixes;   // Normal rules (e.g., "com.au")
  QSet<QString> m_wildcards;  // Wildcard rules (e.g., "*.uk")
  QSet<QString> m_exceptions; // Exception rules (e.g., "!www.ck")
};

#endif // PUBLICSUFFIXLIST_H
