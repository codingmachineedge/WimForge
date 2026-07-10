#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace wimforge {

struct SearchEntry
{
    QString id;
    QString kind;
    QString titleEn;
    QString titleZh;
    QString subtitleEn;
    QString subtitleZh;
    QStringList keywords;
    int page = -1;
    QString action;
    QJsonObject payload;

    [[nodiscard]] bool valid() const;
};

struct SearchMatch
{
    SearchEntry entry;
    int score = 0;
};

class SearchIndex
{
public:
    void clear();
    void add(SearchEntry entry);
    [[nodiscard]] qsizetype size() const;
    [[nodiscard]] QList<SearchMatch> search(const QString &query, int limit = 50) const;

    [[nodiscard]] static QString normalize(const QString &value);

private:
    QList<SearchEntry> m_entries;
};

} // namespace wimforge
