#include "VmLabScope.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace wimforge::vmlab {
namespace {

void setError(QString *target, const QString &message)
{
    if (target)
        *target = message;
}

bool isLink(const QFileInfo &info)
{
    if (info.isSymLink())
        return true;
#ifdef Q_OS_WIN
    return info.isJunction();
#else
    return false;
#endif
}

Qt::CaseSensitivity pathSensitivity()
{
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

bool contained(const QString &root, const QString &candidate)
{
    const QString cleanRoot = QDir::fromNativeSeparators(QDir::cleanPath(root));
    const QString cleanCandidate = QDir::fromNativeSeparators(QDir::cleanPath(candidate));
    return cleanCandidate.startsWith(cleanRoot + QLatin1Char('/'), pathSensitivity());
}

bool writeAtomic(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        setError(error, file.errorString());
        return false;
    }
    setError(error, {});
    return true;
}

QString gitMetadataDirectory(const QString &projectRoot, QString *error)
{
    const QFileInfo marker(QDir(projectRoot).filePath(QStringLiteral(".git")));
    QString candidate;
    if (marker.isDir() && !isLink(marker)) {
        candidate = marker.canonicalFilePath();
    } else if (marker.isFile() && !isLink(marker) && marker.size() <= 4096) {
        QFile file(marker.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            setError(error, file.errorString());
            return {};
        }
        const QByteArray line = file.readLine(4097).trimmed();
        if (!line.startsWith("gitdir:")) {
            setError(error, QStringLiteral("Project .git indirection is invalid."));
            return {};
        }
        QString path = QString::fromUtf8(line.mid(7)).trimmed();
        if (!QDir::isAbsolutePath(path))
            path = QDir(projectRoot).absoluteFilePath(path);
        const QFileInfo resolved(path);
        if (!resolved.isDir() || isLink(resolved) || resolved.canonicalFilePath().isEmpty()) {
            setError(error, QStringLiteral("Project Git directory is unsafe."));
            return {};
        }
        candidate = resolved.canonicalFilePath();
        const QFileInfo commonMarker(QDir(candidate).filePath(QStringLiteral("commondir")));
        if (commonMarker.exists()) {
            if (!commonMarker.isFile() || isLink(commonMarker) || commonMarker.size() > 4096) {
                setError(error, QStringLiteral("Project Git common-directory marker is unsafe."));
                return {};
            }
            QFile commonFile(commonMarker.absoluteFilePath());
            if (!commonFile.open(QIODevice::ReadOnly)) {
                setError(error, commonFile.errorString());
                return {};
            }
            QString commonPath = QString::fromUtf8(commonFile.readLine(4097)).trimmed();
            if (!QDir::isAbsolutePath(commonPath))
                commonPath = QDir(candidate).absoluteFilePath(commonPath);
            const QFileInfo common(commonPath);
            if (!common.isDir() || isLink(common) || common.canonicalFilePath().isEmpty()) {
                setError(error, QStringLiteral("Project Git common directory is unsafe."));
                return {};
            }
            candidate = common.canonicalFilePath();
        }
    }
    if (candidate.isEmpty())
        setError(error, QStringLiteral("Project Git metadata could not be resolved."));
    return candidate;
}

bool ensureIgnored(const QString &projectRoot, QString *error)
{
    const QString gitDirectory = gitMetadataDirectory(projectRoot, error);
    if (gitDirectory.isEmpty())
        return false;
    const QString infoPath = QDir(gitDirectory).filePath(QStringLiteral("info"));
    if (!QDir().mkpath(infoPath)) {
        setError(error, QStringLiteral("Could not create the local Git exclude directory."));
        return false;
    }
    const QFileInfo info(infoPath);
    if (!info.isDir() || isLink(info) || info.canonicalFilePath().isEmpty()
        || !contained(gitDirectory, info.canonicalFilePath())) {
        setError(error, QStringLiteral("Local Git exclude directory is unsafe."));
        return false;
    }
    const QString excludePath = QDir(info.canonicalFilePath()).filePath(QStringLiteral("exclude"));
    QByteArray bytes;
    const QFileInfo existing(excludePath);
    if (existing.exists()) {
        if (!existing.isFile() || isLink(existing) || existing.size() > 1024 * 1024) {
            setError(error, QStringLiteral("Local Git exclude file is unsafe or oversized."));
            return false;
        }
        QFile file(excludePath);
        if (!file.open(QIODevice::ReadOnly)) {
            setError(error, file.errorString());
            return false;
        }
        bytes = file.readAll();
    }
    const QList<QByteArray> required{
        QByteArrayLiteral(".wimforge/project-id"),
        QByteArrayLiteral(".wimforge/project-id.lock")};
    bool changed = false;
    for (const QByteArray &pattern : required) {
        const QList<QByteArray> lines = bytes.split('\n');
        if (std::any_of(lines.cbegin(), lines.cend(), [&pattern](const QByteArray &line) {
                return line.trimmed() == pattern;
            })) {
            continue;
        }
        if (!bytes.isEmpty() && !bytes.endsWith('\n'))
            bytes.append('\n');
        bytes.append(pattern);
        bytes.append('\n');
        changed = true;
    }
    return !changed || writeAtomic(excludePath, bytes, error);
}

} // namespace

QString ensureProjectScopeId(const QString &projectDirectory, QString *error)
{
    const QFileInfo root(projectDirectory);
    const QString canonicalRoot = root.canonicalFilePath();
    if (!root.isDir() || isLink(root) || canonicalRoot.isEmpty()) {
        setError(error, QStringLiteral("Project root could not be resolved safely for VM Lab."));
        return {};
    }
    if (!ensureIgnored(canonicalRoot, error))
        return {};
    const QString metadataPath = QDir(canonicalRoot).filePath(QStringLiteral(".wimforge"));
    if (!QDir().mkpath(metadataPath)) {
        setError(error, QStringLiteral("Could not create the project metadata directory."));
        return {};
    }
    const QFileInfo metadata(metadataPath);
    const QString canonicalMetadata = metadata.canonicalFilePath();
    if (!metadata.isDir() || isLink(metadata) || canonicalMetadata.isEmpty()
        || !contained(canonicalRoot, canonicalMetadata)) {
        setError(error, QStringLiteral("Project .wimforge directory is unsafe."));
        return {};
    }
    const QString idPath = QDir(canonicalMetadata).filePath(QStringLiteral("project-id"));
    QLockFile lock(idPath + QStringLiteral(".lock"));
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(10000)) {
        setError(error, QStringLiteral("Project VM scope identity is busy. Try again."));
        return {};
    }
    const QFileInfo existing(idPath);
    if (existing.exists()) {
        if (!existing.isFile() || isLink(existing) || existing.size() > 128) {
            setError(error, QStringLiteral("Project VM scope ID file is unsafe."));
            return {};
        }
        QFile file(idPath);
        if (!file.open(QIODevice::ReadOnly)) {
            setError(error, file.errorString());
            return {};
        }
        const QUuid id(QString::fromLatin1(file.readAll()).trimmed());
        if (id.isNull()) {
            setError(error, QStringLiteral("Project VM scope ID is invalid."));
            return {};
        }
        setError(error, {});
        return id.toString(QUuid::WithoutBraces).toLower();
    }
    const QString value = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
    if (!writeAtomic(idPath, value.toUtf8() + '\n', error))
        return {};
    const QFileInfo written(idPath);
    if (!written.isFile() || isLink(written) || written.canonicalFilePath().isEmpty()
        || !contained(canonicalMetadata, written.canonicalFilePath())) {
        setError(error, QStringLiteral("Project VM scope ID was not created safely."));
        return {};
    }
    setError(error, {});
    return value;
}

QString vmLabApplicationDataRoot()
{
    QString location = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (location.trimmed().isEmpty())
        return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("WimForge"));
    QDir base(location);
    const QString application = QCoreApplication::applicationName();
    if (!application.isEmpty()
        && QFileInfo(base.absolutePath()).fileName().compare(
               application, Qt::CaseInsensitive) == 0)
        base.cdUp();
    const QString organization = QCoreApplication::organizationName();
    if (!organization.isEmpty()
        && QFileInfo(base.absolutePath()).fileName().compare(
               organization, Qt::CaseInsensitive) == 0)
        base.cdUp();
    return base.filePath(QStringLiteral("WimForge/WimForge"));
}

VmLabScope resolveVmLabScope(const QString &projectDirectory, QString *error)
{
    VmLabScope scope;
    const QString dataRoot = vmLabApplicationDataRoot();
    if (projectDirectory.trimmed().isEmpty()) {
        scope.root = QDir(dataRoot).filePath(QStringLiteral("vm-lab/global"));
    } else {
        scope.projectId = ensureProjectScopeId(projectDirectory, error);
        if (scope.projectId.isEmpty())
            return {};
        scope.projectScoped = true;
        scope.root = QDir(dataRoot).filePath(
            QStringLiteral("vm-lab/projects/%1").arg(scope.projectId));
    }
    scope.root = QFileInfo(scope.root).absoluteFilePath();
    scope.catalogPath = QDir(scope.root).filePath(QStringLiteral("catalog.json"));
    scope.managedRoot = QDir(scope.root).filePath(QStringLiteral("machines"));
    setError(error, {});
    return scope;
}

} // namespace wimforge::vmlab
