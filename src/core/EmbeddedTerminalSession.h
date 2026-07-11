#pragma once

#include <QByteArray>
#include <QObject>
#include <QSize>
#include <QStringList>

#include <memory>

namespace wimforge {

// Hosts an interactive Windows console application inside a ConPTY. The
// class owns the pseudoconsole and all of its pipes; callers only exchange
// decoded UTF-8/VT text through signals and writeInput(). No console window is
// allocated for the child process.
class EmbeddedTerminalSession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(QString transcript READ transcript NOTIFY transcriptChanged)
    Q_PROPERTY(QString displayTranscript READ displayTranscript
                   NOTIFY displayTranscriptChanged)
    Q_PROPERTY(bool transcriptTruncated READ transcriptTruncated
                   NOTIFY transcriptTruncatedChanged)
    Q_PROPERTY(quint64 droppedOutputBytes READ droppedOutputBytes
                   NOTIFY droppedOutputBytesChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(int exitCode READ exitCode NOTIFY exitChanged)
    Q_PROPERTY(ExitStatus exitStatus READ exitStatus NOTIFY exitChanged)

public:
    enum class State {
        Idle,
        Starting,
        Running,
        Stopping,
        Exited,
        Failed,
        Unsupported
    };
    Q_ENUM(State)

    enum class ExitStatus {
        NotExited,
        NormalExit,
        Crashed,
        Terminated
    };
    Q_ENUM(ExitStatus)

    enum class Shell {
        DefaultShell,
        PowerShell,
        CommandPrompt
    };
    Q_ENUM(Shell)

    struct StartOptions {
        Shell shell = Shell::DefaultShell;
        QStringList arguments;
        QString workingDirectory;
        QSize initialSize = QSize(120, 30);
        qsizetype maxTranscriptBytes = 1024 * 1024;
        qsizetype maxPendingOutputBytes = 256 * 1024;
        qsizetype maxPendingInputBytes = 64 * 1024;
    };

    explicit EmbeddedTerminalSession(QObject *parent = nullptr);
    ~EmbeddedTerminalSession() override;

    EmbeddedTerminalSession(const EmbeddedTerminalSession &) = delete;
    EmbeddedTerminalSession &operator=(const EmbeddedTerminalSession &) = delete;

    [[nodiscard]] State state() const;
    [[nodiscard]] bool running() const;
    [[nodiscard]] QString transcript() const;
    [[nodiscard]] QString displayTranscript() const;
    [[nodiscard]] bool transcriptTruncated() const;
    [[nodiscard]] quint64 droppedOutputBytes() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] int exitCode() const;
    [[nodiscard]] ExitStatus exitStatus() const;

    // Starts the selected trusted system shell. Shell executables are always
    // resolved from GetSystemDirectoryW(), never PATH, COMSPEC, or another
    // caller-controlled environment variable.
    bool start();
    bool start(const StartOptions &options);

    // QML-facing convenience wrapper. Only "default", "powershell", and
    // "cmd"/"command-prompt" are accepted; arbitrary executable paths are
    // intentionally not part of this API.
    Q_INVOKABLE bool startShell(const QString &shellName,
                                const QString &workingDirectory,
                                int columns = 120,
                                int rows = 30);

    // Input is queued and written on a worker thread so a stalled console
    // client cannot block the UI thread. The queue is bounded by StartOptions.
    Q_INVOKABLE bool writeInput(const QString &text);
    Q_INVOKABLE bool resize(int columns, int rows);
    Q_INVOKABLE void stopGracefully(int forceAfterMs = 3000);
    Q_INVOKABLE void forceStop();
    Q_INVOKABLE void clearTranscript();

    [[nodiscard]] static bool isSupported();
    [[nodiscard]] static QString resolveShellExecutable(
        Shell shell = Shell::DefaultShell,
        QString *error = nullptr);

signals:
    void stateChanged();
    // Decoded UTF-8 output. ANSI/VT control sequences are deliberately kept
    // intact so a terminal renderer can interpret them.
    void outputReceived(const QString &utf8Vt);
    // Plain display text derived from outputReceived(). It removes ANSI/VT
    // control traffic and applies common cursor-editing controls so a basic
    // QML TextArea never receives raw escape sequences.
    void displayOutputReceived(const QString &plainText);
    void outputDiscarded(quint64 totalDiscardedBytes);
    void transcriptChanged();
    void displayTranscriptChanged();
    void transcriptTruncatedChanged();
    void droppedOutputBytesChanged();
    void errorChanged();
    void errorOccurred(const QString &message);
    void exitChanged();
    void processExited(int exitCode, ExitStatus exitStatus);

private:
    struct Impl;

    void setState(State state);
    void setError(const QString &message, bool fatal);
    void enqueueOutputFromWorker(const QByteArray &bytes);
    void reportWorkerError(const QString &message);
    void drainOutput();
    void appendTranscript(const QString &text);
    void appendDisplayOutput(const QString &utf8Vt);
    void handleNativeExit(quint32 nativeExitCode, bool forced);
    void joinFinishedWorkers();
    void cleanupNativeResources();

    std::unique_ptr<Impl> m_impl;
    State m_state = State::Idle;
    QString m_transcript;
    QString m_displayTranscript;
    QString m_displayEscapeCarry;
    QByteArray m_utf8Carry;
    bool m_transcriptTruncated = false;
    quint64 m_droppedOutputBytes = 0;
    QString m_errorString;
    int m_exitCode = -1;
    ExitStatus m_exitStatus = ExitStatus::NotExited;
    qsizetype m_maxTranscriptBytes = 1024 * 1024;
    quint64 m_sessionGeneration = 0;
};

} // namespace wimforge
