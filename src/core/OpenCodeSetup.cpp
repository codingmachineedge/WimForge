#include "OpenCodeSetup.h"
#include "ProcessLaunch.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <utility>

namespace wimforge {
namespace {

QString environmentFile(const char *name, const QString &relative)
{
    const QString root = qEnvironmentVariable(name).trimmed();
    return root.isEmpty() ? QString() : QDir(root).filePath(relative);
}

QString findOpenCodeExecutable()
{
#ifdef Q_OS_WIN
    // npm also creates cmd and POSIX shims. Resolve the native executable so
    // prompts are always passed as structured CreateProcess arguments.
    const QStringList names{QStringLiteral("opencode.exe")};
#else
    const QStringList names{QStringLiteral("opencode")};
#endif
    for (const QString &name : names) {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty())
            return QDir::cleanPath(found);
    }

    const QStringList candidates{
        environmentFile("APPDATA", QStringLiteral("npm/opencode.exe")),
        environmentFile("LOCALAPPDATA", QStringLiteral("npm/opencode.exe")),
        environmentFile("APPDATA", QStringLiteral("npm/node_modules/opencode-ai/bin/opencode.exe")),
        environmentFile("LOCALAPPDATA", QStringLiteral("npm/node_modules/opencode-ai/bin/opencode.exe")),
        environmentFile("USERPROFILE", QStringLiteral(".opencode/bin/opencode.exe")),
        environmentFile("LOCALAPPDATA", QStringLiteral("opencode/bin/opencode.exe")),
    };
    for (const QString &candidate : candidates) {
        if (!candidate.isEmpty() && QFileInfo(candidate).isFile())
            return QDir::cleanPath(candidate);
    }
    return {};
}

OpenCodeToolCommand findNpmCommand()
{
#ifdef Q_OS_WIN
    QStringList nodeCandidates;
    const QString pathNode = QStandardPaths::findExecutable(QStringLiteral("node.exe"));
    if (!pathNode.isEmpty())
        nodeCandidates.append(pathNode);
    nodeCandidates.append(environmentFile("ProgramFiles", QStringLiteral("nodejs/node.exe")));
    nodeCandidates.append(environmentFile("ProgramW6432", QStringLiteral("nodejs/node.exe")));
    nodeCandidates.append(environmentFile("ProgramFiles(x86)", QStringLiteral("nodejs/node.exe")));

    QStringList npmRoots;
    const QString npmShim = QStandardPaths::findExecutable(QStringLiteral("npm.cmd"));
    if (!npmShim.isEmpty())
        npmRoots.append(QFileInfo(npmShim).absolutePath());
    for (const QString &node : std::as_const(nodeCandidates)) {
        if (node.isEmpty() || !QFileInfo(node).isFile())
            continue;
        QStringList roots = npmRoots;
        roots.prepend(QFileInfo(node).absolutePath());
        for (const QString &root : std::as_const(roots)) {
            const QString npmCli = QDir(root).filePath(
                QStringLiteral("node_modules/npm/bin/npm-cli.js"));
            if (QFileInfo(npmCli).isFile())
                return {QDir::cleanPath(node), {QDir::cleanPath(npmCli)}};
        }
    }
    return {};
#else
    const QString npm = QStandardPaths::findExecutable(QStringLiteral("npm"));
    return npm.isEmpty() ? OpenCodeToolCommand{} : OpenCodeToolCommand{npm, {}};
#endif
}

QString boundedOutput(const QByteArray &bytes)
{
    constexpr qsizetype maximumCharacters = 8'192;
    QString output = QString::fromLocal8Bit(bytes).trimmed();
    if (output.size() > maximumCharacters)
        output = QStringLiteral("…") + output.right(maximumCharacters);
    return output;
}

class SystemOpenCodeProcessRunner final : public OpenCodeProcessRunner
{
public:
    ~SystemOpenCodeProcessRunner() override { shutdown(); }

    quint64 start(const OpenCodeProcessCommand &command, Completion completed) override
    {
        const quint64 id = ++m_nextId;
        auto *process = new QProcess;
        configureProcessWithoutConsole(*process);
        process->setProgram(command.program);
        process->setArguments(command.arguments);
        process->setProcessChannelMode(QProcess::MergedChannels);
        m_processes.insert(id, process);

        auto finish = [this, id, process, completed = std::move(completed)](
                          OpenCodeProcessResult result) mutable {
            if (process->property("wimforgeHandled").toBool())
                return;
            process->setProperty("wimforgeHandled", true);
            result.output = boundedOutput(process->readAll());
            m_processes.remove(id);
            process->deleteLater();
            completed(result);
        };

        QObject::connect(process, &QProcess::errorOccurred, process,
                         [process, finish](QProcess::ProcessError error) mutable {
            if (error != QProcess::FailedToStart)
                return;
            OpenCodeProcessResult result;
            result.outcome = OpenCodeProcessResult::Outcome::FailedToStart;
            result.error = process->errorString();
            finish(result);
        });
        QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
                         [process, finish](int exitCode, QProcess::ExitStatus exitStatus) mutable {
            OpenCodeProcessResult result;
            result.outcome = process->property("wimforgeTimedOut").toBool()
                ? OpenCodeProcessResult::Outcome::TimedOut
                : OpenCodeProcessResult::Outcome::Finished;
            result.exitCode = exitCode;
            result.normalExit = exitStatus == QProcess::NormalExit;
            result.error = process->errorString();
            finish(result);
        });

        if (command.timeoutMs > 0) {
            auto *timer = new QTimer(process);
            timer->setSingleShot(true);
            QObject::connect(timer, &QTimer::timeout, process, [process] {
                if (process->state() == QProcess::NotRunning)
                    return;
                process->setProperty("wimforgeTimedOut", true);
                process->terminate();
                QTimer::singleShot(1'500, process, [process] {
                    if (process->state() != QProcess::NotRunning)
                        process->kill();
                });
            });
            timer->start(command.timeoutMs);
        }

        process->start();
        return id;
    }

    void shutdown() override
    {
        const QList<QProcess *> processes = m_processes.values();
        m_processes.clear();
        for (QProcess *process : processes) {
            if (!process)
                continue;
            process->setProperty("wimforgeHandled", true);
            QObject::disconnect(process, nullptr, nullptr, nullptr);
            if (process->state() != QProcess::NotRunning) {
                process->terminate();
                if (!process->waitForFinished(1'500)) {
                    process->kill();
                    process->waitForFinished(1'500);
                }
            }
            delete process;
        }
    }

private:
    quint64 m_nextId = 0;
    QHash<quint64, QProcess *> m_processes;
};

QString firstOutputLine(const QString &output)
{
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QString() : lines.first().trimmed();
}

} // namespace

OpenCodeSetupEnvironment systemOpenCodeEnvironment()
{
    return {
        [] { return findOpenCodeExecutable(); },
        [] { return findNpmCommand(); },
        [] { return QStandardPaths::findExecutable(QStringLiteral("winget.exe")); },
        [] { return QDateTime::currentMSecsSinceEpoch(); },
    };
}

std::unique_ptr<OpenCodeProcessRunner> createSystemOpenCodeProcessRunner()
{
    return std::make_unique<SystemOpenCodeProcessRunner>();
}

OpenCodeSetup::OpenCodeSetup(QObject *parent)
    : OpenCodeSetup(systemOpenCodeEnvironment(), createSystemOpenCodeProcessRunner(), parent)
{
}

OpenCodeSetup::OpenCodeSetup(OpenCodeSetupEnvironment environment,
                             std::unique_ptr<OpenCodeProcessRunner> runner,
                             QObject *parent)
    : QObject(parent),
      m_environment(std::move(environment)),
      m_runner(std::move(runner))
{
    if (!m_environment.openCodeExecutable)
        m_environment.openCodeExecutable = [] { return QString(); };
    if (!m_environment.npmCommand)
        m_environment.npmCommand = [] { return OpenCodeToolCommand{}; };
    if (!m_environment.wingetExecutable)
        m_environment.wingetExecutable = [] { return QString(); };
    if (!m_environment.nowMs)
        m_environment.nowMs = [] { return QDateTime::currentMSecsSinceEpoch(); };

    m_executablePath = m_environment.openCodeExecutable();
    m_status = m_executablePath.isEmpty()
        ? QStringLiteral("OpenCode is not installed; automatic setup is queued.")
        : QStringLiteral("OpenCode was found and is waiting for live verification.");
    m_lastTransitionMs = m_environment.nowMs();
}

OpenCodeSetup::~OpenCodeSetup()
{
    shutdown();
}

QString OpenCodeSetup::stateName() const
{
    switch (m_state) {
    case OpenCodeSetupState::Absent: return QStringLiteral("absent");
    case OpenCodeSetupState::Installing: return QStringLiteral("installing");
    case OpenCodeSetupState::Verifying: return QStringLiteral("verifying");
    case OpenCodeSetupState::Ready: return QStringLiteral("ready");
    case OpenCodeSetupState::Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("failed");
}

bool OpenCodeSetup::busy() const
{
    return m_state == OpenCodeSetupState::Installing
        || m_state == OpenCodeSetupState::Verifying;
}

bool OpenCodeSetup::installed() const
{
    return !m_environment.openCodeExecutable().isEmpty();
}

bool OpenCodeSetup::canRetry() const
{
    return !m_shuttingDown && (m_state == OpenCodeSetupState::Absent
                               || m_state == OpenCodeSetupState::Failed);
}

void OpenCodeSetup::ensureReady(Completion completed)
{
    if (m_shuttingDown) {
        if (completed)
            completed(false, QStringLiteral("OpenCode setup is shutting down."));
        return;
    }
    if (completed)
        m_pending.push_back(std::move(completed));

    if (m_state == OpenCodeSetupState::Ready) {
        const QString currentExecutable = m_environment.openCodeExecutable();
        if (!currentExecutable.isEmpty()) {
            m_executablePath = currentExecutable;
            completePending(true, {});
            return;
        }
        transition(OpenCodeSetupState::Absent,
                   QStringLiteral("OpenCode is no longer available; setup will repair it."));
    }
    if (busy())
        return;
    startAttempt();
}

void OpenCodeSetup::retry(Completion completed)
{
    ensureReady(std::move(completed));
}

void OpenCodeSetup::shutdown()
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    ++m_generation;
    if (m_runner)
        m_runner->shutdown();
    completePending(false, QStringLiteral("OpenCode setup was cancelled during shutdown."));
}

void OpenCodeSetup::startAttempt()
{
    ++m_generation;
    m_installedDuringAttempt = false;
    m_version.clear();
    m_executablePath = m_environment.openCodeExecutable();
    if (!m_executablePath.isEmpty()) {
        startVerification();
        return;
    }
    if (!m_environment.npmCommand().isEmpty()) {
        startNpmInstall();
        return;
    }
    if (!m_environment.wingetExecutable().trimmed().isEmpty()) {
        startWinGetInstall();
        return;
    }
    finishFailed(QStringLiteral(
        "OpenCode needs Node.js/npm, but neither npm nor WinGet is available."));
}

void OpenCodeSetup::startWinGetInstall()
{
    const QString winget = m_environment.wingetExecutable().trimmed();
    if (winget.isEmpty()) {
        finishFailed(QStringLiteral("WinGet disappeared before Node.js installation could start."));
        return;
    }
    transition(OpenCodeSetupState::Installing,
               QStringLiteral("Installing Node.js LTS with WinGet…"));
    const quint64 generation = m_generation;
    m_runner->start(
        {winget,
         {QStringLiteral("install"), QStringLiteral("--id"),
          QStringLiteral("OpenJS.NodeJS.LTS"), QStringLiteral("--exact"),
          QStringLiteral("--silent"), QStringLiteral("--accept-package-agreements"),
          QStringLiteral("--accept-source-agreements"),
          QStringLiteral("--disable-interactivity")},
         WinGetInstallTimeoutMs},
        [this, generation](const OpenCodeProcessResult &result) {
            handleWinGetResult(generation, result);
        });
}

void OpenCodeSetup::startNpmInstall()
{
    const OpenCodeToolCommand npm = m_environment.npmCommand();
    if (npm.isEmpty()) {
        finishFailed(QStringLiteral(
            "Node.js installation completed, but npm could not be found. Restart WimForge or retry after Node.js finishes updating PATH."));
        return;
    }
    transition(OpenCodeSetupState::Installing,
               QStringLiteral("Installing OpenCode automatically with npm…"));
    QStringList arguments = npm.prefixArguments;
    arguments.append({QStringLiteral("install"), QStringLiteral("-g"),
                      QStringLiteral("opencode-ai@latest")});
    const quint64 generation = m_generation;
    m_runner->start(
        {npm.program, arguments, NpmInstallTimeoutMs},
        [this, generation](const OpenCodeProcessResult &result) {
            handleNpmResult(generation, result);
        });
}

void OpenCodeSetup::startVerification()
{
    m_executablePath = m_environment.openCodeExecutable();
    if (m_executablePath.isEmpty()) {
        finishFailed(QStringLiteral(
            "OpenCode installation returned success, but no native executable was found in PATH or the npm prefix."));
        return;
    }
    transition(OpenCodeSetupState::Verifying,
               QStringLiteral("Live-verifying OpenCode…"));
    const quint64 generation = m_generation;
    m_runner->start(
        {m_executablePath, {QStringLiteral("--version")}, VerifyTimeoutMs},
        [this, generation](const OpenCodeProcessResult &result) {
            handleVerificationResult(generation, result);
        });
}

void OpenCodeSetup::handleWinGetResult(quint64 generation,
                                       const OpenCodeProcessResult &result)
{
    if (m_shuttingDown || generation != m_generation)
        return;
    if (!result.succeeded()) {
        finishFailed(processFailure(QStringLiteral("Node.js installation"), result));
        return;
    }
    startNpmInstall();
}

void OpenCodeSetup::handleNpmResult(quint64 generation,
                                    const OpenCodeProcessResult &result)
{
    if (m_shuttingDown || generation != m_generation)
        return;
    if (!result.succeeded()) {
        finishFailed(processFailure(QStringLiteral("OpenCode npm installation"), result));
        return;
    }
    m_installedDuringAttempt = true;
    startVerification();
}

void OpenCodeSetup::handleVerificationResult(quint64 generation,
                                             const OpenCodeProcessResult &result)
{
    if (m_shuttingDown || generation != m_generation)
        return;
    if (!result.succeeded()) {
        finishFailed(processFailure(QStringLiteral("OpenCode live verification"), result));
        return;
    }
    const QString version = firstOutputLine(result.output);
    if (version.isEmpty()) {
        finishFailed(QStringLiteral(
            "OpenCode live verification returned no version text."));
        return;
    }
    finishReady(version);
}

void OpenCodeSetup::transition(OpenCodeSetupState state,
                               const QString &status,
                               const QString &error)
{
    m_state = state;
    m_status = status;
    m_error = error;
    m_lastTransitionMs = m_environment.nowMs();
    emit changed();
}

void OpenCodeSetup::finishReady(const QString &version)
{
    m_version = version;
    const bool installedDuringAttempt = m_installedDuringAttempt;
    transition(OpenCodeSetupState::Ready,
               QStringLiteral("OpenCode is installed and live-verified: %1").arg(version));
    emit becameReady(installedDuringAttempt);
    completePending(true, {});
}

void OpenCodeSetup::finishFailed(const QString &message)
{
    m_executablePath.clear();
    transition(OpenCodeSetupState::Failed, message, message);
    emit failed(message);
    completePending(false, message);
}

void OpenCodeSetup::completePending(bool ready, const QString &error)
{
    std::vector<Completion> pending;
    pending.swap(m_pending);
    for (Completion &completed : pending) {
        if (completed)
            completed(ready, error);
    }
}

QString OpenCodeSetup::processFailure(const QString &operation,
                                      const OpenCodeProcessResult &result) const
{
    if (result.outcome == OpenCodeProcessResult::Outcome::TimedOut)
        return QStringLiteral("%1 timed out.").arg(operation);
    if (result.outcome == OpenCodeProcessResult::Outcome::FailedToStart)
        return QStringLiteral("%1 could not start: %2").arg(operation, result.error);
    const QString detail = !result.output.isEmpty() ? result.output : result.error;
    return QStringLiteral("%1 failed (exit %2): %3")
        .arg(operation)
        .arg(result.exitCode)
        .arg(detail.isEmpty() ? QStringLiteral("No diagnostic output was returned.") : detail);
}

} // namespace wimforge
