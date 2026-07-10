#include "AppController.h"
#include "cli/CliRunner.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif

namespace {

void applicationMessageHandler(QtMsgType type,
                               const QMessageLogContext &context,
                               const QString &message)
{
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    const QString directory = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("logs"));
    QDir().mkpath(directory);
    QFile file(QDir(directory).filePath(QStringLiteral("application.log")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    const QString level = type == QtDebugMsg ? QStringLiteral("debug")
        : type == QtInfoMsg ? QStringLiteral("info")
        : type == QtWarningMsg ? QStringLiteral("warning")
        : type == QtCriticalMsg ? QStringLiteral("critical") : QStringLiteral("fatal");
    QTextStream stream(&file);
    stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
           << " [" << level << "] " << message;
    if (context.file)
        stream << " (" << context.file << ':' << context.line << ')';
    stream << '\n';
    stream.flush();
}

bool isCliInvocation(int argc, char *argv[])
{
    const QStringList commands{
        QStringLiteral("help"), QStringLiteral("project"), QStringLiteral("config"),
        QStringLiteral("plan"), QStringLiteral("dry-run"), QStringLiteral("apply"),
        QStringLiteral("history"), QStringLiteral("notification"),
        QStringLiteral("notifications"), QStringLiteral("unattend"),
        QStringLiteral("package"), QStringLiteral("packages"),
        QStringLiteral("package-manager"), QStringLiteral("gpo"),
        QStringLiteral("bundle"), QStringLiteral("action-history"),
        QStringLiteral("winforge"),
    };
    for (int index = 1; index < argc; ++index)
        if (QString::fromLocal8Bit(argv[index]) == QStringLiteral("--cli"))
            return true;
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        if (argument == QStringLiteral("--demo") || argument == QStringLiteral("--language")
            || argument == QStringLiteral("--page"))
            return false;
    }
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        if (argument == QStringLiteral("--config")
            || argument.startsWith(QLatin1Char('@'))
            || commands.contains(argument.toLower())) {
            return true;
        }
    }
    return false;
}

void writeCliOutput(const QString &standardOutput, const QString &standardError)
{
#ifdef Q_OS_WIN
    if (::GetConsoleWindow() == nullptr)
        ::AttachConsole(ATTACH_PARENT_PROCESS);
    FILE *stream = nullptr;
    if (stdout && _fileno(stdout) < 0)
        freopen_s(&stream, "CONOUT$", "w", stdout);
    if (stderr && _fileno(stderr) < 0)
        freopen_s(&stream, "CONOUT$", "w", stderr);
#endif
    if (!standardOutput.isEmpty()) {
        QTextStream output(stdout);
        output << standardOutput;
        output.flush();
    }
    if (!standardError.isEmpty()) {
        QTextStream error(stderr);
        error << standardError;
        error.flush();
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("WimForge"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("github.com/codingmachineedge"));
    QCoreApplication::setApplicationName(QStringLiteral("WimForge"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(WIMFORGE_VERSION));

    if (isCliInvocation(argc, argv)) {
        QCoreApplication application(argc, argv);
        QStringList arguments = application.arguments().mid(1);
        arguments.removeAll(QStringLiteral("--cli"));
        const wimforge::CliResult result = wimforge::CliRunner().run(arguments);
        writeCliOutput(result.standardOutput, result.standardError);
        return result.exitCode();
    }

    QQuickStyle::setStyle(QStringLiteral("Material"));

    QGuiApplication application(argc, argv);
    qInstallMessageHandler(applicationMessageHandler);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Open-source, Git-backed Windows image customization studio."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("project"), QStringLiteral("Open a project folder."), QStringLiteral("folder")});
    parser.addOption({QStringLiteral("demo"), QStringLiteral("Open a safe populated demo project for screenshots and evaluation.")});
    parser.addOption({QStringLiteral("language"), QStringLiteral("UI language: en, zh-HK, or bilingual."), QStringLiteral("mode")});
    parser.addOption({QStringLiteral("page"), QStringLiteral("Open a studio page: overview, source, customize, gpo, unattended, packages, winforge, plan, history, or settings."), QStringLiteral("id")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Save a PNG of the selected page after startup, then exit."), QStringLiteral("path")});
    parser.process(application);

    AppController controller;
    if (parser.isSet(QStringLiteral("demo"))) {
        QString error;
        if (!controller.loadDemoProject(&error))
            qWarning().noquote() << error;
    } else if (parser.isSet(QStringLiteral("project"))) {
        controller.openProject(parser.value(QStringLiteral("project")));
    }
    const QString language = parser.value(QStringLiteral("language")).toLower();
    if (language == QStringLiteral("en")) controller.setLanguageMode(0);
    else if (language == QStringLiteral("zh-hk") || language == QStringLiteral("yue")) controller.setLanguageMode(1);
    else if (language == QStringLiteral("bilingual")) controller.setLanguageMode(2);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    const QStringList pageIds{QStringLiteral("overview"), QStringLiteral("source"),
        QStringLiteral("customize"), QStringLiteral("gpo"), QStringLiteral("unattended"),
        QStringLiteral("packages"), QStringLiteral("winforge"), QStringLiteral("plan"),
        QStringLiteral("history"), QStringLiteral("settings")};
    const int requestedPage = qMax(0, pageIds.indexOf(parser.value(QStringLiteral("page")).toLower()));
    engine.rootContext()->setContextProperty(QStringLiteral("startupPage"), requestedPage);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &application, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("WimForge"), QStringLiteral("Main"));

    if (parser.isSet(QStringLiteral("screenshot"))) {
        const QString screenshotPath = QFileInfo(parser.value(QStringLiteral("screenshot")))
                                           .absoluteFilePath();
        const auto rootObjects = engine.rootObjects();
        auto *window = rootObjects.isEmpty()
            ? nullptr
            : qobject_cast<QQuickWindow *>(rootObjects.constFirst());
        if (!window) {
            qCritical().noquote() << QStringLiteral("Unable to capture the documentation screenshot: the root window is unavailable.");
            return 2;
        }

        QTimer::singleShot(1500, &application,
                           [&application, window, screenshotPath] {
            if (!QDir().mkpath(QFileInfo(screenshotPath).absolutePath())) {
                qCritical().noquote() << QStringLiteral("Unable to create the screenshot output directory: %1")
                                             .arg(QFileInfo(screenshotPath).absolutePath());
                application.exit(3);
                return;
            }
            QImage image = window->grabWindow();
            const QSize viewportSize = window->size();
            if (image.width() >= viewportSize.width()
                && image.height() >= viewportSize.height()
                && image.size() != viewportSize) {
                image = image.copy(0, 0, viewportSize.width(), viewportSize.height());
            }
            image.setDevicePixelRatio(1.0);
            if (image.isNull() || !image.save(screenshotPath, "PNG")) {
                qCritical().noquote() << QStringLiteral("Unable to save the documentation screenshot: %1")
                                             .arg(screenshotPath);
                application.exit(4);
                return;
            }
            application.quit();
        });
    }
    return application.exec();
}
