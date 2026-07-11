#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace wimforge::vmvalidation {

enum class RunStatus
{
    Running,
    Passed,
    Failed,
    Cancelled
};

enum class MilestonePhase
{
    Boot,
    Install
};

enum class MilestoneStatus
{
    Reached,
    Failed,
    Skipped
};

enum class EvidenceKind
{
    Screenshot,
    Log,
    Report,
    Other
};

enum class ReferenceScope
{
    Project,
    External
};

[[nodiscard]] QString runStatusName(RunStatus status);
[[nodiscard]] QString milestonePhaseName(MilestonePhase phase);
[[nodiscard]] QString milestoneStatusName(MilestoneStatus status);
[[nodiscard]] QString evidenceKindName(EvidenceKind kind);
[[nodiscard]] QString referenceScopeName(ReferenceScope scope);
[[nodiscard]] bool isCompleted(RunStatus status);

struct FileReference
{
    // Project-scoped paths use normalized forward-slash relative syntax.
    // External paths are absolute and are never resolved relative to a project.
    QString path;
    QString sha256;
    ReferenceScope scope = ReferenceScope::Project;
    QJsonObject externalMetadata;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] QString resolvedPath(const QString &projectDirectory) const;
};

struct VmIdentity
{
    QString providerId;
    QString providerVersion;
    QString vmId;
    QString vmName;
    FileReference config;

    [[nodiscard]] QJsonObject toJson() const;
};

struct ValidationMilestone
{
    QString id;
    MilestonePhase phase = MilestonePhase::Boot;
    QString name;
    MilestoneStatus status = MilestoneStatus::Reached;
    QDateTime occurredAt;
    QString note;
    QJsonObject data;

    [[nodiscard]] QJsonObject toJson() const;
};

struct ValidationLogEntry
{
    QDateTime occurredAt;
    QString channel;
    QString message;

    [[nodiscard]] QJsonObject toJson() const;
};

struct EvidenceReference
{
    QString id;
    EvidenceKind kind = EvidenceKind::Other;
    QString label;
    FileReference file;
    QDateTime capturedAt;

    [[nodiscard]] QJsonObject toJson() const;
};

struct ValidationRun
{
    QString id;
    qint64 sequence = 0;
    int revision = 0;
    QString identityHash;
    FileReference iso;
    FileReference image;
    VmIdentity vm;
    QJsonObject configSnapshot;
    QDateTime startedAt;
    QDateTime updatedAt;
    QDateTime endedAt;
    RunStatus status = RunStatus::Running;
    QList<ValidationMilestone> milestones;
    QList<ValidationLogEntry> logs;
    QList<EvidenceReference> evidence;
    QString completionNote;

    [[nodiscard]] bool completed() const;
    [[nodiscard]] QJsonObject toJson() const;
};

struct RunStart
{
    QString isoPath;
    // Optional input: when supplied, it must match the file. The persisted
    // run always contains the computed lowercase SHA-256.
    QString isoSha256;
    QString imagePath;
    QString imageSha256;
    QString providerId;
    QString providerVersion;
    QString vmId;
    QString vmName;
    QString vmConfigPath;
    QJsonObject configSnapshot;
    QDateTime startedAt;
};

struct MilestoneDraft
{
    MilestonePhase phase = MilestonePhase::Boot;
    QString name;
    MilestoneStatus status = MilestoneStatus::Reached;
    QDateTime occurredAt;
    QString note;
    QJsonObject data;
};

struct LogDraft
{
    QDateTime occurredAt;
    QString channel;
    QString message;
};

struct EvidenceDraft
{
    EvidenceKind kind = EvidenceKind::Other;
    QString label;
    QString path;
    QDateTime capturedAt;
    // An outside-project reference is accepted only when both this flag and
    // non-empty externalMetadata are supplied.
    bool external = false;
    QJsonObject externalMetadata;
};

struct RunUpdate
{
    QList<MilestoneDraft> milestones;
    QList<LogDraft> logs;
    QList<EvidenceDraft> evidence;
};

struct MutationResult
{
    ValidationRun run;
    QString storeRevision;
};

struct RunFilter
{
    QString providerId;
    QString vmId;
    std::optional<RunStatus> status;
    QDateTime startedAtOrAfter;
    QDateTime startedBefore;
    QString text;
    int maximumCount = 1'000;
    bool newestFirst = true;
};

struct StoreSnapshot
{
    qint64 generation = 0;
    QString revision;
    QList<ValidationRun> runs;
    // Non-zero only when an older, supported representation was migrated in
    // memory. The next successful mutation writes the current schema.
    int migratedFromVersion = 0;

    [[nodiscard]] QJsonObject toJson() const;
};

struct RecoveryResult
{
    QString restoredRevision;
    qsizetype restoredRunCount = 0;
    QString archivedCorruptPath;
    QString warning;
};

class VmValidationStore
{
public:
    static constexpr int CurrentVersion = 2;
    static constexpr int LegacyVersion = 1;
    static constexpr qsizetype MaxRuns = 10'000;
    static constexpr qsizetype MaxMilestonesPerRun = 512;
    static constexpr qsizetype MaxLogEntriesPerRun = 2'000;
    static constexpr qsizetype MaxEvidencePerRun = 512;
    static constexpr qsizetype MaxLogEntryBytes = 64 * 1024;
    static constexpr qsizetype MaxLogBytesPerRun = 1024 * 1024;
    static constexpr qsizetype MaxConfigBytes = 1024 * 1024;
    static constexpr qsizetype MaxMetadataBytes = 64 * 1024;
    static constexpr qsizetype MaxNoteBytes = 64 * 1024;
    static constexpr qsizetype MaxStoreBytes = 32 * 1024 * 1024;

    explicit VmValidationStore(QString projectDirectory);

    [[nodiscard]] QString projectDirectory() const;
    [[nodiscard]] QString stateFilePath() const;
    [[nodiscard]] QString backupFilePath() const;
    [[nodiscard]] QString lockFilePath() const;

    bool load(StoreSnapshot *snapshot, QString *error = nullptr) const;
    [[nodiscard]] std::optional<ValidationRun> find(const QString &runId,
                                                    QString *error = nullptr) const;
    [[nodiscard]] QList<ValidationRun> history(const RunFilter &filter = {},
                                               QString *error = nullptr) const;

    // Empty expectedStoreRevision means "merge against the latest state under
    // the writer lock". A non-empty value provides compare-and-swap semantics.
    bool appendRun(const RunStart &start,
                   MutationResult *result = nullptr,
                   QString *error = nullptr,
                   const QString &expectedStoreRevision = {}) const;
    bool updateRun(const QString &runId,
                   int expectedRunRevision,
                   const RunUpdate &update,
                   MutationResult *result = nullptr,
                   QString *error = nullptr,
                   const QString &expectedStoreRevision = {}) const;
    bool completeRun(const QString &runId,
                     int expectedRunRevision,
                     RunStatus finalStatus,
                     const QString &note,
                     const QDateTime &endedAt = {},
                     MutationResult *result = nullptr,
                     QString *error = nullptr,
                     const QString &expectedStoreRevision = {}) const;

    // Recovery is explicit: a corrupt primary is preserved byte-for-byte in
    // the recovery folder before the last atomic backup is restored.
    bool recoverFromBackup(RecoveryResult *result = nullptr,
                           QString *error = nullptr) const;

    [[nodiscard]] static QString fileSha256(const QString &path,
                                            QString *error = nullptr);

private:
    QString m_projectDirectory;
};

} // namespace wimforge::vmvalidation
