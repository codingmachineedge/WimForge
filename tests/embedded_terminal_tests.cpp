#include "core/EmbeddedTerminalSession.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QThread>

#include <functional>

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

bool waitUntil(const std::function<bool()> &predicate, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    return predicate();
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    TestContext test;

#ifdef Q_OS_WIN
    test.check(wimforge::EmbeddedTerminalSession::isSupported(),
               QStringLiteral("ConPTY is available on supported Windows hosts"));

    const QByteArray originalPath = qgetenv("PATH");
    const QByteArray originalComSpec = qgetenv("COMSPEC");
    qputenv("PATH", QByteArray("C:\\attacker-controlled-path"));
    qputenv("COMSPEC", QByteArray("C:\\attacker-controlled-path\\cmd.exe"));
    QString resolutionError;
    const QString trustedCommand =
        wimforge::EmbeddedTerminalSession::resolveShellExecutable(
            wimforge::EmbeddedTerminalSession::Shell::CommandPrompt,
            &resolutionError);
    qputenv("PATH", originalPath);
    qputenv("COMSPEC", originalComSpec);

    test.check(resolutionError.isEmpty(),
               QStringLiteral("The protected command prompt resolves without error"));
    test.check(QFileInfo(trustedCommand).isAbsolute(),
               QStringLiteral("The selected shell executable is absolute"));
    test.check(QFileInfo(trustedCommand).fileName().compare(
                   QStringLiteral("cmd.exe"), Qt::CaseInsensitive)
                   == 0,
               QStringLiteral("CommandPrompt resolves cmd.exe without consulting COMSPEC"));
    test.check(!trustedCommand.contains(QStringLiteral("attacker-controlled-path"),
                                        Qt::CaseInsensitive),
               QStringLiteral("PATH poisoning cannot redirect shell selection"));

    wimforge::EmbeddedTerminalSession terminal;
    QString streamedOutput;
    QString displayOutput;
    int exitSignalCount = 0;
    QObject::connect(
        &terminal,
        &wimforge::EmbeddedTerminalSession::outputReceived,
        &application,
        [&streamedOutput](const QString &chunk) { streamedOutput.append(chunk); });
    QObject::connect(
        &terminal,
        &wimforge::EmbeddedTerminalSession::displayOutputReceived,
        &application,
        [&displayOutput](const QString &chunk) { displayOutput.append(chunk); });
    QObject::connect(
        &terminal,
        &wimforge::EmbeddedTerminalSession::processExited,
        &application,
        [&exitSignalCount](int, wimforge::EmbeddedTerminalSession::ExitStatus) {
            ++exitSignalCount;
        });

    wimforge::EmbeddedTerminalSession::StartOptions options;
    options.shell = wimforge::EmbeddedTerminalSession::Shell::CommandPrompt;
    options.workingDirectory = QDir::currentPath();
    options.initialSize = QSize(80, 25);
    options.maxTranscriptBytes = 512;
    // Larger than the transcript cap so one drained burst deterministically
    // exercises both pending-output backpressure and transcript truncation.
    options.maxPendingOutputBytes = 1024;
    options.maxPendingInputBytes = 4096;

    test.check(!terminal.startShell(QStringLiteral("C:\\untrusted\\shell.exe"),
                                    QDir::currentPath(),
                                    80,
                                    25),
               QStringLiteral("The QML wrapper rejects arbitrary executable names"));
    test.check(terminal.state() == wimforge::EmbeddedTerminalSession::State::Idle,
               QStringLiteral("Rejected shell selection does not start a process"));

    test.check(terminal.start(options),
               QStringLiteral("An interactive command prompt starts inside ConPTY"));
    test.check(terminal.state()
                   == wimforge::EmbeddedTerminalSession::State::Running,
               QStringLiteral("A started terminal reports Running"));
    test.check(terminal.resize(132, 43),
               QStringLiteral("A live pseudoconsole can be resized"));
    test.check(!terminal.resize(0, 43),
               QStringLiteral("Invalid terminal dimensions are rejected"));
    test.check(!terminal.writeInput(
                   QString(options.maxPendingInputBytes + 1, QLatin1Char('x'))),
               QStringLiteral("Oversized input is rejected before entering the bounded queue"));

    const QString marker = QStringLiteral("WIMFORGE_CONPTY_INTERACTIVE_OK");
    test.check(terminal.writeInput(QStringLiteral("echo %1\r\n").arg(marker)),
               QStringLiteral("Interactive UTF-8 input is accepted"));
    test.check(waitUntil([&streamedOutput, &marker] {
                   return streamedOutput.contains(marker);
               }, 10'000),
               QStringLiteral("ConPTY output is delivered asynchronously"));
    test.check(waitUntil([&displayOutput, &marker] {
                   return displayOutput.contains(marker);
               }, 5'000),
               QStringLiteral("A plain display stream retains printable terminal output"));
    test.check(streamedOutput.contains(QChar(0x1B)),
               QStringLiteral("The raw output signal preserves VT escape sequences"));
    test.check(!terminal.displayTranscript().contains(QChar(0x1B)),
               QStringLiteral("The display transcript never exposes raw escape sequences"));

    // Hold the event loop briefly while the reader continues. The component's
    // coalescing queue must remain bounded and account for discarded bytes.
    test.check(terminal.writeInput(QStringLiteral(
                   "for /L %i in (1,1,600) do @echo 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n")),
               QStringLiteral("A high-volume console command is queued"));
    QThread::msleep(350);
    test.check(waitUntil([&terminal] {
                   return terminal.droppedOutputBytes() > 0;
               }, 5'000),
               QStringLiteral("Backpressure drops and accounts for excess queued output"));
    test.check(terminal.transcript().toUtf8().size() <= options.maxTranscriptBytes,
               QStringLiteral("The retained terminal transcript stays within its byte cap"));
    test.check(terminal.displayTranscript().toUtf8().size()
                   <= options.maxTranscriptBytes,
               QStringLiteral("The plain display transcript stays within the same byte cap"));
    test.check(terminal.transcriptTruncated(),
               QStringLiteral("Transcript truncation is explicitly reported"));

    test.check(terminal.writeInput(QStringLiteral("exit /b 7\r\n")),
               QStringLiteral("The interactive shell accepts its exit command"));
    test.check(waitUntil([&terminal] {
                   return terminal.state()
                       == wimforge::EmbeddedTerminalSession::State::Exited;
               }, 10'000),
               QStringLiteral("Natural shell exit is observed"));
    test.check(terminal.exitStatus()
                   == wimforge::EmbeddedTerminalSession::ExitStatus::NormalExit,
               QStringLiteral("A shell exit is reported as a normal exit"));
    test.check(terminal.exitCode() == 7,
               QStringLiteral("The native process exit code is retained"));
    test.check(exitSignalCount == 1,
               QStringLiteral("Natural exit emits one explicit process-exit signal"));

    terminal.clearTranscript();
    test.check(terminal.transcript().isEmpty()
                   && terminal.displayTranscript().isEmpty()
                   && !terminal.transcriptTruncated(),
               QStringLiteral("Raw and display transcript state clear between sessions"));

    streamedOutput.clear();
    test.check(terminal.start(options),
               QStringLiteral("A completed terminal object can start a new session"));
    terminal.forceStop();
    test.check(waitUntil([&terminal] {
                   return terminal.state()
                       == wimforge::EmbeddedTerminalSession::State::Exited;
               }, 10'000),
               QStringLiteral("Forced stop terminates the contained shell"));
    test.check(terminal.exitStatus()
                   == wimforge::EmbeddedTerminalSession::ExitStatus::Terminated,
               QStringLiteral("Forced termination has a distinct exit status"));
    test.check(exitSignalCount == 2,
               QStringLiteral("Forced exit emits one additional exit signal"));

    test.check(terminal.startShell(QStringLiteral("cmd"),
                                   QDir::currentPath(),
                                   100,
                                   30),
               QStringLiteral("The QML-safe cmd wrapper starts a third session"));
    terminal.stopGracefully(5000);
    test.check(waitUntil([&terminal] {
                   return terminal.state()
                       == wimforge::EmbeddedTerminalSession::State::Exited;
               }, 10'000),
               QStringLiteral("Graceful stop closes the interactive shell"));
    test.check(terminal.exitStatus()
                   == wimforge::EmbeddedTerminalSession::ExitStatus::NormalExit,
               QStringLiteral("A successful graceful stop remains a normal exit"));
#else
    test.check(!wimforge::EmbeddedTerminalSession::isSupported(),
               QStringLiteral("Non-Windows hosts report ConPTY as unsupported"));
    QString resolutionError;
    test.check(wimforge::EmbeddedTerminalSession::resolveShellExecutable(
                   wimforge::EmbeddedTerminalSession::Shell::DefaultShell,
                   &resolutionError)
                   .isEmpty()
                   && !resolutionError.isEmpty(),
               QStringLiteral("Non-Windows shell resolution gives an explicit error"));
    wimforge::EmbeddedTerminalSession terminal;
    test.check(!terminal.start(),
               QStringLiteral("Starting a ConPTY session on non-Windows fails cleanly"));
    test.check(terminal.state()
                   == wimforge::EmbeddedTerminalSession::State::Unsupported,
               QStringLiteral("The unsupported fallback has an explicit state"));
#endif

    return test.failures() == 0 ? 0 : 1;
}
