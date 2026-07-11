#include "VmValidationStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <initializer_list>

namespace wimforge::vmvalidation {

namespace {

constexpr auto StoreSchema = "wimforge.vm-validation-runs";
constexpr auto RelativeStatePath = ".wimforge/vm-validation-runs.json";
constexpr auto RelativeBackupPath = ".wimforge/vm-validation-runs.backup.json";
constexpr auto RelativeLockPath = ".wimforge/vm-validation-runs.lock";

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QString timestamp(const QDateTime &value)
{
    return value.isValid() ? value.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QDateTime parseTimestamp(const QJsonValue &value,
                         const QString &context,
                         bool required,
                         QStringList *errors)
{
    if (value.isUndefined() || value.isNull()
        || (value.isString() && value.toString().isEmpty())) {
        if (required)
            errors->append(QStringLiteral("%1 is required.").arg(context));
        return {};
    }
    if (!value.isString()) {
        errors->append(QStringLiteral("%1 must be an ISO-8601 timestamp.").arg(context));
        return {};
    }
    QDateTime result = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (!result.isValid())
        result = QDateTime::fromString(value.toString(), Qt::ISODate);
    if (!result.isValid()) {
        errors->append(QStringLiteral("%1 must be an ISO-8601 timestamp.").arg(context));
        return {};
    }
    return result.toUTC();
}

qsizetype jsonBytes(const QJsonValue &value)
{
    if (value.isObject())
        return QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact).size();
    if (value.isArray())
        return QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact).size();
    return QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact).size();
}

bool boundedText(const QString &value,
                 qsizetype maximumBytes,
                 const QString &context,
                 QStringList *errors,
                 bool required = false)
{
    if (required && value.trimmed().isEmpty()) {
        errors->append(QStringLiteral("%1 is required.").arg(context));
        return false;
    }
    if (value.toUtf8().size() > maximumBytes) {
        errors->append(QStringLiteral("%1 exceeds the %2-byte limit.")
                           .arg(context)
                           .arg(maximumBytes));
        return false;
    }
    return true;
}

bool hasOnlyKeys(const QJsonObject &object,
                 std::initializer_list<QString> allowed,
                 const QString &context,
                 QStringList *errors)
{
    const QSet<QString> keys(allowed.begin(), allowed.end());
    bool valid = true;
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!keys.contains(iterator.key())) {
            errors->append(QStringLiteral("%1 contains unsupported field '%2'.")
                               .arg(context, iterator.key()));
            valid = false;
        }
    }
    return valid;
}

bool isLowerSha256(const QString &value)
{
    static const QRegularExpression expression(QStringLiteral("^[0-9a-f]{64}$"));
    return expression.match(value).hasMatch();
}

bool isValidStableId(const QString &value)
{
    const QUuid parsed(value);
    return !parsed.isNull()
        && parsed.toString(QUuid::WithoutBraces).toLower() == value.toLower();
}

bool isValidKey(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$"));
    return expression.match(value).hasMatch();
}

QString deterministicUuid(const QByteArray &seed)
{
    QByteArray hex = QCryptographicHash::hash(seed, QCryptographicHash::Sha256).toHex().left(32);
    hex[12] = '5';
    hex[16] = '8';
    return QString::fromLatin1(hex.left(8) + '-' + hex.mid(8, 4) + '-' + hex.mid(12, 4)
                               + '-' + hex.mid(16, 4) + '-' + hex.mid(20, 12));
}

bool portableRelativePath(const QString &path)
{
    if (path.isEmpty() || path.size() > 32'768 || QDir::isAbsolutePath(path)
        || path.contains(QLatin1Char('\\')) || path.contains(QChar::Null)) {
        return false;
    }
    const QString clean = QDir::cleanPath(path);
    if (clean != path || clean == QStringLiteral(".") || clean == QStringLiteral("..")
        || clean.startsWith(QStringLiteral("../")) || clean.contains(QStringLiteral("/../"))
        || clean.startsWith(QLatin1Char('/')) || clean.endsWith(QLatin1Char('/'))) {
        return false;
    }
#ifdef Q_OS_WIN
    if (clean.contains(QLatin1Char(':')))
        return false;
#endif
    return true;
}

Qt::CaseSensitivity pathCaseSensitivity()
{
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

bool sameOrContained(const QString &root, const QString &path)
{
    const QString cleanRoot = QDir::fromNativeSeparators(QDir::cleanPath(root));
    const QString cleanPath = QDir::fromNativeSeparators(QDir::cleanPath(path));
    if (QString::compare(cleanRoot, cleanPath, pathCaseSensitivity()) == 0)
        return true;
    QString prefix = cleanRoot;
    if (!prefix.endsWith(QLatin1Char('/')))
        prefix.append(QLatin1Char('/'));
    return cleanPath.startsWith(prefix, pathCaseSensitivity());
}

QString canonicalProjectDirectory(const QString &projectDirectory, QStringList *errors)
{
    const QFileInfo info(projectDirectory);
    if (!info.isAbsolute() || !info.exists() || !info.isDir()) {
        errors->append(QStringLiteral("Project directory must be an existing absolute directory."));
        return {};
    }
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty()) {
        errors->append(QStringLiteral("Project directory could not be resolved."));
        return {};
    }
    return QDir::fromNativeSeparators(canonical);
}

bool normalizeInputReference(const QString &projectDirectory,
                             const QString &inputPath,
                             const QString &providedSha256,
                             bool requireHash,
                             bool requireExplicitExternal,
                             bool declaredExternal,
                             const QJsonObject &externalMetadata,
                             FileReference *reference,
                             QString *error)
{
    QStringList errors;
    const QString projectRoot = canonicalProjectDirectory(projectDirectory, &errors);
    if (inputPath.trimmed().isEmpty())
        errors.append(QStringLiteral("Referenced file path is required."));

    QString candidate = inputPath.trimmed();
    if (!candidate.isEmpty() && !QDir::isAbsolutePath(candidate))
        candidate = QDir(projectRoot).absoluteFilePath(candidate);
    const QFileInfo file(candidate);
    if (!candidate.isEmpty() && (!file.exists() || !file.isFile()))
        errors.append(QStringLiteral("Referenced file does not exist or is not a regular file: %1")
                          .arg(inputPath));
    const QString canonical = file.canonicalFilePath();
    if (!candidate.isEmpty() && canonical.isEmpty())
        errors.append(QStringLiteral("Referenced file could not be resolved: %1").arg(inputPath));

    QString computedSha;
    if (errors.isEmpty() && requireHash) {
        QString hashError;
        computedSha = VmValidationStore::fileSha256(canonical, &hashError);
        if (!hashError.isEmpty())
            errors.append(hashError);
        const QString expected = providedSha256.trimmed().toLower();
        if (!expected.isEmpty() && (!isLowerSha256(expected) || expected != computedSha)) {
            errors.append(QStringLiteral("Supplied SHA-256 does not match %1.").arg(inputPath));
        }
    } else if (!providedSha256.trimmed().isEmpty()) {
        errors.append(QStringLiteral("A hash is not accepted for this path reference."));
    }

    FileReference normalized;
    if (errors.isEmpty()) {
        const QString normalizedCanonical = QDir::fromNativeSeparators(canonical);
        if (sameOrContained(projectRoot, normalizedCanonical)) {
            normalized.path = QDir::fromNativeSeparators(
                QDir(projectRoot).relativeFilePath(normalizedCanonical));
            if (!portableRelativePath(normalized.path))
                errors.append(QStringLiteral("Project reference is not a safe portable path."));
            normalized.scope = ReferenceScope::Project;
        } else {
            if (requireExplicitExternal
                && (!declaredExternal || externalMetadata.isEmpty())) {
                errors.append(QStringLiteral(
                    "Outside-project evidence requires external=true and non-empty external metadata."));
            }
            if (jsonBytes(externalMetadata) > VmValidationStore::MaxMetadataBytes) {
                errors.append(QStringLiteral("External metadata exceeds the size limit."));
            }
            normalized.path = normalizedCanonical;
            normalized.scope = ReferenceScope::External;
            normalized.externalMetadata = externalMetadata;
        }
        normalized.sha256 = computedSha;
    }

    if (!errors.isEmpty()) {
        setError(error, errors.join(QLatin1Char('\n')));
        return false;
    }
    *reference = std::move(normalized);
    setError(error, {});
    return true;
}

bool validateStoredReference(const FileReference &reference,
                             bool requireHash,
                             bool requireExternalMetadata,
                             const QString &context,
                             QStringList *errors)
{
    bool valid = true;
    if (reference.scope == ReferenceScope::Project) {
        if (!portableRelativePath(reference.path)) {
            errors->append(QStringLiteral("%1 path is not a safe project-relative path.").arg(context));
            valid = false;
        }
        if (!reference.externalMetadata.isEmpty()) {
            errors->append(QStringLiteral("%1 project reference cannot contain external metadata.")
                               .arg(context));
            valid = false;
        }
    } else {
        if (!QDir::isAbsolutePath(reference.path)
            || QDir::fromNativeSeparators(QDir::cleanPath(reference.path)) != reference.path
            || reference.path.size() > 32'768 || reference.path.contains(QChar::Null)) {
            errors->append(QStringLiteral("%1 external path must be absolute and normalized.")
                               .arg(context));
            valid = false;
        }
        if (requireExternalMetadata && reference.externalMetadata.isEmpty()) {
            errors->append(QStringLiteral("%1 external reference needs explicit metadata.")
                               .arg(context));
            valid = false;
        }
        if (jsonBytes(reference.externalMetadata) > VmValidationStore::MaxMetadataBytes) {
            errors->append(QStringLiteral("%1 external metadata exceeds the size limit.")
                               .arg(context));
            valid = false;
        }
    }
    if (requireHash && !isLowerSha256(reference.sha256)) {
        errors->append(QStringLiteral("%1 must contain a lowercase SHA-256.").arg(context));
        valid = false;
    }
    if (!requireHash && !reference.sha256.isEmpty()) {
        errors->append(QStringLiteral("%1 must not contain a SHA-256.").arg(context));
        valid = false;
    }
    return valid;
}

bool parseReference(const QJsonValue &value,
                    const QString &context,
                    FileReference *reference,
                    QStringList *errors)
{
    if (!value.isObject()) {
        errors->append(QStringLiteral("%1 must be an object.").arg(context));
        return false;
    }
    const QJsonObject object = value.toObject();
    hasOnlyKeys(object,
                {QStringLiteral("path"), QStringLiteral("sha256"), QStringLiteral("scope"),
                 QStringLiteral("externalMetadata")},
                context, errors);
    FileReference parsed;
    parsed.path = object.value(QStringLiteral("path")).toString();
    parsed.sha256 = object.value(QStringLiteral("sha256")).toString();
    const QString scope = object.value(QStringLiteral("scope")).toString();
    if (scope == QStringLiteral("project"))
        parsed.scope = ReferenceScope::Project;
    else if (scope == QStringLiteral("external"))
        parsed.scope = ReferenceScope::External;
    else
        errors->append(QStringLiteral("%1.scope is invalid.").arg(context));
    const QJsonValue metadata = object.value(QStringLiteral("externalMetadata"));
    if (!metadata.isUndefined() && !metadata.isObject())
        errors->append(QStringLiteral("%1.externalMetadata must be an object.").arg(context));
    else
        parsed.externalMetadata = metadata.toObject();
    *reference = std::move(parsed);
    return true;
}

std::optional<RunStatus> runStatusFromName(const QString &name)
{
    if (name == QStringLiteral("running"))
        return RunStatus::Running;
    if (name == QStringLiteral("passed"))
        return RunStatus::Passed;
    if (name == QStringLiteral("failed"))
        return RunStatus::Failed;
    if (name == QStringLiteral("cancelled"))
        return RunStatus::Cancelled;
    return std::nullopt;
}

std::optional<MilestonePhase> milestonePhaseFromName(const QString &name)
{
    if (name == QStringLiteral("boot"))
        return MilestonePhase::Boot;
    if (name == QStringLiteral("install"))
        return MilestonePhase::Install;
    return std::nullopt;
}

std::optional<MilestoneStatus> milestoneStatusFromName(const QString &name)
{
    if (name == QStringLiteral("reached"))
        return MilestoneStatus::Reached;
    if (name == QStringLiteral("failed"))
        return MilestoneStatus::Failed;
    if (name == QStringLiteral("skipped"))
        return MilestoneStatus::Skipped;
    return std::nullopt;
}

std::optional<EvidenceKind> evidenceKindFromName(const QString &name)
{
    if (name == QStringLiteral("screenshot"))
        return EvidenceKind::Screenshot;
    if (name == QStringLiteral("log"))
        return EvidenceKind::Log;
    if (name == QStringLiteral("report"))
        return EvidenceKind::Report;
    if (name == QStringLiteral("other"))
        return EvidenceKind::Other;
    return std::nullopt;
}

QJsonObject identityJson(const ValidationRun &run)
{
    return QJsonObject{
        {QStringLiteral("id"), run.id},
        {QStringLiteral("sequence"), run.sequence},
        {QStringLiteral("iso"), run.iso.toJson()},
        {QStringLiteral("image"), run.image.toJson()},
        {QStringLiteral("vm"), run.vm.toJson()},
        {QStringLiteral("configSnapshot"), run.configSnapshot},
        {QStringLiteral("startedAt"), timestamp(run.startedAt)},
    };
}

QString identityHash(const ValidationRun &run)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(identityJson(run)).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex());
}

std::optional<QStringList> requiredMilestonesForProfile(const QString &profile)
{
    if (profile == QStringLiteral("installation")) {
        return QStringList{QStringLiteral("installation-boot"),
                           QStringLiteral("disk-layout"),
                           QStringLiteral("installation-complete")};
    }
    if (profile == QStringLiteral("first-boot")) {
        return QStringList{QStringLiteral("first-boot"),
                           QStringLiteral("drivers"),
                           QStringLiteral("networking"),
                           QStringLiteral("smoke-test")};
    }
    if (profile == QStringLiteral("upgrade")) {
        return QStringList{QStringLiteral("installation-boot"),
                           QStringLiteral("installation-complete"),
                           QStringLiteral("first-boot"),
                           QStringLiteral("drivers"),
                           QStringLiteral("smoke-test")};
    }
    if (profile == QStringLiteral("customization")) {
        return QStringList{QStringLiteral("customizations"),
                           QStringLiteral("first-boot"),
                           QStringLiteral("smoke-test")};
    }
    if (profile == QStringLiteral("full-smoke")) {
        return QStringList{QStringLiteral("installation-boot"),
                           QStringLiteral("disk-layout"),
                           QStringLiteral("installation-complete"),
                           QStringLiteral("first-boot"),
                           QStringLiteral("drivers"),
                           QStringLiteral("networking"),
                           QStringLiteral("customizations"),
                           QStringLiteral("smoke-test")};
    }
    return std::nullopt;
}

QStringList passGateProblems(const ValidationRun &run)
{
    const QString profile = run.configSnapshot.value(QStringLiteral("profile"))
                                .toString().trimmed();
    const std::optional<QStringList> required = requiredMilestonesForProfile(profile);
    const bool milestoneFailed = std::any_of(
        run.milestones.cbegin(), run.milestones.cend(),
        [](const ValidationMilestone &milestone) {
            return milestone.status == MilestoneStatus::Failed;
        });
    const bool hashedEvidence = std::any_of(
        run.evidence.cbegin(), run.evidence.cend(),
        [](const EvidenceReference &evidence) {
            return isLowerSha256(evidence.file.sha256);
        });

    QStringList problems;
    if (!required) {
        problems.append(QStringLiteral(
            "a supported validation profile (installation, first-boot, upgrade, customization, or full-smoke)"));
    } else {
        for (const QString &name : *required) {
            const bool reached = std::any_of(
                run.milestones.cbegin(), run.milestones.cend(),
                [&name](const ValidationMilestone &milestone) {
                    return milestone.name == name
                        && milestone.status == MilestoneStatus::Reached;
                });
            if (!reached) {
                problems.append(QStringLiteral("reached required milestone '%1'")
                                    .arg(name));
            }
        }
    }
    if (milestoneFailed)
        problems.append(QStringLiteral("no failed milestones"));
    if (!hashedEvidence)
        problems.append(QStringLiteral("at least one hashed evidence file"));
    return problems;
}

bool validateRun(const ValidationRun &run, const QString &context, QStringList *errors)
{
    const qsizetype before = errors->size();
    if (!isValidStableId(run.id))
        errors->append(QStringLiteral("%1.id must be a stable UUID.").arg(context));
    if (run.sequence < 1)
        errors->append(QStringLiteral("%1.sequence must be positive.").arg(context));
    if (run.revision < 1)
        errors->append(QStringLiteral("%1.revision must be positive.").arg(context));
    if (!isLowerSha256(run.identityHash) || run.identityHash != identityHash(run))
        errors->append(QStringLiteral("%1 immutable identity hash is invalid.").arg(context));

    validateStoredReference(run.iso, true, false, context + QStringLiteral(".iso"), errors);
    validateStoredReference(run.image, true, false, context + QStringLiteral(".image"), errors);
    validateStoredReference(run.vm.config, true, false,
                            context + QStringLiteral(".vm.config"), errors);
    if (!isValidKey(run.vm.providerId))
        errors->append(QStringLiteral("%1.vm.providerId is invalid.").arg(context));
    boundedText(run.vm.providerVersion, 1'024,
                context + QStringLiteral(".vm.providerVersion"), errors);
    boundedText(run.vm.vmId, 4'096, context + QStringLiteral(".vm.id"), errors, true);
    boundedText(run.vm.vmName, 4'096, context + QStringLiteral(".vm.name"), errors, true);
    if (jsonBytes(run.configSnapshot) > VmValidationStore::MaxConfigBytes)
        errors->append(QStringLiteral("%1.configSnapshot exceeds the size limit.").arg(context));
    if (!run.startedAt.isValid() || !run.updatedAt.isValid()
        || run.updatedAt < run.startedAt) {
        errors->append(QStringLiteral("%1 timestamps are incomplete or out of order.").arg(context));
    }

    if (run.completed()) {
        if (!run.endedAt.isValid() || run.endedAt < run.startedAt
            || run.updatedAt != run.endedAt) {
            errors->append(QStringLiteral("%1 completed timestamps are invalid.").arg(context));
        }
        if ((run.status == RunStatus::Failed || run.status == RunStatus::Cancelled)
            && run.completionNote.trimmed().isEmpty()) {
            errors->append(QStringLiteral("%1 failed/cancelled run requires a completion note.")
                               .arg(context));
        }
    } else if (run.endedAt.isValid() || !run.completionNote.isEmpty()) {
        errors->append(QStringLiteral("%1 running run cannot have completion fields.").arg(context));
    }
    if (run.status == RunStatus::Passed) {
        const QStringList problems = passGateProblems(run);
        if (!problems.isEmpty()) {
            errors->append(QStringLiteral("%1 passed validation requires %2.")
                               .arg(context, problems.join(QStringLiteral(", "))));
        }
    }
    boundedText(run.completionNote, VmValidationStore::MaxNoteBytes,
                context + QStringLiteral(".completionNote"), errors);

    if (run.milestones.size() > VmValidationStore::MaxMilestonesPerRun)
        errors->append(QStringLiteral("%1 has too many milestones.").arg(context));
    if (run.logs.size() > VmValidationStore::MaxLogEntriesPerRun)
        errors->append(QStringLiteral("%1 has too many log entries.").arg(context));
    if (run.evidence.size() > VmValidationStore::MaxEvidencePerRun)
        errors->append(QStringLiteral("%1 has too many evidence references.").arg(context));

    QSet<QString> milestoneIds;
    for (qsizetype index = 0; index < run.milestones.size(); ++index) {
        const ValidationMilestone &item = run.milestones.at(index);
        const QString itemContext = QStringLiteral("%1.milestones[%2]").arg(context).arg(index);
        if (!isValidStableId(item.id) || milestoneIds.contains(item.id))
            errors->append(QStringLiteral("%1 has an invalid or duplicate id.").arg(itemContext));
        milestoneIds.insert(item.id);
        boundedText(item.name, 1'024, itemContext + QStringLiteral(".name"), errors, true);
        boundedText(item.note, VmValidationStore::MaxNoteBytes,
                    itemContext + QStringLiteral(".note"), errors);
        if (jsonBytes(item.data) > VmValidationStore::MaxMetadataBytes)
            errors->append(QStringLiteral("%1.data exceeds the size limit.").arg(itemContext));
        if (!item.occurredAt.isValid() || item.occurredAt < run.startedAt
            || (run.completed() && item.occurredAt > run.endedAt)) {
            errors->append(QStringLiteral("%1 occurredAt is outside the run interval.")
                               .arg(itemContext));
        }
    }

    qsizetype totalLogBytes = 0;
    for (qsizetype index = 0; index < run.logs.size(); ++index) {
        const ValidationLogEntry &item = run.logs.at(index);
        const QString itemContext = QStringLiteral("%1.logs[%2]").arg(context).arg(index);
        if (!isValidKey(item.channel))
            errors->append(QStringLiteral("%1.channel is invalid.").arg(itemContext));
        const qsizetype bytes = item.message.toUtf8().size();
        totalLogBytes += bytes;
        if (bytes > VmValidationStore::MaxLogEntryBytes)
            errors->append(QStringLiteral("%1.message exceeds the per-entry size limit.")
                               .arg(itemContext));
        if (!item.occurredAt.isValid() || item.occurredAt < run.startedAt
            || (run.completed() && item.occurredAt > run.endedAt)) {
            errors->append(QStringLiteral("%1.occurredAt is outside the run interval.")
                               .arg(itemContext));
        }
    }
    if (totalLogBytes > VmValidationStore::MaxLogBytesPerRun)
        errors->append(QStringLiteral("%1 log payload exceeds the per-run size limit.").arg(context));

    QSet<QString> evidenceIds;
    for (qsizetype index = 0; index < run.evidence.size(); ++index) {
        const EvidenceReference &item = run.evidence.at(index);
        const QString itemContext = QStringLiteral("%1.evidence[%2]").arg(context).arg(index);
        if (!isValidStableId(item.id) || evidenceIds.contains(item.id))
            errors->append(QStringLiteral("%1 has an invalid or duplicate id.").arg(itemContext));
        evidenceIds.insert(item.id);
        boundedText(item.label, 1'024, itemContext + QStringLiteral(".label"), errors, true);
        validateStoredReference(item.file, true, true,
                                itemContext + QStringLiteral(".file"), errors);
        if (!item.capturedAt.isValid() || item.capturedAt < run.startedAt
            || (run.completed() && item.capturedAt > run.endedAt)) {
            errors->append(QStringLiteral("%1.capturedAt is outside the run interval.")
                               .arg(itemContext));
        }
    }
    return errors->size() == before;
}

bool parseVm(const QJsonValue &value, VmIdentity *vm, QStringList *errors)
{
    if (!value.isObject()) {
        errors->append(QStringLiteral("run.vm must be an object."));
        return false;
    }
    const QJsonObject object = value.toObject();
    hasOnlyKeys(object,
                {QStringLiteral("providerId"), QStringLiteral("providerVersion"),
                 QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("config")},
                QStringLiteral("run.vm"), errors);
    VmIdentity parsed;
    parsed.providerId = object.value(QStringLiteral("providerId")).toString();
    parsed.providerVersion = object.value(QStringLiteral("providerVersion")).toString();
    parsed.vmId = object.value(QStringLiteral("id")).toString();
    parsed.vmName = object.value(QStringLiteral("name")).toString();
    parseReference(object.value(QStringLiteral("config")), QStringLiteral("run.vm.config"),
                   &parsed.config, errors);
    *vm = std::move(parsed);
    return true;
}

bool parseMilestone(const QJsonValue &value,
                    qsizetype index,
                    ValidationMilestone *milestone,
                    QStringList *errors)
{
    const QString context = QStringLiteral("run.milestones[%1]").arg(index);
    if (!value.isObject()) {
        errors->append(QStringLiteral("%1 must be an object.").arg(context));
        return false;
    }
    const QJsonObject object = value.toObject();
    hasOnlyKeys(object,
                {QStringLiteral("id"), QStringLiteral("phase"), QStringLiteral("name"),
                 QStringLiteral("status"), QStringLiteral("occurredAt"), QStringLiteral("note"),
                 QStringLiteral("data")},
                context, errors);
    ValidationMilestone parsed;
    parsed.id = object.value(QStringLiteral("id")).toString();
    const auto phase = milestonePhaseFromName(object.value(QStringLiteral("phase")).toString());
    const auto status = milestoneStatusFromName(object.value(QStringLiteral("status")).toString());
    if (!phase)
        errors->append(QStringLiteral("%1.phase is invalid.").arg(context));
    else
        parsed.phase = *phase;
    if (!status)
        errors->append(QStringLiteral("%1.status is invalid.").arg(context));
    else
        parsed.status = *status;
    parsed.name = object.value(QStringLiteral("name")).toString();
    parsed.occurredAt = parseTimestamp(object.value(QStringLiteral("occurredAt")),
                                       context + QStringLiteral(".occurredAt"), true, errors);
    parsed.note = object.value(QStringLiteral("note")).toString();
    const QJsonValue data = object.value(QStringLiteral("data"));
    if (!data.isUndefined() && !data.isObject())
        errors->append(QStringLiteral("%1.data must be an object.").arg(context));
    else
        parsed.data = data.toObject();
    *milestone = std::move(parsed);
    return true;
}

bool parseLog(const QJsonValue &value,
              qsizetype index,
              ValidationLogEntry *entry,
              QStringList *errors)
{
    const QString context = QStringLiteral("run.logs[%1]").arg(index);
    if (!value.isObject()) {
        errors->append(QStringLiteral("%1 must be an object.").arg(context));
        return false;
    }
    const QJsonObject object = value.toObject();
    hasOnlyKeys(object,
                {QStringLiteral("occurredAt"), QStringLiteral("channel"), QStringLiteral("message")},
                context, errors);
    ValidationLogEntry parsed;
    parsed.occurredAt = parseTimestamp(object.value(QStringLiteral("occurredAt")),
                                       context + QStringLiteral(".occurredAt"), true, errors);
    parsed.channel = object.value(QStringLiteral("channel")).toString();
    parsed.message = object.value(QStringLiteral("message")).toString();
    *entry = std::move(parsed);
    return true;
}

bool parseEvidence(const QJsonValue &value,
                   qsizetype index,
                   EvidenceReference *evidence,
                   QStringList *errors)
{
    const QString context = QStringLiteral("run.evidence[%1]").arg(index);
    if (!value.isObject()) {
        errors->append(QStringLiteral("%1 must be an object.").arg(context));
        return false;
    }
    const QJsonObject object = value.toObject();
    hasOnlyKeys(object,
                {QStringLiteral("id"), QStringLiteral("kind"), QStringLiteral("label"),
                 QStringLiteral("file"), QStringLiteral("capturedAt")},
                context, errors);
    EvidenceReference parsed;
    parsed.id = object.value(QStringLiteral("id")).toString();
    const auto kind = evidenceKindFromName(object.value(QStringLiteral("kind")).toString());
    if (!kind)
        errors->append(QStringLiteral("%1.kind is invalid.").arg(context));
    else
        parsed.kind = *kind;
    parsed.label = object.value(QStringLiteral("label")).toString();
    parseReference(object.value(QStringLiteral("file")), context + QStringLiteral(".file"),
                   &parsed.file, errors);
    parsed.capturedAt = parseTimestamp(object.value(QStringLiteral("capturedAt")),
                                       context + QStringLiteral(".capturedAt"), true, errors);
    *evidence = std::move(parsed);
    return true;
}

bool parseCurrentRun(const QJsonObject &object,
                     qsizetype index,
                     ValidationRun *run,
                     QStringList *errors)
{
    const QString context = QStringLiteral("runs[%1]").arg(index);
    hasOnlyKeys(object,
                {QStringLiteral("id"), QStringLiteral("sequence"), QStringLiteral("revision"),
                 QStringLiteral("identityHash"), QStringLiteral("iso"), QStringLiteral("image"),
                 QStringLiteral("vm"), QStringLiteral("configSnapshot"),
                 QStringLiteral("startedAt"), QStringLiteral("updatedAt"),
                 QStringLiteral("endedAt"), QStringLiteral("status"),
                 QStringLiteral("milestones"), QStringLiteral("logs"),
                 QStringLiteral("evidence"), QStringLiteral("completionNote")},
                context, errors);
    ValidationRun parsed;
    parsed.id = object.value(QStringLiteral("id")).toString();
    parsed.sequence = object.value(QStringLiteral("sequence")).toInteger();
    parsed.revision = object.value(QStringLiteral("revision")).toInt();
    parsed.identityHash = object.value(QStringLiteral("identityHash")).toString();
    parseReference(object.value(QStringLiteral("iso")), context + QStringLiteral(".iso"),
                   &parsed.iso, errors);
    parseReference(object.value(QStringLiteral("image")), context + QStringLiteral(".image"),
                   &parsed.image, errors);
    parseVm(object.value(QStringLiteral("vm")), &parsed.vm, errors);
    const QJsonValue config = object.value(QStringLiteral("configSnapshot"));
    if (!config.isObject())
        errors->append(QStringLiteral("%1.configSnapshot must be an object.").arg(context));
    else
        parsed.configSnapshot = config.toObject();
    parsed.startedAt = parseTimestamp(object.value(QStringLiteral("startedAt")),
                                      context + QStringLiteral(".startedAt"), true, errors);
    parsed.updatedAt = parseTimestamp(object.value(QStringLiteral("updatedAt")),
                                      context + QStringLiteral(".updatedAt"), true, errors);
    parsed.endedAt = parseTimestamp(object.value(QStringLiteral("endedAt")),
                                    context + QStringLiteral(".endedAt"), false, errors);
    const auto status = runStatusFromName(object.value(QStringLiteral("status")).toString());
    if (!status)
        errors->append(QStringLiteral("%1.status is invalid.").arg(context));
    else
        parsed.status = *status;
    const QJsonValue milestones = object.value(QStringLiteral("milestones"));
    if (!milestones.isArray())
        errors->append(QStringLiteral("%1.milestones must be an array.").arg(context));
    else {
        const QJsonArray array = milestones.toArray();
        for (qsizetype itemIndex = 0; itemIndex < array.size(); ++itemIndex) {
            ValidationMilestone item;
            parseMilestone(array.at(itemIndex), itemIndex, &item, errors);
            parsed.milestones.append(std::move(item));
        }
    }
    const QJsonValue logs = object.value(QStringLiteral("logs"));
    if (!logs.isArray())
        errors->append(QStringLiteral("%1.logs must be an array.").arg(context));
    else {
        const QJsonArray array = logs.toArray();
        for (qsizetype itemIndex = 0; itemIndex < array.size(); ++itemIndex) {
            ValidationLogEntry item;
            parseLog(array.at(itemIndex), itemIndex, &item, errors);
            parsed.logs.append(std::move(item));
        }
    }
    const QJsonValue evidence = object.value(QStringLiteral("evidence"));
    if (!evidence.isArray())
        errors->append(QStringLiteral("%1.evidence must be an array.").arg(context));
    else {
        const QJsonArray array = evidence.toArray();
        for (qsizetype itemIndex = 0; itemIndex < array.size(); ++itemIndex) {
            EvidenceReference item;
            parseEvidence(array.at(itemIndex), itemIndex, &item, errors);
            parsed.evidence.append(std::move(item));
        }
    }
    parsed.completionNote = object.value(QStringLiteral("completionNote")).toString();
    validateRun(parsed, context, errors);
    *run = std::move(parsed);
    return true;
}

FileReference legacyReference(const QString &projectDirectory,
                              const QString &path,
                              const QString &sha256,
                              bool requireHash,
                              bool evidence,
                              bool external,
                              const QJsonObject &externalMetadata,
                              const QString &context,
                              QStringList *errors)
{
    FileReference result;
    result.sha256 = sha256.trimmed().toLower();
    const QString normalized = QDir::fromNativeSeparators(QDir::cleanPath(path));
    if (QDir::isAbsolutePath(normalized)) {
        const QString root = QDir::fromNativeSeparators(QDir::cleanPath(projectDirectory));
        if (sameOrContained(root, normalized)) {
            result.scope = ReferenceScope::Project;
            result.path = QDir::fromNativeSeparators(QDir(root).relativeFilePath(normalized));
        } else {
            result.scope = ReferenceScope::External;
            result.path = normalized;
            result.externalMetadata = externalMetadata;
            if (evidence && (!external || externalMetadata.isEmpty())) {
                errors->append(QStringLiteral(
                    "%1 outside-project evidence lacks explicit external metadata.").arg(context));
            }
        }
    } else {
        result.scope = ReferenceScope::Project;
        result.path = QDir::fromNativeSeparators(path);
    }
    validateStoredReference(result, requireHash, evidence, context, errors);
    return result;
}

bool migrateLegacyRun(const QJsonObject &object,
                      qsizetype index,
                      const QString &projectDirectory,
                      ValidationRun *run,
                      QStringList *errors)
{
    const QString context = QStringLiteral("legacy runs[%1]").arg(index);
    hasOnlyKeys(object,
                {QStringLiteral("id"), QStringLiteral("sequence"), QStringLiteral("revision"),
                 QStringLiteral("isoPath"), QStringLiteral("isoSha256"),
                 QStringLiteral("imagePath"), QStringLiteral("imageSha256"),
                 QStringLiteral("providerId"), QStringLiteral("providerVersion"),
                 QStringLiteral("vmId"), QStringLiteral("vmName"),
                 QStringLiteral("vmConfigPath"), QStringLiteral("configSnapshot"),
                 QStringLiteral("startedAt"), QStringLiteral("updatedAt"),
                 QStringLiteral("endedAt"), QStringLiteral("status"),
                 QStringLiteral("milestones"), QStringLiteral("logs"),
                 QStringLiteral("evidence"), QStringLiteral("completionNote"),
                 QStringLiteral("note")},
                context, errors);
    ValidationRun parsed;
    parsed.id = object.value(QStringLiteral("id")).toString();
    if (parsed.id.isEmpty()) {
        parsed.id = deterministicUuid(
            QJsonDocument(object).toJson(QJsonDocument::Compact)
            + QByteArray::number(index));
    }
    parsed.sequence = object.value(QStringLiteral("sequence")).toInteger(index + 1);
    parsed.revision = std::max(1, object.value(QStringLiteral("revision")).toInt(1));
    parsed.iso = legacyReference(projectDirectory,
                                 object.value(QStringLiteral("isoPath")).toString(),
                                 object.value(QStringLiteral("isoSha256")).toString(), true,
                                 false, false, {}, context + QStringLiteral(".iso"), errors);
    parsed.image = legacyReference(projectDirectory,
                                   object.value(QStringLiteral("imagePath")).toString(),
                                   object.value(QStringLiteral("imageSha256")).toString(), true,
                                   false, false, {}, context + QStringLiteral(".image"), errors);
    parsed.vm.providerId = object.value(QStringLiteral("providerId")).toString();
    parsed.vm.providerVersion = object.value(QStringLiteral("providerVersion")).toString();
    parsed.vm.vmId = object.value(QStringLiteral("vmId")).toString();
    parsed.vm.vmName = object.value(QStringLiteral("vmName")).toString();
    parsed.vm.config = legacyReference(projectDirectory,
                                       object.value(QStringLiteral("vmConfigPath")).toString(), {},
                                       false, false, false, {},
                                       context + QStringLiteral(".vm.config"), errors);
    const QString legacyConfigPath = parsed.vm.config.resolvedPath(projectDirectory);
    QString configHashError;
    parsed.vm.config.sha256 = VmValidationStore::fileSha256(legacyConfigPath, &configHashError);
    if (!configHashError.isEmpty()) {
        errors->append(QStringLiteral("%1.vm.config could not be hash-bound during migration: %2")
                           .arg(context, configHashError));
    }
    validateStoredReference(parsed.vm.config, true, false,
                            context + QStringLiteral(".vm.config"), errors);
    const QJsonValue config = object.value(QStringLiteral("configSnapshot"));
    if (!config.isObject())
        errors->append(QStringLiteral("%1.configSnapshot must be an object.").arg(context));
    else
        parsed.configSnapshot = config.toObject();
    parsed.startedAt = parseTimestamp(object.value(QStringLiteral("startedAt")),
                                      context + QStringLiteral(".startedAt"), true, errors);
    parsed.endedAt = parseTimestamp(object.value(QStringLiteral("endedAt")),
                                    context + QStringLiteral(".endedAt"), false, errors);
    const auto status = runStatusFromName(object.value(QStringLiteral("status")).toString());
    if (!status)
        errors->append(QStringLiteral("%1.status is invalid.").arg(context));
    else
        parsed.status = *status;

    const QJsonArray milestones = object.value(QStringLiteral("milestones")).toArray();
    for (qsizetype itemIndex = 0; itemIndex < milestones.size(); ++itemIndex) {
        const QJsonObject itemObject = milestones.at(itemIndex).toObject();
        ValidationMilestone item;
        item.id = itemObject.value(QStringLiteral("id")).toString();
        if (item.id.isEmpty())
            item.id = deterministicUuid(parsed.id.toUtf8() + ":milestone:"
                                        + QByteArray::number(itemIndex));
        const auto phase = milestonePhaseFromName(itemObject.value(QStringLiteral("phase")).toString());
        const auto itemStatus = milestoneStatusFromName(
            itemObject.value(QStringLiteral("status")).toString());
        if (!phase || !itemStatus) {
            errors->append(QStringLiteral("%1.milestones[%2] has an invalid phase or status.")
                               .arg(context)
                               .arg(itemIndex));
        } else {
            item.phase = *phase;
            item.status = *itemStatus;
        }
        item.name = itemObject.value(QStringLiteral("name")).toString();
        item.occurredAt = parseTimestamp(itemObject.value(QStringLiteral("occurredAt")),
                                         context + QStringLiteral(".milestone.occurredAt"),
                                         true, errors);
        item.note = itemObject.value(QStringLiteral("note")).toString();
        item.data = itemObject.value(QStringLiteral("data")).toObject();
        parsed.milestones.append(std::move(item));
    }
    const QJsonArray logs = object.value(QStringLiteral("logs")).toArray();
    for (qsizetype itemIndex = 0; itemIndex < logs.size(); ++itemIndex) {
        const QJsonObject itemObject = logs.at(itemIndex).toObject();
        ValidationLogEntry item;
        item.occurredAt = parseTimestamp(itemObject.value(QStringLiteral("occurredAt")),
                                         context + QStringLiteral(".log.occurredAt"), true, errors);
        item.channel = itemObject.value(QStringLiteral("channel")).toString();
        item.message = itemObject.value(QStringLiteral("message")).toString();
        parsed.logs.append(std::move(item));
    }
    const QJsonArray evidence = object.value(QStringLiteral("evidence")).toArray();
    for (qsizetype itemIndex = 0; itemIndex < evidence.size(); ++itemIndex) {
        const QJsonObject itemObject = evidence.at(itemIndex).toObject();
        EvidenceReference item;
        item.id = itemObject.value(QStringLiteral("id")).toString();
        if (item.id.isEmpty())
            item.id = deterministicUuid(parsed.id.toUtf8() + ":evidence:"
                                        + QByteArray::number(itemIndex));
        const auto kind = evidenceKindFromName(itemObject.value(QStringLiteral("kind")).toString());
        if (!kind)
            errors->append(QStringLiteral("%1.evidence[%2].kind is invalid.")
                               .arg(context)
                               .arg(itemIndex));
        else
            item.kind = *kind;
        item.label = itemObject.value(QStringLiteral("label")).toString();
        item.file = legacyReference(
            projectDirectory,
            itemObject.value(QStringLiteral("path")).toString(),
            itemObject.value(QStringLiteral("sha256")).toString(), true, true,
            itemObject.value(QStringLiteral("external")).toBool(),
            itemObject.value(QStringLiteral("externalMetadata")).toObject(),
            context + QStringLiteral(".evidence.file"), errors);
        item.capturedAt = parseTimestamp(itemObject.value(QStringLiteral("capturedAt")),
                                         context + QStringLiteral(".evidence.capturedAt"), true,
                                         errors);
        parsed.evidence.append(std::move(item));
    }
    parsed.completionNote = object.value(QStringLiteral("completionNote")).toString();
    if (parsed.completionNote.isEmpty())
        parsed.completionNote = object.value(QStringLiteral("note")).toString();
    parsed.updatedAt = parseTimestamp(object.value(QStringLiteral("updatedAt")),
                                      context + QStringLiteral(".updatedAt"), false, errors);
    if (!parsed.updatedAt.isValid())
        parsed.updatedAt = parsed.completed() ? parsed.endedAt : parsed.startedAt;
    parsed.identityHash = identityHash(parsed);
    validateRun(parsed, context, errors);
    *run = std::move(parsed);
    return true;
}

QJsonObject rootWithoutRevision(const StoreSnapshot &snapshot)
{
    QJsonArray runs;
    for (const ValidationRun &run : snapshot.runs)
        runs.append(run.toJson());
    return QJsonObject{
        {QStringLiteral("schema"), QString::fromLatin1(StoreSchema)},
        {QStringLiteral("version"), VmValidationStore::CurrentVersion},
        {QStringLiteral("generation"), snapshot.generation},
        {QStringLiteral("runs"), runs},
    };
}

QString snapshotRevision(const StoreSnapshot &snapshot)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(rootWithoutRevision(snapshot)).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex());
}

StoreSnapshot emptySnapshot()
{
    StoreSnapshot result;
    result.revision = snapshotRevision(result);
    return result;
}

bool parseSnapshot(const QByteArray &bytes,
                   const QString &projectDirectory,
                   StoreSnapshot *snapshot,
                   QString *error)
{
    if (bytes.size() > VmValidationStore::MaxStoreBytes) {
        setError(error, QStringLiteral("VM validation store exceeds the size limit."));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("VM validation store JSON is invalid at offset %1: %2")
                            .arg(parseError.offset)
                            .arg(parseError.errorString()));
        return false;
    }
    const QJsonObject root = document.object();
    QStringList errors;
    if (!root.value(QStringLiteral("schema")).isString()
        || root.value(QStringLiteral("schema")).toString() != QString::fromLatin1(StoreSchema)) {
        errors.append(QStringLiteral("VM validation store schema is unsupported."));
    }
    if (!root.value(QStringLiteral("version")).isDouble())
        errors.append(QStringLiteral("VM validation store version must be an integer."));
    const int version = root.value(QStringLiteral("version")).toInt(-1);
    if (version != VmValidationStore::CurrentVersion
        && version != VmValidationStore::LegacyVersion) {
        errors.append(QStringLiteral("VM validation store version is unsupported."));
    }
    if (version == VmValidationStore::CurrentVersion) {
        hasOnlyKeys(root,
                    {QStringLiteral("schema"), QStringLiteral("version"),
                     QStringLiteral("generation"), QStringLiteral("revision"),
                     QStringLiteral("runs")},
                    QStringLiteral("store"), &errors);
    } else if (version == VmValidationStore::LegacyVersion) {
        hasOnlyKeys(root,
                    {QStringLiteral("schema"), QStringLiteral("version"),
                     QStringLiteral("generation"), QStringLiteral("runs")},
                    QStringLiteral("legacy store"), &errors);
    }
    if (!root.value(QStringLiteral("generation")).isDouble())
        errors.append(QStringLiteral("VM validation store generation must be an integer."));
    if (!root.value(QStringLiteral("runs")).isArray())
        errors.append(QStringLiteral("VM validation store runs must be an array."));

    StoreSnapshot parsed;
    parsed.generation = root.value(QStringLiteral("generation")).toInteger();
    if (parsed.generation < 0)
        errors.append(QStringLiteral("VM validation store generation cannot be negative."));
    const QJsonArray array = root.value(QStringLiteral("runs")).toArray();
    if (array.size() > VmValidationStore::MaxRuns)
        errors.append(QStringLiteral("VM validation store has too many runs."));
    QSet<QString> ids;
    for (qsizetype index = 0; index < array.size(); ++index) {
        if (!array.at(index).isObject()) {
            errors.append(QStringLiteral("runs[%1] must be an object.").arg(index));
            continue;
        }
        ValidationRun run;
        if (version == VmValidationStore::CurrentVersion)
            parseCurrentRun(array.at(index).toObject(), index, &run, &errors);
        else
            migrateLegacyRun(array.at(index).toObject(), index, projectDirectory, &run, &errors);
        if (run.sequence != index + 1)
            errors.append(QStringLiteral("runs[%1] has a non-contiguous sequence.").arg(index));
        if (ids.contains(run.id))
            errors.append(QStringLiteral("runs[%1] duplicates a run id.").arg(index));
        ids.insert(run.id);
        parsed.runs.append(std::move(run));
    }
    if (version == VmValidationStore::LegacyVersion) {
        parsed.migratedFromVersion = VmValidationStore::LegacyVersion;
        if (!root.contains(QStringLiteral("generation")))
            parsed.generation = parsed.runs.size();
        parsed.revision = snapshotRevision(parsed);
    } else {
        if (!root.value(QStringLiteral("revision")).isString())
            errors.append(QStringLiteral("VM validation store revision must be a string."));
        parsed.revision = root.value(QStringLiteral("revision")).toString();
        const QString expectedRevision = snapshotRevision(parsed);
        if (!isLowerSha256(parsed.revision) || parsed.revision != expectedRevision)
            errors.append(QStringLiteral("VM validation store revision integrity check failed."));
    }

    if (!errors.isEmpty()) {
        setError(error, errors.join(QLatin1Char('\n')));
        return false;
    }
    *snapshot = std::move(parsed);
    setError(error, {});
    return true;
}

bool readBytes(const QString &path, QByteArray *bytes, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Could not read %1: %2").arg(path, file.errorString()));
        return false;
    }
    *bytes = file.read(VmValidationStore::MaxStoreBytes + 1);
    if (bytes->size() > VmValidationStore::MaxStoreBytes) {
        setError(error, QStringLiteral("%1 exceeds the VM validation store size limit.").arg(path));
        return false;
    }
    setError(error, {});
    return true;
}

bool atomicWrite(const QString &path, const QByteArray &bytes, QString *error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        setError(error, QStringLiteral("Could not create VM validation store directory."));
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Could not open %1: %2").arg(path, file.errorString()));
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        const QString writeError = file.errorString();
        file.cancelWriting();
        setError(error, QStringLiteral("Could not write %1: %2").arg(path, writeError));
        return false;
    }
    if (!file.commit()) {
        setError(error, QStringLiteral("Could not atomically replace %1: %2")
                            .arg(path, file.errorString()));
        return false;
    }
    setError(error, {});
    return true;
}

bool loadPrimary(const QString &projectDirectory,
                 const QString &statePath,
                 const QString &backupPath,
                 StoreSnapshot *snapshot,
                 QString *error,
    bool mentionRecovery)
{
    if (!QFileInfo::exists(statePath)) {
        if (mentionRecovery && QFileInfo::exists(backupPath)) {
            QByteArray backupBytes;
            StoreSnapshot backup;
            QString backupError;
            if (readBytes(backupPath, &backupBytes, &backupError)
                && parseSnapshot(backupBytes, projectDirectory, &backup, &backupError)) {
                setError(error, QStringLiteral(
                    "VM validation primary is missing while a valid atomic backup exists. "
                    "Call recoverFromBackup() to restore it explicitly."));
                return false;
            }
            setError(error, QStringLiteral(
                "VM validation primary is missing and its backup is invalid: %1")
                                .arg(backupError));
            return false;
        }
        *snapshot = emptySnapshot();
        setError(error, {});
        return true;
    }
    QByteArray bytes;
    QString loadError;
    if (readBytes(statePath, &bytes, &loadError)
        && parseSnapshot(bytes, projectDirectory, snapshot, &loadError)) {
        setError(error, {});
        return true;
    }
    if (mentionRecovery && QFileInfo::exists(backupPath)) {
        QByteArray backupBytes;
        StoreSnapshot backup;
        QString backupError;
        if (readBytes(backupPath, &backupBytes, &backupError)
            && parseSnapshot(backupBytes, projectDirectory, &backup, &backupError)) {
            loadError += QStringLiteral(
                "\nA valid atomic backup is available. Call recoverFromBackup() to preserve "
                "the corrupt primary and restore it explicitly.");
        }
    }
    setError(error, loadError);
    return false;
}

bool writeSnapshot(const QString &statePath,
                   const QString &backupPath,
                   StoreSnapshot *snapshot,
                   QString *error)
{
    snapshot->migratedFromVersion = 0;
    snapshot->revision = snapshotRevision(*snapshot);
    const QByteArray bytes = QJsonDocument(snapshot->toJson()).toJson(QJsonDocument::Indented);
    if (bytes.size() > VmValidationStore::MaxStoreBytes) {
        setError(error, QStringLiteral("VM validation store update exceeds the size limit."));
        return false;
    }

    const bool hadBackup = QFileInfo::exists(backupPath);
    QByteArray priorBackup;
    if (hadBackup && !readBytes(backupPath, &priorBackup, error))
        return false;
    if (!atomicWrite(backupPath, bytes, error))
        return false;

    QString primaryError;
    if (atomicWrite(statePath, bytes, &primaryError)) {
        setError(error, {});
        return true;
    }

    QString rollbackError;
    bool backupRolledBack = true;
    if (hadBackup)
        backupRolledBack = atomicWrite(backupPath, priorBackup, &rollbackError);
    else if (QFileInfo::exists(backupPath))
        backupRolledBack = QFile::remove(backupPath);
    if (!backupRolledBack) {
        primaryError += QStringLiteral(" Backup rollback also failed: %1").arg(rollbackError);
    }
    setError(error, primaryError);
    return false;
}

bool prepareMutation(const VmValidationStore &store,
                     const QString &expectedStoreRevision,
                     QLockFile *lock,
                     StoreSnapshot *snapshot,
                     QString *error)
{
    QStringList projectErrors;
    canonicalProjectDirectory(store.projectDirectory(), &projectErrors);
    if (!projectErrors.isEmpty()) {
        setError(error, projectErrors.join(QLatin1Char('\n')));
        return false;
    }
    if (!QDir().mkpath(QFileInfo(store.stateFilePath()).absolutePath())) {
        setError(error, QStringLiteral("Could not create the VM validation store folder."));
        return false;
    }
    lock->setStaleLockTime(30'000);
    if (!lock->tryLock(10'000)) {
        setError(error, QStringLiteral("VM validation history is busy. Try again in a moment."));
        return false;
    }
    if (!loadPrimary(store.projectDirectory(), store.stateFilePath(), store.backupFilePath(),
                     snapshot, error, true)) {
        return false;
    }
    if (!expectedStoreRevision.isEmpty()
        && expectedStoreRevision != snapshot->revision) {
        setError(error, QStringLiteral(
            "VM validation history changed on disk; reload and retry the reviewed update."));
        return false;
    }
    return true;
}

ValidationRun *findMutable(QList<ValidationRun> *runs, const QString &id)
{
    const auto found = std::find_if(runs->begin(), runs->end(), [&id](const ValidationRun &run) {
        return run.id == id;
    });
    return found == runs->end() ? nullptr : &*found;
}

QDateTime later(const QDateTime &left, const QDateTime &right)
{
    if (!left.isValid())
        return right;
    if (!right.isValid())
        return left;
    return left < right ? right : left;
}

} // namespace

QString runStatusName(RunStatus status)
{
    switch (status) {
    case RunStatus::Running:
        return QStringLiteral("running");
    case RunStatus::Passed:
        return QStringLiteral("passed");
    case RunStatus::Failed:
        return QStringLiteral("failed");
    case RunStatus::Cancelled:
        return QStringLiteral("cancelled");
    }
    return QStringLiteral("running");
}

QString milestonePhaseName(MilestonePhase phase)
{
    return phase == MilestonePhase::Boot ? QStringLiteral("boot") : QStringLiteral("install");
}

QString milestoneStatusName(MilestoneStatus status)
{
    switch (status) {
    case MilestoneStatus::Reached:
        return QStringLiteral("reached");
    case MilestoneStatus::Failed:
        return QStringLiteral("failed");
    case MilestoneStatus::Skipped:
        return QStringLiteral("skipped");
    }
    return QStringLiteral("reached");
}

QString evidenceKindName(EvidenceKind kind)
{
    switch (kind) {
    case EvidenceKind::Screenshot:
        return QStringLiteral("screenshot");
    case EvidenceKind::Log:
        return QStringLiteral("log");
    case EvidenceKind::Report:
        return QStringLiteral("report");
    case EvidenceKind::Other:
        return QStringLiteral("other");
    }
    return QStringLiteral("other");
}

QString referenceScopeName(ReferenceScope scope)
{
    return scope == ReferenceScope::Project ? QStringLiteral("project")
                                             : QStringLiteral("external");
}

bool isCompleted(RunStatus status)
{
    return status != RunStatus::Running;
}

QJsonObject FileReference::toJson() const
{
    return QJsonObject{
        {QStringLiteral("path"), path},
        {QStringLiteral("sha256"), sha256},
        {QStringLiteral("scope"), referenceScopeName(scope)},
        {QStringLiteral("externalMetadata"), externalMetadata},
    };
}

QString FileReference::resolvedPath(const QString &projectDirectory) const
{
    if (scope == ReferenceScope::External)
        return QDir::isAbsolutePath(path) ? QDir::cleanPath(path) : QString();
    if (!portableRelativePath(path))
        return {};
    const QString root = QDir(projectDirectory).absolutePath();
    const QString resolved = QDir::cleanPath(QDir(root).absoluteFilePath(path));
    return sameOrContained(root, resolved) ? resolved : QString();
}

QJsonObject VmIdentity::toJson() const
{
    return QJsonObject{
        {QStringLiteral("providerId"), providerId},
        {QStringLiteral("providerVersion"), providerVersion},
        {QStringLiteral("id"), vmId},
        {QStringLiteral("name"), vmName},
        {QStringLiteral("config"), config.toJson()},
    };
}

QJsonObject ValidationMilestone::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("phase"), milestonePhaseName(phase)},
        {QStringLiteral("name"), name},
        {QStringLiteral("status"), milestoneStatusName(status)},
        {QStringLiteral("occurredAt"), timestamp(occurredAt)},
        {QStringLiteral("note"), note},
        {QStringLiteral("data"), data},
    };
}

QJsonObject ValidationLogEntry::toJson() const
{
    return QJsonObject{
        {QStringLiteral("occurredAt"), timestamp(occurredAt)},
        {QStringLiteral("channel"), channel},
        {QStringLiteral("message"), message},
    };
}

QJsonObject EvidenceReference::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("kind"), evidenceKindName(kind)},
        {QStringLiteral("label"), label},
        {QStringLiteral("file"), file.toJson()},
        {QStringLiteral("capturedAt"), timestamp(capturedAt)},
    };
}

bool ValidationRun::completed() const
{
    return isCompleted(status);
}

QJsonObject ValidationRun::toJson() const
{
    QJsonArray milestoneArray;
    for (const ValidationMilestone &item : milestones)
        milestoneArray.append(item.toJson());
    QJsonArray logArray;
    for (const ValidationLogEntry &item : logs)
        logArray.append(item.toJson());
    QJsonArray evidenceArray;
    for (const EvidenceReference &item : evidence)
        evidenceArray.append(item.toJson());
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("sequence"), sequence},
        {QStringLiteral("revision"), revision},
        {QStringLiteral("identityHash"), identityHash},
        {QStringLiteral("iso"), iso.toJson()},
        {QStringLiteral("image"), image.toJson()},
        {QStringLiteral("vm"), vm.toJson()},
        {QStringLiteral("configSnapshot"), configSnapshot},
        {QStringLiteral("startedAt"), timestamp(startedAt)},
        {QStringLiteral("updatedAt"), timestamp(updatedAt)},
        {QStringLiteral("endedAt"), timestamp(endedAt)},
        {QStringLiteral("status"), runStatusName(status)},
        {QStringLiteral("milestones"), milestoneArray},
        {QStringLiteral("logs"), logArray},
        {QStringLiteral("evidence"), evidenceArray},
        {QStringLiteral("completionNote"), completionNote},
    };
}

QJsonObject StoreSnapshot::toJson() const
{
    QJsonObject root = rootWithoutRevision(*this);
    root.insert(QStringLiteral("revision"), snapshotRevision(*this));
    return root;
}

VmValidationStore::VmValidationStore(QString projectDirectory)
    : m_projectDirectory(projectDirectory.trimmed().isEmpty()
          ? QString()
          : QDir(projectDirectory).absolutePath())
{
}

QString VmValidationStore::projectDirectory() const
{
    return m_projectDirectory;
}

QString VmValidationStore::stateFilePath() const
{
    return QDir(m_projectDirectory).filePath(QString::fromLatin1(RelativeStatePath));
}

QString VmValidationStore::backupFilePath() const
{
    return QDir(m_projectDirectory).filePath(QString::fromLatin1(RelativeBackupPath));
}

QString VmValidationStore::lockFilePath() const
{
    return QDir(m_projectDirectory).filePath(QString::fromLatin1(RelativeLockPath));
}

bool VmValidationStore::load(StoreSnapshot *snapshot, QString *error) const
{
    if (!snapshot) {
        setError(error, QStringLiteral("VM validation snapshot output is null."));
        return false;
    }
    QStringList projectErrors;
    canonicalProjectDirectory(m_projectDirectory, &projectErrors);
    if (!projectErrors.isEmpty()) {
        setError(error, projectErrors.join(QLatin1Char('\n')));
        return false;
    }
    return loadPrimary(m_projectDirectory, stateFilePath(), backupFilePath(), snapshot, error, true);
}

std::optional<ValidationRun> VmValidationStore::find(const QString &runId, QString *error) const
{
    StoreSnapshot snapshot;
    if (!load(&snapshot, error))
        return std::nullopt;
    const auto found = std::find_if(snapshot.runs.cbegin(), snapshot.runs.cend(),
                                    [&runId](const ValidationRun &run) {
                                        return run.id == runId;
                                    });
    setError(error, {});
    return found == snapshot.runs.cend() ? std::nullopt
                                         : std::optional<ValidationRun>(*found);
}

QList<ValidationRun> VmValidationStore::history(const RunFilter &filter, QString *error) const
{
    if (filter.maximumCount < 0 || filter.maximumCount > MaxRuns) {
        setError(error, QStringLiteral("History maximumCount is outside the supported range."));
        return {};
    }
    StoreSnapshot snapshot;
    if (!load(&snapshot, error))
        return {};
    QList<ValidationRun> result;
    const QString text = filter.text.trimmed();
    for (const ValidationRun &run : snapshot.runs) {
        if (!filter.providerId.isEmpty() && run.vm.providerId != filter.providerId)
            continue;
        if (!filter.vmId.isEmpty() && run.vm.vmId != filter.vmId)
            continue;
        if (filter.status && run.status != *filter.status)
            continue;
        if (filter.startedAtOrAfter.isValid()
            && run.startedAt < filter.startedAtOrAfter.toUTC()) {
            continue;
        }
        if (filter.startedBefore.isValid() && run.startedAt >= filter.startedBefore.toUTC())
            continue;
        if (!text.isEmpty()) {
            const QString haystack = QStringLiteral("%1\n%2\n%3\n%4\n%5\n%6")
                                         .arg(run.id, run.vm.providerId, run.vm.providerVersion,
                                              run.vm.vmId, run.vm.vmName, run.completionNote);
            if (!haystack.contains(text, Qt::CaseInsensitive))
                continue;
        }
        result.append(run);
    }
    if (filter.newestFirst)
        std::reverse(result.begin(), result.end());
    if (result.size() > filter.maximumCount)
        result.resize(filter.maximumCount);
    setError(error, {});
    return result;
}

bool VmValidationStore::appendRun(const RunStart &start,
                                  MutationResult *result,
                                  QString *error,
                                  const QString &expectedStoreRevision) const
{
    QLockFile lock(lockFilePath());
    StoreSnapshot snapshot;
    if (!prepareMutation(*this, expectedStoreRevision, &lock, &snapshot, error))
        return false;
    if (snapshot.runs.size() >= MaxRuns) {
        setError(error, QStringLiteral("VM validation history reached its run limit."));
        return false;
    }

    QStringList errors;
    if (!isValidKey(start.providerId))
        errors.append(QStringLiteral("Provider id is invalid."));
    boundedText(start.providerVersion, 1'024, QStringLiteral("Provider version"), &errors);
    boundedText(start.vmId, 4'096, QStringLiteral("VM id"), &errors, true);
    boundedText(start.vmName, 4'096, QStringLiteral("VM name"), &errors, true);
    if (jsonBytes(start.configSnapshot) > MaxConfigBytes)
        errors.append(QStringLiteral("VM configuration snapshot exceeds the size limit."));
    if (start.startedAt.isValid() && !start.startedAt.toUTC().isValid())
        errors.append(QStringLiteral("Run start timestamp is invalid."));
    if (!errors.isEmpty()) {
        setError(error, errors.join(QLatin1Char('\n')));
        return false;
    }

    ValidationRun run;
    QString referenceError;
    if (!normalizeInputReference(m_projectDirectory, start.isoPath, start.isoSha256,
                                 true, false, false, {}, &run.iso, &referenceError)
        || !normalizeInputReference(m_projectDirectory, start.imagePath, start.imageSha256,
                                    true, false, false, {}, &run.image, &referenceError)
        || !normalizeInputReference(m_projectDirectory, start.vmConfigPath, {},
                                    true, false, false, {}, &run.vm.config, &referenceError)) {
        setError(error, referenceError);
        return false;
    }
    run.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    run.sequence = snapshot.runs.size() + 1;
    run.revision = 1;
    run.vm.providerId = start.providerId;
    run.vm.providerVersion = start.providerVersion.trimmed();
    run.vm.vmId = start.vmId.trimmed();
    run.vm.vmName = start.vmName.trimmed();
    run.configSnapshot = start.configSnapshot;
    run.startedAt = start.startedAt.isValid() ? start.startedAt.toUTC()
                                               : QDateTime::currentDateTimeUtc();
    run.updatedAt = run.startedAt;
    run.identityHash = identityHash(run);
    validateRun(run, QStringLiteral("new run"), &errors);
    if (!errors.isEmpty()) {
        setError(error, errors.join(QLatin1Char('\n')));
        return false;
    }

    snapshot.runs.append(run);
    ++snapshot.generation;
    if (!writeSnapshot(stateFilePath(), backupFilePath(), &snapshot, error))
        return false;
    if (result)
        *result = MutationResult{run, snapshot.revision};
    setError(error, {});
    return true;
}

bool VmValidationStore::updateRun(const QString &runId,
                                  int expectedRunRevision,
                                  const RunUpdate &update,
                                  MutationResult *result,
                                  QString *error,
                                  const QString &expectedStoreRevision) const
{
    if (update.milestones.isEmpty() && update.logs.isEmpty() && update.evidence.isEmpty()) {
        setError(error, QStringLiteral("VM validation update is empty."));
        return false;
    }
    QLockFile lock(lockFilePath());
    StoreSnapshot snapshot;
    if (!prepareMutation(*this, expectedStoreRevision, &lock, &snapshot, error))
        return false;
    ValidationRun *stored = findMutable(&snapshot.runs, runId);
    if (!stored) {
        setError(error, QStringLiteral("VM validation run '%1' was not found.").arg(runId));
        return false;
    }
    if (stored->revision != expectedRunRevision) {
        setError(error, QStringLiteral("VM validation run changed; reload and retry."));
        return false;
    }
    if (stored->completed()) {
        setError(error, QStringLiteral("Completed VM validation runs are immutable."));
        return false;
    }
    ValidationRun candidate = *stored;
    QDateTime latest = candidate.updatedAt;
    for (const MilestoneDraft &draft : update.milestones) {
        ValidationMilestone item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.phase = draft.phase;
        item.name = draft.name.trimmed();
        item.status = draft.status;
        item.occurredAt = draft.occurredAt.isValid() ? draft.occurredAt.toUTC()
                                                      : QDateTime::currentDateTimeUtc();
        item.note = draft.note;
        item.data = draft.data;
        latest = later(latest, item.occurredAt);
        candidate.milestones.append(std::move(item));
    }
    for (const LogDraft &draft : update.logs) {
        ValidationLogEntry item;
        item.occurredAt = draft.occurredAt.isValid() ? draft.occurredAt.toUTC()
                                                      : QDateTime::currentDateTimeUtc();
        item.channel = draft.channel.trimmed();
        item.message = draft.message;
        latest = later(latest, item.occurredAt);
        candidate.logs.append(std::move(item));
    }
    for (const EvidenceDraft &draft : update.evidence) {
        EvidenceReference item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.kind = draft.kind;
        item.label = draft.label.trimmed();
        item.capturedAt = draft.capturedAt.isValid() ? draft.capturedAt.toUTC()
                                                      : QDateTime::currentDateTimeUtc();
        QString referenceError;
        if (!normalizeInputReference(m_projectDirectory, draft.path, {}, true, true,
                                     draft.external, draft.externalMetadata,
                                     &item.file, &referenceError)) {
            setError(error, referenceError);
            return false;
        }
        latest = later(latest, item.capturedAt);
        candidate.evidence.append(std::move(item));
    }
    candidate.updatedAt = later(latest, QDateTime::currentDateTimeUtc());
    ++candidate.revision;
    QStringList errors;
    validateRun(candidate, QStringLiteral("updated run"), &errors);
    if (!errors.isEmpty()) {
        setError(error, errors.join(QLatin1Char('\n')));
        return false;
    }

    *stored = candidate;
    ++snapshot.generation;
    if (!writeSnapshot(stateFilePath(), backupFilePath(), &snapshot, error))
        return false;
    if (result)
        *result = MutationResult{candidate, snapshot.revision};
    setError(error, {});
    return true;
}

bool VmValidationStore::completeRun(const QString &runId,
                                    int expectedRunRevision,
                                    RunStatus finalStatus,
                                    const QString &note,
                                    const QDateTime &endedAt,
                                    MutationResult *result,
                                    QString *error,
                                    const QString &expectedStoreRevision) const
{
    if (!isCompleted(finalStatus)) {
        setError(error, QStringLiteral("A completion status must be passed, failed, or cancelled."));
        return false;
    }
    if (note.toUtf8().size() > MaxNoteBytes) {
        setError(error, QStringLiteral("Completion note exceeds the size limit."));
        return false;
    }
    if ((finalStatus == RunStatus::Failed || finalStatus == RunStatus::Cancelled)
        && note.trimmed().isEmpty()) {
        setError(error, QStringLiteral("Failed and cancelled runs require a completion note."));
        return false;
    }

    QLockFile lock(lockFilePath());
    StoreSnapshot snapshot;
    if (!prepareMutation(*this, expectedStoreRevision, &lock, &snapshot, error))
        return false;
    ValidationRun *stored = findMutable(&snapshot.runs, runId);
    if (!stored) {
        setError(error, QStringLiteral("VM validation run '%1' was not found.").arg(runId));
        return false;
    }
    if (stored->revision != expectedRunRevision) {
        setError(error, QStringLiteral("VM validation run changed; reload and retry."));
        return false;
    }
    if (stored->completed()) {
        setError(error, QStringLiteral("Completed VM validation runs are immutable."));
        return false;
    }
    if (finalStatus == RunStatus::Passed) {
        const QStringList missing = passGateProblems(*stored);
        if (!missing.isEmpty()) {
            setError(error, QStringLiteral("A passed validation run requires %1.")
                                .arg(missing.join(QStringLiteral(", "))));
            return false;
        }
    }

    ValidationRun candidate = *stored;
    candidate.endedAt = endedAt.isValid() ? endedAt.toUTC() : QDateTime::currentDateTimeUtc();
    candidate.updatedAt = candidate.endedAt;
    candidate.status = finalStatus;
    candidate.completionNote = note;
    ++candidate.revision;
    QStringList errors;
    validateRun(candidate, QStringLiteral("completed run"), &errors);
    if (!errors.isEmpty()) {
        setError(error, errors.join(QLatin1Char('\n')));
        return false;
    }

    *stored = candidate;
    ++snapshot.generation;
    if (!writeSnapshot(stateFilePath(), backupFilePath(), &snapshot, error))
        return false;
    if (result)
        *result = MutationResult{candidate, snapshot.revision};
    setError(error, {});
    return true;
}

bool VmValidationStore::recoverFromBackup(RecoveryResult *result, QString *error) const
{
    QStringList projectErrors;
    canonicalProjectDirectory(m_projectDirectory, &projectErrors);
    if (!projectErrors.isEmpty()) {
        setError(error, projectErrors.join(QLatin1Char('\n')));
        return false;
    }
    if (!QDir().mkpath(QFileInfo(stateFilePath()).absolutePath())) {
        setError(error, QStringLiteral("Could not create the VM validation store folder."));
        return false;
    }
    QLockFile lock(lockFilePath());
    lock.setStaleLockTime(30'000);
    if (!lock.tryLock(10'000)) {
        setError(error, QStringLiteral("VM validation history is busy. Try again in a moment."));
        return false;
    }

    QByteArray primaryBytes;
    StoreSnapshot primary;
    QString primaryError;
    const bool primaryExists = QFileInfo::exists(stateFilePath());
    bool primaryOversized = false;
    if (primaryExists) {
        QFile primaryFile(stateFilePath());
        if (!primaryFile.open(QIODevice::ReadOnly)) {
            setError(error, QStringLiteral(
                "Cannot preserve the unreadable VM validation primary: %1")
                                .arg(primaryFile.errorString()));
            return false;
        }
        primaryOversized = primaryFile.size() > MaxStoreBytes;
        primaryFile.close();
        if (!primaryOversized) {
            if (!readBytes(stateFilePath(), &primaryBytes, &primaryError)) {
                setError(error, QStringLiteral(
                    "Cannot preserve the unreadable VM validation primary: %1")
                                    .arg(primaryError));
                return false;
            }
            if (parseSnapshot(primaryBytes, m_projectDirectory, &primary, &primaryError)) {
                setError(error,
                         QStringLiteral("VM validation store is valid and does not need recovery."));
                return false;
            }
        } else {
            primaryError = QStringLiteral("VM validation primary exceeds the size limit.");
        }
    }
    if (!QFileInfo::exists(backupFilePath())) {
        setError(error, QStringLiteral("No VM validation backup is available for recovery."));
        return false;
    }
    QByteArray backupBytes;
    StoreSnapshot backup;
    QString backupError;
    if (!readBytes(backupFilePath(), &backupBytes, &backupError)
        || !parseSnapshot(backupBytes, m_projectDirectory, &backup, &backupError)) {
        setError(error, QStringLiteral("VM validation backup is invalid: %1").arg(backupError));
        return false;
    }

    QString archivedPath;
    if (primaryExists) {
        const QString recoveryDirectory = QDir(m_projectDirectory).filePath(
            QStringLiteral(".wimforge/recovery"));
        QString digest;
        if (primaryOversized) {
            QString hashError;
            digest = fileSha256(stateFilePath(), &hashError).left(16);
            if (!hashError.isEmpty()) {
                setError(error, QStringLiteral("Could not identify the corrupt primary: %1")
                                    .arg(hashError));
                return false;
            }
        } else {
            digest = QString::fromLatin1(QCryptographicHash::hash(
                primaryBytes, QCryptographicHash::Sha256).toHex().left(16));
        }
        const QString stamp = QDateTime::currentDateTimeUtc().toString(
            QStringLiteral("yyyyMMdd-HHmmsszzz"));
        archivedPath = QDir(recoveryDirectory).filePath(
            QStringLiteral("vm-validation-runs-corrupt-%1-%2.json").arg(stamp, digest));
        int suffix = 2;
        while (QFileInfo::exists(archivedPath)) {
            archivedPath = QDir(recoveryDirectory).filePath(
                QStringLiteral("vm-validation-runs-corrupt-%1-%2-%3.json")
                    .arg(stamp, digest)
                    .arg(suffix++));
        }
        if (primaryOversized) {
            if (!QDir().mkpath(recoveryDirectory)
                || !QFile::copy(stateFilePath(), archivedPath)) {
                setError(error, QStringLiteral("Could not preserve the oversized corrupt primary."));
                return false;
            }
            QString archiveHashError;
            const QString archiveHash = fileSha256(archivedPath, &archiveHashError);
            QString sourceHashError;
            const QString sourceHash = fileSha256(stateFilePath(), &sourceHashError);
            if (!archiveHashError.isEmpty() || !sourceHashError.isEmpty()
                || archiveHash != sourceHash) {
                QFile::remove(archivedPath);
                setError(error, QStringLiteral("Corrupt primary archive verification failed."));
                return false;
            }
        } else if (!atomicWrite(archivedPath, primaryBytes, error)) {
            return false;
        }
    }
    if (!atomicWrite(stateFilePath(), backupBytes, error))
        return false;

    if (result) {
        result->restoredRevision = backup.revision;
        result->restoredRunCount = backup.runs.size();
        result->archivedCorruptPath = archivedPath;
        result->warning = primaryExists
            ? QStringLiteral("The corrupt primary was preserved at %1 before recovery.")
                  .arg(archivedPath)
            : QString();
    }
    setError(error, {});
    return true;
}

QString VmValidationStore::fileSha256(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (!info.isAbsolute() || !info.exists() || !info.isFile()) {
        setError(error, QStringLiteral("Cannot hash missing or non-file path: %1").arg(path));
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Could not read %1 for SHA-256: %2")
                            .arg(path, file.errorString()));
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            setError(error, QStringLiteral("Could not hash %1: %2").arg(path, file.errorString()));
            return {};
        }
        hash.addData(chunk);
    }
    setError(error, {});
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace wimforge::vmvalidation
