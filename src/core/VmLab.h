#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QLockFile>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <optional>
#include <functional>
#include <memory>

namespace wimforge::vmlab {

enum class PowerState
{
    Unknown,
    Inaccessible,
    PoweredOff,
    Running,
    Paused,
    Suspended,
    Saved,
    Aborted
};

enum class Ownership { External, Managed };
enum class Risk { ReadOnly, Reversible, Disruptive, Destructive };
enum class Firmware { Bios, Efi };
enum class NetworkMode { Nat, Bridged, HostOnly, Internal, Disconnected };
enum class EvidenceFormat { RawSha256, VmwareRunningPathsSha256 };

[[nodiscard]] QString virtualBoxProviderId();
[[nodiscard]] QString vmwareWorkstationProviderId();
[[nodiscard]] QString vmwarePlayerProviderId();
[[nodiscard]] bool isKnownProviderId(const QString &id);
[[nodiscard]] QString powerStateName(PowerState state);
[[nodiscard]] QString ownershipName(Ownership ownership);
[[nodiscard]] QString riskName(Risk risk);
[[nodiscard]] QString firmwareName(Firmware firmware);
[[nodiscard]] QString networkModeName(NetworkMode mode);
[[nodiscard]] bool isSafeMachineFileStem(const QString &value);

struct ProviderInfo
{
    QString id;
    QString displayName;
    QString executable;
    QString consoleExecutable;
    QString diskManagerExecutable;
    QString version;
    QStringList evidence;
    QStringList warnings;
    QSet<QString> capabilities;
    bool available = false;

    [[nodiscard]] bool supports(const QString &capability) const;
};

struct VmRef
{
    QString providerId;
    QString id;
    QString name;

    [[nodiscard]] bool valid() const;
};

struct StorageAttachment
{
    QString id;
    QString bus;
    int controller = 0;
    int port = 0;
    int device = 0;
    QString controllerName;
    QString path;
    bool optical = false;
};

struct NetworkAttachment
{
    QString id;
    int slot = 1;
    NetworkMode mode = NetworkMode::Disconnected;
    QString interfaceName;
    QString model;
    QString macAddress;
    bool connected = false;
};

struct Machine
{
    VmRef ref;
    QString configPath;
    // Live provider inventory only. Catalog persistence intentionally omits
    // storage topology so stale paths can never authorize deletion.
    QStringList storagePaths;
    QList<StorageAttachment> storageDevices;
    QList<NetworkAttachment> networkDevices;
    std::optional<int> cpuCount;
    std::optional<int> memoryMiB;
    std::optional<Firmware> firmware;
    std::optional<bool> secureBoot;
    std::optional<bool> tpm;
    PowerState powerState = PowerState::Unknown;
    Ownership ownership = Ownership::External;
    // Random capability written both to the catalog and to a marker inside a
    // WimForge-created VM directory. Managed deletion requires an exact match.
    QString ownershipToken;
    QString inaccessibleReason;
    QStringList warnings;
    // Ephemeral hash of the provider response used to derive live state.
    // Catalog persistence intentionally clears this value.
    QString stateRevision;
    bool inventoryComplete = false;
    // True only when the provider configuration was parsed into typed CPU,
    // memory, security, storage, and networking fields for safe editing.
    bool hardwareInventoryComplete = false;
};

struct Snapshot
{
    QString id;
    QString name;
    QString description;
    QDateTime createdAt;
    bool current = false;
    // SHA-256 of the exact provider snapshot-list output that produced this
    // entry. Restore/delete re-list and require the same revision.
    QString inventoryRevision;
};

struct CreateSpec
{
    QString providerId;
    QString id;
    QString name;
    QString directory;
    QString guestType;
    Firmware firmware = Firmware::Efi;
    bool secureBoot = false;
    bool tpm = false;
    // Provider planner resolves 0 to a version-compatible value.
    int virtualHardwareVersion = 0;
    int cpuCount = 2;
    int memoryMiB = 4096;
    int diskMiB = 65536;
    NetworkMode networkMode = NetworkMode::Nat;
    QString bridgedInterface;
    QString isoPath;
    bool unattendedBoot = false;
};

struct ConfigPatch
{
    std::optional<int> cpuCount;
    std::optional<int> memoryMiB;
    std::optional<Firmware> firmware;
    std::optional<bool> secureBoot;
    std::optional<bool> tpm;
    std::optional<NetworkMode> networkMode;
    std::optional<QString> bridgedInterface;
    // An empty value detaches the ISO. A non-empty value must be absolute.
    std::optional<QString> isoPath;

    [[nodiscard]] bool empty() const;
};

struct Command
{
    QString executable;
    QStringList arguments;
    QString workingDirectory;
    int timeoutMs = 30000;
    // Provider console applications outlive the reviewed launch operation and
    // must never be killed by the command timeout.
    bool detached = false;
    // Read-only probes may be interrupted and killed at their deadline.
    // Provider mutations default to a deferred cancel and are allowed to
    // finish so WimForge does not corrupt a VM mid-operation.
    bool interruptible = false;

    [[nodiscard]] bool valid(QString *error = nullptr) const;
};

struct OperationPreview
{
    QUuid id;
    QString action;
    VmRef target;
    Risk risk = Risk::ReadOnly;
    QStringList effects;
    QStringList warnings;
    QList<Command> commands;
    QString revision;
    QString confirmation;
    QDateTime expiry;

    [[nodiscard]] bool expired(const QDateTime &now = QDateTime::currentDateTimeUtc()) const;
};

struct AtomicWrite
{
    QString path;
    QByteArray contents;
    // Empty means the destination must not exist. Otherwise it is the
    // lowercase SHA-256 of the bytes observed while planning.
    QString expectedSha256;
};

struct CommandEvidence
{
    Command command;
    EvidenceFormat format = EvidenceFormat::RawSha256;
    QString expected;
    QString description;
};

struct FileEvidence
{
    QString path;
    // Filled only by the execution worker after the cheap review-time
    // identity/size/timestamp binding succeeds.
    QString expectedSha256;
    qint64 expectedSize = -1;
    qint64 expectedLastModifiedMs = -1;
    QString expectedIdentity;
    QString description;
};

struct ManagedDeletion
{
    Machine machine;
    QString managedRoot;
    QList<Machine> catalogMachines;
    // File identity captured by the reviewed preview. Execution refuses a
    // directory swapped into the same pathname after review.
    QString expectedIdentity;
    // Identity of the managed root captured by the reviewed preview. Both the
    // root and target are leased without FILE_SHARE_DELETE for execution.
    QString expectedRootIdentity;
    // VirtualBox unregisters before local removal. Its second, full provider
    // inventory must prove the target is absent before any file is deleted.
    bool expectTargetAbsentAfterCommands = false;
};

struct ManagedCreateReservation
{
    QString managedRoot;
    QString targetDirectory;
    // Root identity captured by review. Execution atomically reserves a new
    // direct child while retaining no-follow root/target handles.
    QString expectedRootIdentity;
};

struct Plan
{
    OperationPreview preview;
    QStringList errors;
    QList<AtomicWrite> atomicWritesAfterCommands;
    // Directories created exclusively immediately before provider execution.
    // This is used for VMware managed creates whose disk tool requires an
    // existing, dedicated directory.
    QStringList directoriesBeforeCommands;
    std::optional<ManagedCreateReservation> managedCreateReservation;
    std::optional<ManagedDeletion> managedDeletionAfterCommands;
    QList<CommandEvidence> preflight;
    QList<FileEvidence> filePreflight;
    // Populated only for managed create plans. The catalog stores this token
    // after every provider command and marker write succeeds.
    QString managedOwnershipToken;

    [[nodiscard]] bool ok() const { return errors.isEmpty(); }
};

[[nodiscard]] QString managedOwnershipMarkerFileName();
[[nodiscard]] QByteArray managedOwnershipMarkerContents(const VmRef &reference,
                                                         const QString &token);

struct ProcessResult
{
    bool started = false;
    bool timedOut = false;
    bool deadlineExceeded = false;
    int exitCode = -1;
    QByteArray standardOutput;
    QByteArray standardError;
    bool standardOutputTruncated = false;
    bool standardErrorTruncated = false;
    QString error;

    [[nodiscard]] bool ok() const;
};

class CommandRunner
{
public:
    virtual ~CommandRunner() = default;
    virtual ProcessResult run(const Command &command) = 0;
};

class ProcessCommandRunner final : public CommandRunner
{
public:
    ProcessResult run(const Command &command) override;
};

struct Result
{
    bool success = false;
    QString error;
    QList<ProcessResult> processes;
    QList<FileEvidence> verifiedFiles;
};

struct DeletionGuard
{
    bool allowed = false;
    QString canonicalDirectory;
    QString identity;
    QString canonicalRoot;
    QString rootIdentity;
    QString error;
};

struct CreationGuard
{
    bool allowed = false;
    QString canonicalRoot;
    QString targetDirectory;
    QString rootIdentity;
    QString error;
};

class PathPolicy
{
public:
    static CreationGuard managedCreateGuard(const QString &managedRoot,
                                            const QString &targetDirectory);
    static DeletionGuard managedDeletionGuard(const Machine &machine,
                                               const QString &managedRoot,
                                               const QList<Machine> &catalogMachines);
    static DeletionGuard managedDeletionGuardAfterUnregister(
        const Machine &machine,
        const QString &managedRoot,
        const QList<Machine> &catalogMachines);
    static bool deleteManagedDirectory(const Machine &machine,
                                       const QString &managedRoot,
                                       const QList<Machine> &catalogMachines,
                                       const QString &expectedIdentity,
                                       QString *error = nullptr);
};

class Catalog
{
public:
    static constexpr int CurrentVersion = 1;

    explicit Catalog(QString path = {});
    ~Catalog();

    Catalog(const Catalog &) = delete;
    Catalog &operator=(const Catalog &) = delete;

    [[nodiscard]] QString path() const;
    [[nodiscard]] QList<Machine> machines() const;
    [[nodiscard]] QString revision() const;

    void setMachines(const QList<Machine> &machines);
    bool upsert(const Machine &machine, QString *error = nullptr);
    bool remove(const VmRef &reference);
    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr);
    // Reloads and verifies the reviewed semantic revision while holding the
    // catalog writer lease through the provider mutation and catalog commit.
    bool beginTransaction(const QString &expectedRevision,
                          QString *error = nullptr);
    void endTransaction();
    [[nodiscard]] bool transactionActive() const;

private:
    QString m_path;
    QList<Machine> m_machines;
    QString m_expectedDiskSha256;
    bool m_diskStateKnown = false;
    bool m_expectedMissing = true;
    std::unique_ptr<QLockFile> m_transactionLock;
};

class Executor
{
public:
    using ManagedInventoryRefresh = std::function<bool(
        CommandRunner &runner, QList<Machine> *machines, QString *error)>;

    static bool validate(const Plan &plan,
                         const QString &currentRevision,
                         const QString &typedConfirmation,
                         const QDateTime &now,
                         QString *error = nullptr);
    static Result execute(const Plan &plan,
                          const QString &currentRevision,
                          const QString &typedConfirmation,
                          const QDateTime &now,
                          CommandRunner &runner,
                          const ManagedInventoryRefresh &managedInventoryRefresh = {});
};

[[nodiscard]] OperationPreview makePreview(const QString &action,
                                           const VmRef &target,
                                           Risk risk,
                                           const QStringList &effects,
                                           const QStringList &warnings,
                                           const QList<Command> &commands,
                                           const QString &revision,
                                           const QDateTime &now);
// Catalog-only repair operation. It deliberately does not require a live
// provider, but only external entries may be forgotten so WimForge never loses
// the sole ownership token/path record for files it created.
[[nodiscard]] Plan makeForgetCatalogPlan(const Machine &machine,
                                         const QString &revision,
                                         const QDateTime &now);
[[nodiscard]] QString fileSha256(const QString &path, QString *error = nullptr);
bool addFileEvidence(Plan &plan, const QString &path, const QString &description,
                     QString *error = nullptr);
[[nodiscard]] QString commandEvidence(EvidenceFormat format,
                                      const QByteArray &standardOutput,
                                      QString *error = nullptr);

} // namespace wimforge::vmlab
