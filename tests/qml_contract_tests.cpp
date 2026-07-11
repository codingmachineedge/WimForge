#include <QCoreApplication>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextStream>

#ifndef WIMFORGE_SOURCE_DIR
#error WIMFORGE_SOURCE_DIR must point at the WimForge source tree
#endif

namespace {

class TestContext
{
public:
    void check(bool condition, const QString &message)
    {
        if (condition)
            return;
        ++m_failures;
        QTextStream(stderr) << "FAIL: " << message << '\n';
    }

    [[nodiscard]] int failures() const { return m_failures; }

private:
    int m_failures = 0;
};

QString readText(const QString &path, TestContext *test)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        test->check(false, QStringLiteral("Could not read %1: %2").arg(path, file.errorString()));
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QSet<QString> captures(const QString &text, const QRegularExpression &expression)
{
    QSet<QString> values;
    auto matches = expression.globalMatch(text);
    while (matches.hasNext())
        values.insert(matches.next().captured(1));
    return values;
}

QString lowerFirst(QString value)
{
    if (!value.isEmpty())
        value[0] = value.at(0).toLower();
    return value;
}

qsizetype matchingBrace(const QString &text, qsizetype openingBrace)
{
    int depth = 0;
    QChar quote;
    bool escaped = false;
    bool lineComment = false;
    bool blockComment = false;
    for (qsizetype index = openingBrace; index < text.size(); ++index) {
        const QChar current = text.at(index);
        const QChar next = index + 1 < text.size() ? text.at(index + 1) : QChar{};
        if (lineComment) {
            if (current == QLatin1Char('\n'))
                lineComment = false;
            continue;
        }
        if (blockComment) {
            if (current == QLatin1Char('*') && next == QLatin1Char('/')) {
                blockComment = false;
                ++index;
            }
            continue;
        }
        if (!quote.isNull()) {
            if (escaped) {
                escaped = false;
            } else if (current == QLatin1Char('\\')) {
                escaped = true;
            } else if (current == quote) {
                quote = {};
            }
            continue;
        }
        if (current == QLatin1Char('/') && next == QLatin1Char('/')) {
            lineComment = true;
            ++index;
        } else if (current == QLatin1Char('/') && next == QLatin1Char('*')) {
            blockComment = true;
            ++index;
        } else if (current == QLatin1Char('"') || current == QLatin1Char('\'')) {
            quote = current;
        } else if (current == QLatin1Char('{')) {
            ++depth;
        } else if (current == QLatin1Char('}')) {
            --depth;
            if (depth == 0)
                return index;
        }
    }
    return -1;
}

QSet<QString> appConnectionHandlers(const QString &text,
                                    const QRegularExpression &handlerExpression)
{
    QSet<QString> handlers;
    const QRegularExpression connectionsExpression(
        QStringLiteral(R"(\bConnections\s*\{)"));
    const QRegularExpression appTargetExpression(
        QStringLiteral(R"(\btarget\s*:\s*(?:root\.)?(?:app|controller)\b)"));
    qsizetype offset = 0;
    while (offset < text.size()) {
        const QRegularExpressionMatch connection = connectionsExpression.match(text, offset);
        if (!connection.hasMatch())
            break;
        const qsizetype openingBrace = text.indexOf(QLatin1Char('{'),
                                                     connection.capturedStart());
        const qsizetype closingBrace = matchingBrace(text, openingBrace);
        if (openingBrace < 0 || closingBrace < 0)
            break;
        const QString block = text.mid(openingBrace + 1,
                                       closingBrace - openingBrace - 1);
        if (appTargetExpression.match(block).hasMatch())
            handlers.unite(captures(block, handlerExpression));
        offset = closingBrace + 1;
    }
    return handlers;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    TestContext test;
    const QString sourceRoot = QString::fromUtf8(WIMFORGE_SOURCE_DIR);
    const QString controllerHeader = readText(
        sourceRoot + QStringLiteral("/src/AppController.h"), &test);

    const QRegularExpression propertyExpression(
        QStringLiteral(R"(Q_PROPERTY\([^\n\)]*\b([A-Za-z_][A-Za-z0-9_]*)\s+READ\b)"));
    const QRegularExpression writablePropertyExpression(
        QStringLiteral(R"(Q_PROPERTY\([^\n\)]*\b([A-Za-z_][A-Za-z0-9_]*)\s+READ\b[^\n\)]*\bWRITE\b)"));
    const QRegularExpression invokableExpression(
        QStringLiteral(R"(Q_INVOKABLE\s+[^;\n\(]*\b([A-Za-z_][A-Za-z0-9_]*)\s*\()"));

    QSet<QString> readableApi = captures(controllerHeader, propertyExpression);
    const QSet<QString> writableApi = captures(controllerHeader, writablePropertyExpression);
    readableApi.unite(captures(controllerHeader, invokableExpression));

    const qsizetype signalsStart = controllerHeader.indexOf(QStringLiteral("signals:"));
    const qsizetype signalsEnd = controllerHeader.indexOf(QStringLiteral("private:"), signalsStart);
    test.check(signalsStart >= 0 && signalsEnd > signalsStart,
               QStringLiteral("AppController signals section could not be parsed"));
    const QString signalsSection = signalsStart >= 0 && signalsEnd > signalsStart
        ? controllerHeader.mid(signalsStart, signalsEnd - signalsStart)
        : QString();
    const QRegularExpression signalExpression(
        QStringLiteral(R"(^\s*void\s+([A-Za-z_][A-Za-z0-9_]*)\s*\()"),
        QRegularExpression::MultilineOption);
    const QSet<QString> controllerSignals = captures(signalsSection, signalExpression);

    const QRegularExpression appReferenceExpression(
        QStringLiteral(R"(\bapp\.([A-Za-z_][A-Za-z0-9_]*))"));
    const QRegularExpression appAssignmentExpression(
        QStringLiteral(R"(\bapp\.([A-Za-z_][A-Za-z0-9_]*)\s*=(?!=))"));
    const QRegularExpression signalHandlerExpression(
        QStringLiteral(R"(\bfunction\s+on([A-Z][A-Za-z0-9_]*)\s*\()"));

    int qmlFileCount = 0;
    QDirIterator iterator(sourceRoot + QStringLiteral("/qml"),
                          {QStringLiteral("*.qml")}, QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QString text = readText(path, &test);
        ++qmlFileCount;

        for (const QString &name : captures(text, appReferenceExpression)) {
            test.check(readableApi.contains(name),
                       QStringLiteral("%1 references unknown AppController member app.%2")
                           .arg(QDir(sourceRoot).relativeFilePath(path), name));
        }
        for (const QString &name : captures(text, appAssignmentExpression)) {
            test.check(writableApi.contains(name),
                       QStringLiteral("%1 assigns read-only or unknown AppController property app.%2")
                           .arg(QDir(sourceRoot).relativeFilePath(path), name));
        }
        for (const QString &handler : appConnectionHandlers(text, signalHandlerExpression)) {
            const QString signal = lowerFirst(handler);
            test.check(controllerSignals.contains(signal),
                       QStringLiteral("%1 handles unknown AppController signal %2")
                           .arg(QDir(sourceRoot).relativeFilePath(path), signal));
        }
    }

    test.check(qmlFileCount >= 15,
               QStringLiteral("Expected to scan the complete QML module, found only %1 files")
                   .arg(qmlFileCount));
    if (test.failures() == 0)
        QTextStream(stdout) << "QML/AppController contract is valid across " << qmlFileCount
                            << " files.\n";
    return test.failures() == 0 ? 0 : 1;
}
