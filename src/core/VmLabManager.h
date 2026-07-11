#pragma once

#include "VmLab.h"
#include "VmLabProviders.h"

#include <QObject>

#include <functional>
#include <memory>
#include <optional>

namespace wimforge::vmlab {

enum class ManagerState
{
    Idle,
    DetectingProviders,
    RefreshingInventory,
    RefreshingSnapshots,
    Executing,
    Cancelling,
    Error
};

enum class ManagerAction
{
    Create,
    Register,
    OpenConsole,
    Start,
    GracefulShutdown,
    PowerOff,
    Pause,
    Resume,
    Reset,
    SaveState,
    Configure,
    AttachIso,
    DetachIso,
    AttachStorage,
    DetachStorage,
    AttachNetwork,
    DetachNetwork,
    ListSnapshots,
    TakeSnapshot,
    RestoreSnapshot,
    DeleteSnapshot,
    ForgetCatalog,
    Unregister,
    Delete
};

enum class StorageBus { Ide, Sata, Scsi, Nvme };

struct StorageDeviceSpec
{
    StorageBus bus = StorageBus::Sata;
    int controller = 0;
    int port = 1;
    int device = 0;
    // Optional provider-visible controller name. VirtualBox requires it when
    // addressing a non-default controller; VMware uses the numeric topology.
    QString controllerName;
    QString path;
    bool optical = false;
};

struct NetworkAdapterSpec
{
    // Provider adapters expose slots as one-based values to match VirtualBox.
    int slot = 1;
    NetworkMode mode = NetworkMode::Nat;
    QString interfaceName;
    bool connected = true;
};

struct CommandTranscript
{
    Command command;
    ProcessResult result;
};

struct OperationEvidence
{
    QUuid id;
    QString action;
    QDateTime startedAt;
    QDateTime finishedAt;
    bool success = false;
    bool cancelled = false;
    QString error;
    QList<CommandTranscript> commands;
    QList<FileEvidence> files;
};

struct InventoryRefreshResult
{
    bool success = false;
    bool complete = false;
    QList<Machine> machines;
    QStringList warnings;
    QString error;
};

struct OperationRequest
{
    ManagerAction action = ManagerAction::OpenConsole;
    std::optional<Machine> machine;
    CreateSpec createSpec;
    Ownership ownership = Ownership::Managed;
    QString path;
    QString name;
    QString description;
    ConfigPatch configPatch;
    StorageDeviceSpec storage;
    NetworkAdapterSpec network;
    Snapshot snapshot;
    bool headless = false;
    QString managedRoot;
    QList<Machine> allMachines;
    QString revision;
    QDateTime now;
};

class CancelableCommandRunner : public CommandRunner
{
public:
    ~CancelableCommandRunner() override = default;
    virtual void requestCancel() = 0;
    [[nodiscard]] virtual bool cancellationRequested() const = 0;
};

class CommandRunnerFactory
{
public:
    virtual ~CommandRunnerFactory() = default;
    [[nodiscard]] virtual std::shared_ptr<CancelableCommandRunner> create() const = 0;
};

class ProcessCommandRunnerFactory final : public CommandRunnerFactory
{
public:
    [[nodiscard]] std::shared_ptr<CancelableCommandRunner> create() const override;
};

class VmLabProviderAdapter
{
public:
    virtual ~VmLabProviderAdapter() = default;

    [[nodiscard]] virtual QList<ProviderInfo> detect(
        const QList<ProviderProbePaths> &candidates,
        CommandRunner &runner) const = 0;
    [[nodiscard]] virtual InventoryRefreshResult refreshInventory(
        const QList<ProviderInfo> &providers,
        const QList<Machine> &catalogMachines,
        CommandRunner &runner) const = 0;
    [[nodiscard]] virtual Plan plan(const ProviderInfo &provider,
                                    const OperationRequest &request) const = 0;
    [[nodiscard]] virtual QList<Snapshot> parseSnapshots(
        const QString &providerId,
        const QByteArray &standardOutput,
        QString *error = nullptr) const = 0;
};

class NativeVmLabProviderAdapter final : public VmLabProviderAdapter
{
public:
    [[nodiscard]] QList<ProviderInfo> detect(
        const QList<ProviderProbePaths> &candidates,
        CommandRunner &runner) const override;
    [[nodiscard]] InventoryRefreshResult refreshInventory(
        const QList<ProviderInfo> &providers,
        const QList<Machine> &catalogMachines,
        CommandRunner &runner) const override;
    [[nodiscard]] Plan plan(const ProviderInfo &provider,
                            const OperationRequest &request) const override;
    [[nodiscard]] QList<Snapshot> parseSnapshots(
        const QString &providerId,
        const QByteArray &standardOutput,
        QString *error = nullptr) const override;
};

class VmLabManager final : public QObject
{
    Q_OBJECT

public:
    explicit VmLabManager(
        QString catalogPath,
        QString managedRoot,
        std::shared_ptr<VmLabProviderAdapter> providerAdapter = {},
        std::shared_ptr<CommandRunnerFactory> runnerFactory = {},
        QObject *parent = nullptr);
    ~VmLabManager() override;

    VmLabManager(const VmLabManager &) = delete;
    VmLabManager &operator=(const VmLabManager &) = delete;

    [[nodiscard]] ManagerState state() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QString managedRoot() const;
    [[nodiscard]] QList<ProviderInfo> providers() const;
    [[nodiscard]] QList<Machine> machines() const;
    [[nodiscard]] QList<Snapshot> snapshots() const;
    [[nodiscard]] QList<OperationEvidence> evidenceHistory() const;
    [[nodiscard]] std::optional<Machine> selectedMachine() const;
    [[nodiscard]] std::optional<Plan> reviewedPlan() const;

    void setProbeCandidates(QList<ProviderProbePaths> candidates);
    void setAutoRefresh(bool enabled);
    [[nodiscard]] bool autoRefresh() const;

    bool load(QString *error = nullptr);
    bool selectMachine(const QString &providerId, const QString &id);
    void clearSelection();

    bool detectProviders();
    bool refreshInventory();
    bool refreshSnapshots();
    bool cancel();

    [[nodiscard]] std::optional<OperationPreview> reviewCreate(
        CreateSpec spec, Ownership ownership = Ownership::Managed);
    [[nodiscard]] std::optional<OperationPreview> reviewRegister(
        const QString &providerId, const QString &configurationPath,
        const QString &displayName, Ownership ownership = Ownership::External);
    [[nodiscard]] std::optional<OperationPreview> reviewOpenConsole();
    [[nodiscard]] std::optional<OperationPreview> reviewStart(bool headless);
    [[nodiscard]] std::optional<OperationPreview> reviewGracefulShutdown();
    [[nodiscard]] std::optional<OperationPreview> reviewPowerOff();
    [[nodiscard]] std::optional<OperationPreview> reviewPause();
    [[nodiscard]] std::optional<OperationPreview> reviewResume();
    [[nodiscard]] std::optional<OperationPreview> reviewReset();
    [[nodiscard]] std::optional<OperationPreview> reviewSaveState();
    [[nodiscard]] std::optional<OperationPreview> reviewConfigure(const ConfigPatch &patch);
    [[nodiscard]] std::optional<OperationPreview> reviewAttachIso(const QString &isoPath);
    [[nodiscard]] std::optional<OperationPreview> reviewDetachIso();
    [[nodiscard]] std::optional<OperationPreview> reviewAttachStorage(
        const StorageDeviceSpec &storage);
    [[nodiscard]] std::optional<OperationPreview> reviewDetachStorage(
        const StorageDeviceSpec &storage);
    [[nodiscard]] std::optional<OperationPreview> reviewAttachNetwork(
        const NetworkAdapterSpec &network);
    [[nodiscard]] std::optional<OperationPreview> reviewDetachNetwork(int slot);
    [[nodiscard]] std::optional<OperationPreview> reviewTakeSnapshot(
        const QString &name, const QString &description = {});
    [[nodiscard]] std::optional<OperationPreview> reviewRestoreSnapshot(
        const Snapshot &snapshot);
    [[nodiscard]] std::optional<OperationPreview> reviewDeleteSnapshot(
        const Snapshot &snapshot);
    [[nodiscard]] std::optional<OperationPreview> reviewUnregister();
    [[nodiscard]] std::optional<OperationPreview> reviewForgetCatalog();
    [[nodiscard]] std::optional<OperationPreview> reviewDelete();

    bool executeReviewed(const QUuid &previewId, const QString &typedConfirmation = {});
    void clearReviewedPlan();

signals:
    void stateChanged(wimforge::vmlab::ManagerState state);
    void providersChanged();
    void machinesChanged();
    void selectionChanged();
    void snapshotsChanged();
    void reviewedPlanChanged();
    void evidenceAdded(const wimforge::vmlab::OperationEvidence &evidence);
    void errorOccurred(const QString &message);
    void taskFinished(const wimforge::vmlab::OperationEvidence &evidence);

private:
    struct AsyncResult;
    struct Private;
    std::unique_ptr<Private> d;

    using AsyncWork = std::function<std::shared_ptr<AsyncResult>(CommandRunner &runner)>;

    bool startAsync(const QString &action, ManagerState state, AsyncWork work);
    void finishAsync(const std::shared_ptr<AsyncResult> &result);
    void setState(ManagerState state);
    void setError(const QString &error);
    [[nodiscard]] std::optional<OperationPreview> review(OperationRequest request);
    [[nodiscard]] std::optional<OperationPreview> reviewSelected(ManagerAction action);
    [[nodiscard]] std::optional<ProviderInfo> provider(const QString &id) const;
};

[[nodiscard]] QString managerStateName(ManagerState state);
[[nodiscard]] QString managerActionName(ManagerAction action);
[[nodiscard]] QString storageBusName(StorageBus bus);

} // namespace wimforge::vmlab

Q_DECLARE_METATYPE(wimforge::vmlab::ManagerState)
Q_DECLARE_METATYPE(wimforge::vmlab::OperationEvidence)
