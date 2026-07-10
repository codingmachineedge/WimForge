#include "SearchIndex.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>

namespace wimforge {
namespace {

QString acronym(const QString &value)
{
    QString result;
    for (const QString &word : value.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        result.append(word.front());
    return result;
}

bool hasWordPrefix(const QString &text, const QString &token)
{
    for (const QString &word : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (word.startsWith(token))
            return true;
    }
    return false;
}

int scoreField(const QString &field, const QString &query, const QStringList &tokens,
               int exact, int prefix, int contains)
{
    if (field.isEmpty())
        return 0;
    int score = 0;
    if (field == query)
        score = exact;
    else if (field.startsWith(query))
        score = prefix;
    else if (field.contains(query))
        score = contains;
    else if (acronym(field).startsWith(query))
        score = contains - 20;

    int tokenScore = 0;
    for (const QString &token : tokens) {
        if (hasWordPrefix(field, token))
            tokenScore += 30;
        else if (field.contains(token))
            tokenScore += 12;
    }
    return score + tokenScore;
}

} // namespace

bool SearchEntry::valid() const
{
    return !id.trimmed().isEmpty() && !kind.trimmed().isEmpty()
        && !titleEn.trimmed().isEmpty() && page >= -1;
}

void SearchIndex::clear()
{
    m_entries.clear();
}

void SearchIndex::add(SearchEntry entry)
{
    if (!entry.valid())
        return;
    const auto duplicate = std::find_if(m_entries.cbegin(), m_entries.cend(),
                                        [&entry](const SearchEntry &existing) {
        return existing.id == entry.id;
    });
    if (duplicate == m_entries.cend())
        m_entries.append(std::move(entry));
}

qsizetype SearchIndex::size() const
{
    return m_entries.size();
}

QString SearchIndex::normalize(const QString &value)
{
    static const QRegularExpression punctuation(QStringLiteral("[^\\p{L}\\p{N}]+"));
    QString normalized = value.normalized(QString::NormalizationForm_KD).toCaseFolded();
    normalized.replace(punctuation, QStringLiteral(" "));
    return normalized.simplified();
}

QList<SearchMatch> SearchIndex::search(const QString &query, int limit) const
{
    const QString normalizedQuery = normalize(query);
    if (normalizedQuery.isEmpty() || limit <= 0)
        return {};
    const QStringList tokens = normalizedQuery.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    QList<SearchMatch> matches;
    for (const SearchEntry &entry : m_entries) {
        const QString titleEn = normalize(entry.titleEn);
        const QString titleZh = normalize(entry.titleZh);
        const QString subtitleEn = normalize(entry.subtitleEn);
        const QString subtitleZh = normalize(entry.subtitleZh);
        const QString keywordText = normalize(entry.keywords.join(QLatin1Char(' ')));
        const QString completeText = QStringList{titleEn, titleZh, subtitleEn, subtitleZh,
                                                 keywordText, normalize(entry.kind)}
                                         .join(QLatin1Char(' '));
        const bool containsEveryToken = std::all_of(tokens.cbegin(), tokens.cend(),
                                                    [&completeText](const QString &token) {
            return completeText.contains(token);
        });
        if (!containsEveryToken)
            continue;

        int score = scoreField(titleEn, normalizedQuery, tokens, 1'000, 850, 650);
        score = qMax(score, scoreField(titleZh, normalizedQuery, tokens, 1'000, 850, 650));
        score += scoreField(keywordText, normalizedQuery, tokens, 360, 300, 220);
        score += qMax(scoreField(subtitleEn, normalizedQuery, tokens, 260, 220, 150),
                      scoreField(subtitleZh, normalizedQuery, tokens, 260, 220, 150));
        if (entry.kind == QStringLiteral("page"))
            score += 25;
        else if (entry.kind == QStringLiteral("command"))
            score += 15;
        matches.append(SearchMatch{entry, score});
    }

    std::sort(matches.begin(), matches.end(), [](const SearchMatch &left,
                                                  const SearchMatch &right) {
        if (left.score != right.score)
            return left.score > right.score;
        const int titleOrder = QString::compare(left.entry.titleEn, right.entry.titleEn,
                                                Qt::CaseInsensitive);
        if (titleOrder != 0)
            return titleOrder < 0;
        return left.entry.id < right.entry.id;
    });
    if (matches.size() > limit)
        matches.resize(limit);
    return matches;
}

} // namespace wimforge
