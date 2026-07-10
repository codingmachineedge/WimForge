#include "core/SearchIndex.h"

#include <QCoreApplication>
#include <QTextStream>

using namespace wimforge;

namespace {

int failures = 0;

void check(bool condition, const QString &message)
{
    if (condition)
        return;
    ++failures;
    QTextStream(stderr) << "FAIL: " << message << '\n';
}

SearchEntry entry(const QString &id, const QString &kind, const QString &title,
                  const QString &subtitle, const QStringList &keywords, int page)
{
    SearchEntry value;
    value.id = id;
    value.kind = kind;
    value.titleEn = title;
    value.subtitleEn = subtitle;
    value.keywords = keywords;
    value.page = page;
    return value;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    SearchIndex index;
    index.add(entry(QStringLiteral("page:gpo"), QStringLiteral("page"),
                    QStringLiteral("Group Policy Studio"), QStringLiteral("Browse ADMX policies"),
                    {QStringLiteral("gpo admx registry")}, 3));
    index.add(entry(QStringLiteral("command:refresh-plan"), QStringLiteral("command"),
                    QStringLiteral("Rebuild servicing plan"), QStringLiteral("Review exact commands"),
                    {QStringLiteral("refresh regenerate operations")}, 7));
    index.add(entry(QStringLiteral("feature:wsl"), QStringLiteral("feature"),
                    QStringLiteral("Windows Subsystem for Linux"), QStringLiteral("Optional feature"),
                    {QStringLiteral("wsl linux microsoft windows subsystem")}, 2));
    SearchEntry bilingual = entry(QStringLiteral("setting:motion"), QStringLiteral("setting"),
                                  QStringLiteral("Reduce motion"), QStringLiteral("Accessibility"),
                                  {QStringLiteral("animation transitions")}, 9);
    bilingual.titleZh = QStringLiteral("減少動畫");
    index.add(bilingual);
    index.add(bilingual);

    check(index.size() == 4, QStringLiteral("duplicate IDs must be ignored"));
    check(SearchIndex::normalize(QStringLiteral("  Group-Policy! "))
              == QStringLiteral("group policy"),
          QStringLiteral("normalization should fold punctuation and case"));

    const QList<SearchMatch> exact = index.search(QStringLiteral("Group Policy"));
    check(exact.size() == 1 && exact.first().entry.id == QStringLiteral("page:gpo"),
          QStringLiteral("page title query should find GPO Studio"));
    const QList<SearchMatch> acronym = index.search(QStringLiteral("wsl"));
    check(acronym.size() == 1 && acronym.first().entry.kind == QStringLiteral("feature"),
          QStringLiteral("keyword/acronym query should find WSL"));
    const QList<SearchMatch> translated = index.search(QStringLiteral("動畫"));
    check(translated.size() == 1
              && translated.first().entry.id == QStringLiteral("setting:motion"),
          QStringLiteral("localized titles should be searchable"));
    check(index.search(QStringLiteral("servicing refresh"), 1).size() == 1,
          QStringLiteral("multi-token keyword query should match across fields"));
    check(index.search(QStringLiteral("not present")).isEmpty(),
          QStringLiteral("unmatched queries should be empty"));
    check(index.search(QStringLiteral(" ")).isEmpty(),
          QStringLiteral("blank queries should be empty"));

    return failures == 0 ? 0 : 1;
}
