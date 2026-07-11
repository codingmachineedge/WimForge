#pragma once

#include "VmLabManager.h"

#include <QJsonObject>

#include <memory>

namespace wimforge::vmlab {

// A JSON-friendly request envelope for the command-line VM Lab surface. The
// caller is responsible only for parsing command-line options into parameters;
// provider discovery, live inventory, review, confirmation, and execution stay
// in this service so CLI and desktop behavior share the same safety gates.
struct VmLabCliRequest
{
    QString action;
    QJsonObject parameters;
    bool execute = false;
    bool yes = false;
    QString typedConfirmation;
    QDateTime now;
    QList<ProviderProbePaths> probeCandidates;
};

struct VmLabCliResult
{
    enum ExitCode
    {
        Ok = 0,
        InvalidRequest = 2,
        ConfirmationRequired = 3,
        ProviderFailure = 4,
        CatalogFailure = 5
    };

    bool success = false;
    int exitCode = InvalidRequest;
    QString error;
    QJsonObject output;
};

// Synchronous by design: this class does not inherit QObject and never needs a
// GUI or Qt event loop. CommandRunner and VmLabProviderAdapter are injected so
// a CLI invocation can use real structured processes while tests remain fully
// deterministic.
class VmLabCliService
{
public:
    explicit VmLabCliService(
        QString catalogPath,
        QString managedRoot,
        std::shared_ptr<VmLabProviderAdapter> providerAdapter = {});

    [[nodiscard]] QString catalogPath() const;
    [[nodiscard]] QString managedRoot() const;
    [[nodiscard]] VmLabCliResult handle(const VmLabCliRequest &request,
                                        CommandRunner &runner) const;

    [[nodiscard]] static QStringList supportedActions();

private:
    QString m_catalogPath;
    QString m_managedRoot;
    std::shared_ptr<VmLabProviderAdapter> m_providerAdapter;
};

} // namespace wimforge::vmlab
