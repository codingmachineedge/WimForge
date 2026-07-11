#include "core/ProcessLaunch.h"

#include <QCoreApplication>
#include <QProcess>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <qt_windows.h>
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

int runCapturedChild()
{
    QTextStream(stdout) << "captured stdout\n";
    QTextStream(stderr) << "captured stderr\n";
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().contains(QStringLiteral("--captured-child")))
        return runCapturedChild();

    TestContext test;
    QProcess process;

#ifdef Q_OS_WIN
    int previousModifierCalls = 0;
    process.setCreateProcessArgumentsModifier(
        [&previousModifierCalls](QProcess::CreateProcessArguments *) {
            ++previousModifierCalls;
        });
#endif

    wimforge::configureProcessWithoutConsole(process);

#ifdef Q_OS_WIN
    const QProcess::CreateProcessArgumentModifier modifier =
        process.createProcessArgumentsModifier();
    test.check(static_cast<bool>(modifier),
               QStringLiteral("Windows child processes receive a CreateProcess modifier"));
    QProcess::CreateProcessArguments arguments{};
    arguments.flags = CREATE_UNICODE_ENVIRONMENT;
    modifier(&arguments);
    test.check((arguments.flags & CREATE_NO_WINDOW) != 0,
               QStringLiteral("Windows child processes use CREATE_NO_WINDOW"));
    test.check(previousModifierCalls == 1,
               QStringLiteral("The no-console helper preserves an existing modifier"));
#endif

    process.setProgram(QCoreApplication::applicationFilePath());
    process.setArguments({QStringLiteral("--captured-child")});
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    test.check(process.waitForStarted(10'000),
               QStringLiteral("The configured child process starts"));
    test.check(process.waitForFinished(10'000),
               QStringLiteral("The configured child process finishes"));
    test.check(process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
               QStringLiteral("The configured child process exits normally"));
    test.check(process.readAllStandardOutput().contains("captured stdout"),
               QStringLiteral("CREATE_NO_WINDOW preserves captured stdout"));
    test.check(process.readAllStandardError().contains("captured stderr"),
               QStringLiteral("CREATE_NO_WINDOW preserves captured stderr"));

    return test.failures() == 0 ? 0 : 1;
}
