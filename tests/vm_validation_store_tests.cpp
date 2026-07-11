#include "core/VmValidationStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>

#include <array>
#include <atomic>
#include <thread>

using namespace wimforge::vmvalidation;

namespace {

class TestRun
{
public:
    void check(bool condition, const QString &message)
    {
        if (condition)
            return;
        ++m_failures;
        QTextStream(stderr) << "FAIL: " << message << '\n';
    }

    [[nodiscard]] int result() const
    {
        if (m_failures == 0)
            QTextStream(stdout) << "vm_validation_store_tests: all checks passed\n";
        return m_failures == 0 ? 0 : 1;
    }

private:
    int m_failures = 0;
};

bool writeFile(const QString &path, const QByteArray &contents)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size() && file.flush();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

struct Fixture
{
    explicit Fixture(const QString &root, const QString &stem = QStringLiteral("fixture"))
        : project(QDir(root).filePath(stem + QStringLiteral("-project"))),
          outside(QDir(root).filePath(stem + QStringLiteral("-outside"))),
          iso(QDir(project).filePath(QStringLiteral("media/source.iso"))),
          image(QDir(project).filePath(QStringLiteral("images/install.wim"))),
          config(QDir(project).filePath(QStringLiteral("vm/test.vmx"))),
          screenshot(QDir(project).filePath(QStringLiteral("evidence/boot.png"))),
          outsideEvidence(QDir(outside).filePath(QStringLiteral("outside.png")))
    {
        QDir().mkpath(project);
        QDir().mkpath(outside);
        writeFile(iso, QByteArray("iso-") + stem.toUtf8());
        writeFile(image, QByteArray("image-") + stem.toUtf8());
        writeFile(config, QByteArray("config.version = 8\n"));
        writeFile(screenshot, QByteArray("png-fixture"));
        writeFile(outsideEvidence, QByteArray("outside-evidence"));
    }

    [[nodiscard]] RunStart start(const QString &vmId = QStringLiteral("vm-1"),
                                 const QString &provider = QStringLiteral("virtualbox")) const
    {
        RunStart result;
        result.isoPath = iso;
        result.imagePath = image;
        result.providerId = provider;
        result.providerVersion = QStringLiteral("7.1.8");
        result.vmId = vmId;
        result.vmName = QStringLiteral("Windows validation %1").arg(vmId);
        result.vmConfigPath = config;
        result.configSnapshot = QJsonObject{
            {QStringLiteral("cpuCount"), 4},
            {QStringLiteral("memoryMiB"), 8192},
            {QStringLiteral("firmware"), QStringLiteral("efi")},
            {QStringLiteral("profile"), QStringLiteral("installation")},
        };
        result.startedAt = QDateTime::currentDateTimeUtc().addSecs(-120);
        return result;
    }

    QString project;
    QString outside;
    QString iso;
    QString image;
    QString config;
    QString screenshot;
    QString outsideEvidence;
};

struct ValidationProfileCase
{
    QString name;
    QStringList requiredMilestones;
};

QList<ValidationProfileCase> validationProfileCases()
{
    return {
        {QStringLiteral("installation"),
         {QStringLiteral("installation-boot"), QStringLiteral("disk-layout"),
          QStringLiteral("installation-complete")}},
        {QStringLiteral("first-boot"),
         {QStringLiteral("first-boot"), QStringLiteral("drivers"),
          QStringLiteral("networking"), QStringLiteral("smoke-test")}},
        {QStringLiteral("upgrade"),
         {QStringLiteral("installation-boot"), QStringLiteral("installation-complete"),
          QStringLiteral("first-boot"), QStringLiteral("drivers"),
          QStringLiteral("smoke-test")}},
        {QStringLiteral("customization"),
         {QStringLiteral("customizations"), QStringLiteral("first-boot"),
          QStringLiteral("smoke-test")}},
        {QStringLiteral("full-smoke"),
         {QStringLiteral("installation-boot"), QStringLiteral("disk-layout"),
          QStringLiteral("installation-complete"), QStringLiteral("first-boot"),
          QStringLiteral("drivers"), QStringLiteral("networking"),
          QStringLiteral("customizations"), QStringLiteral("smoke-test")}},
    };
}

MilestonePhase profileMilestonePhase(const QString &name)
{
    return name.startsWith(QStringLiteral("installation"))
            || name == QStringLiteral("disk-layout")
        ? MilestonePhase::Install : MilestonePhase::Boot;
}

RunUpdate profileUpdate(const QStringList &milestoneNames,
                        const QString &evidencePath)
{
    RunUpdate update;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (qsizetype index = 0; index < milestoneNames.size(); ++index) {
        const QString &name = milestoneNames.at(index);
        update.milestones.append(MilestoneDraft{
            profileMilestonePhase(name), name, MilestoneStatus::Reached,
            now.addSecs(-60 + index), QStringLiteral("Profile gate reached"), {}});
    }
    update.evidence.append(EvidenceDraft{
        EvidenceKind::Screenshot, QStringLiteral("Profile validation evidence"),
        evidencePath, now.addSecs(-30), false, {}});
    return update;
}

void testLifecycleAndPortableEvidence(TestRun &test, const QString &root)
{
    Fixture fixture(root, QStringLiteral("lifecycle"));
    VmValidationStore store(fixture.project);
    QString error;
    StoreSnapshot empty;
    test.check(store.load(&empty, &error) && empty.runs.isEmpty()
                   && empty.revision.size() == 64,
               QStringLiteral("missing store loads as a revisioned empty snapshot"));

    RunStart start = fixture.start();
    start.isoSha256 = VmValidationStore::fileSha256(fixture.iso);
    start.imageSha256 = VmValidationStore::fileSha256(fixture.image);
    MutationResult created;
    test.check(store.appendRun(start, &created, &error, empty.revision),
               QStringLiteral("run is appended with compare-and-swap: %1").arg(error));
    const QString immutableIdentity = created.run.identityHash;
    test.check(created.run.status == RunStatus::Running && created.run.revision == 1
                   && created.run.iso.scope == ReferenceScope::Project
                   && created.run.iso.path == QStringLiteral("media/source.iso")
                   && created.run.iso.sha256 == start.isoSha256
                   && created.run.image.sha256 == start.imageSha256
                   && created.run.vm.config.path == QStringLiteral("vm/test.vmx")
                   && created.run.vm.config.sha256
                       == VmValidationStore::fileSha256(fixture.config),
               QStringLiteral("artifacts retain exact hashes and portable project references"));
    test.check(QFileInfo(created.run.iso.resolvedPath(fixture.project)).canonicalFilePath()
                   == QFileInfo(fixture.iso).canonicalFilePath(),
               QStringLiteral("portable reference resolves inside its project"));
    test.check(!store.completeRun(created.run.id, created.run.revision, RunStatus::Passed,
                                   QStringLiteral("premature"), {}, nullptr, &error,
                                   created.storeRevision)
                   && error.contains(QStringLiteral("installation-boot")),
               QStringLiteral("a run cannot pass without its profile milestones and hashed evidence"));

    RunUpdate update;
    update.milestones = {
        MilestoneDraft{MilestonePhase::Install, QStringLiteral("installation-boot"),
                       MilestoneStatus::Reached,
                       QDateTime::currentDateTimeUtc().addSecs(-60),
                       QStringLiteral("ISO booted"),
                       QJsonObject{{QStringLiteral("screen"), QStringLiteral("boot")}}},
        MilestoneDraft{MilestonePhase::Install, QStringLiteral("disk-layout"),
                       MilestoneStatus::Reached,
                       QDateTime::currentDateTimeUtc().addSecs(-45),
                       QStringLiteral("Installer started"), {}},
        MilestoneDraft{MilestonePhase::Install, QStringLiteral("installation-complete"),
                       MilestoneStatus::Reached,
                       QDateTime::currentDateTimeUtc().addSecs(-30),
                       QStringLiteral("Installer completed"), {}},
    };
    update.logs = {
        LogDraft{QDateTime::currentDateTimeUtc().addSecs(-50),
                 QStringLiteral("provider"), QStringLiteral("VM started")},
        LogDraft{QDateTime::currentDateTimeUtc().addSecs(-40),
                 QStringLiteral("installer"), QStringLiteral("Setup entered specialize")},
    };
    update.evidence = {
        EvidenceDraft{EvidenceKind::Screenshot, QStringLiteral("Boot screen"),
                      fixture.screenshot, QDateTime::currentDateTimeUtc().addSecs(-55), false, {}},
    };
    MutationResult updated;
    test.check(store.updateRun(created.run.id, created.run.revision, update, &updated, &error,
                               created.storeRevision),
               QStringLiteral("profile milestones, logs, and evidence append atomically: %1")
                   .arg(error));
    test.check(updated.run.revision == 2 && updated.run.milestones.size() == 3
                   && updated.run.logs.size() == 2 && updated.run.evidence.size() == 1
                   && updated.run.evidence.first().file.path == QStringLiteral("evidence/boot.png")
                   && updated.run.evidence.first().file.sha256
                       == VmValidationStore::fileSha256(fixture.screenshot)
                   && updated.run.identityHash == immutableIdentity,
               QStringLiteral("update preserves identity and records bounded evidence with SHA-256"));

    MutationResult completed;
    test.check(store.completeRun(updated.run.id, updated.run.revision, RunStatus::Passed,
                                 QStringLiteral("Boot and installation validation passed."), {},
                                 &completed, &error, updated.storeRevision),
               QStringLiteral("running run transitions to passed: %1").arg(error));
    test.check(completed.run.completed() && completed.run.endedAt.isValid()
                   && completed.run.updatedAt == completed.run.endedAt
                   && completed.run.identityHash == immutableIdentity,
               QStringLiteral("completed state contains immutable identity and end timestamp"));

    test.check(!store.updateRun(completed.run.id, completed.run.revision, update, nullptr, &error)
                   && error.contains(QStringLiteral("immutable"), Qt::CaseInsensitive),
               QStringLiteral("completed runs reject updates"));
    test.check(!store.completeRun(completed.run.id, completed.run.revision, RunStatus::Failed,
                                  QStringLiteral("changed"), {}, nullptr, &error)
                   && error.contains(QStringLiteral("immutable"), Qt::CaseInsensitive),
               QStringLiteral("completed result cannot be replaced"));
    test.check(!store.completeRun(completed.run.id, completed.run.revision, RunStatus::Running,
                                  {}, {}, nullptr, &error),
               QStringLiteral("running is not accepted as a completion status"));

    StoreSnapshot loaded;
    test.check(store.load(&loaded, &error) && loaded.runs.size() == 1
                   && loaded.runs.first().toJson() == completed.run.toJson()
                   && loaded.toJson().value(QStringLiteral("schema")).toString()
                       == QStringLiteral("wimforge.vm-validation-runs")
                   && loaded.toJson().value(QStringLiteral("version")).toInt()
                       == VmValidationStore::CurrentVersion
                   && loaded.toJson() == loaded.toJson(),
               QStringLiteral("current schema round-trips deterministically"));
}

void testProfileAwarePassGates(TestRun &test, const QString &root)
{
    for (const ValidationProfileCase &profile : validationProfileCases()) {
        Fixture fixture(root, QStringLiteral("profile-") + profile.name);
        VmValidationStore store(fixture.project);
        QString error;

        RunStart completeStart = fixture.start(QStringLiteral("complete-") + profile.name);
        completeStart.configSnapshot.insert(QStringLiteral("profile"), profile.name);
        MutationResult completeRun;
        test.check(store.appendRun(completeStart, &completeRun, &error),
                   QStringLiteral("%1 profile run starts: %2").arg(profile.name, error));
        MutationResult completeUpdate;
        test.check(store.updateRun(
                       completeRun.run.id, completeRun.run.revision,
                       profileUpdate(profile.requiredMilestones, fixture.screenshot),
                       &completeUpdate, &error),
                   QStringLiteral("%1 profile records every required milestone: %2")
                       .arg(profile.name, error));
        test.check(store.completeRun(
                       completeUpdate.run.id, completeUpdate.run.revision,
                       RunStatus::Passed, QStringLiteral("Exact profile gates passed."),
                       {}, nullptr, &error),
                   QStringLiteral("%1 profile passes with exact required milestones: %2")
                       .arg(profile.name, error));

        RunStart incompleteStart = fixture.start(QStringLiteral("incomplete-") + profile.name);
        incompleteStart.configSnapshot.insert(QStringLiteral("profile"), profile.name);
        MutationResult incompleteRun;
        test.check(store.appendRun(incompleteStart, &incompleteRun, &error),
                   QStringLiteral("%1 incomplete profile run starts: %2")
                       .arg(profile.name, error));
        QStringList incompleteMilestones = profile.requiredMilestones;
        const QString missing = incompleteMilestones.takeLast();
        MutationResult incompleteUpdate;
        test.check(store.updateRun(
                       incompleteRun.run.id, incompleteRun.run.revision,
                       profileUpdate(incompleteMilestones, fixture.screenshot),
                       &incompleteUpdate, &error),
                   QStringLiteral("%1 incomplete profile records partial gates: %2")
                       .arg(profile.name, error));
        test.check(!store.completeRun(
                       incompleteUpdate.run.id, incompleteUpdate.run.revision,
                       RunStatus::Passed, {}, {}, nullptr, &error)
                       && error.contains(missing),
                   QStringLiteral("%1 profile rejects missing exact milestone %2: %3")
                       .arg(profile.name, missing, error));
    }

    Fixture arbitraryFixture(root, QStringLiteral("profile-arbitrary"));
    VmValidationStore arbitraryStore(arbitraryFixture.project);
    QString error;
    RunStart arbitraryStart = arbitraryFixture.start(QStringLiteral("arbitrary-names"));
    arbitraryStart.configSnapshot.insert(QStringLiteral("profile"),
                                         QStringLiteral("full-smoke"));
    MutationResult arbitraryRun;
    test.check(arbitraryStore.appendRun(arbitraryStart, &arbitraryRun, &error),
               QStringLiteral("arbitrary-name profile run starts: %1").arg(error));
    QStringList arbitraryNames;
    for (int index = 0; index < 8; ++index)
        arbitraryNames.append(QStringLiteral("arbitrary-milestone-%1").arg(index));
    MutationResult arbitraryUpdate;
    test.check(arbitraryStore.updateRun(
                   arbitraryRun.run.id, arbitraryRun.run.revision,
                   profileUpdate(arbitraryNames, arbitraryFixture.screenshot),
                   &arbitraryUpdate, &error),
               QStringLiteral("arbitrary-name milestones are recorded: %1").arg(error));
    test.check(!arbitraryStore.completeRun(
                   arbitraryUpdate.run.id, arbitraryUpdate.run.revision,
                   RunStatus::Passed, {}, {}, nullptr, &error)
                   && error.contains(QStringLiteral("installation-boot")),
               QStringLiteral("arbitrary milestone names cannot satisfy full-smoke: %1")
                   .arg(error));

    RunStart unsupportedStart = arbitraryFixture.start(QStringLiteral("unsupported-profile"));
    unsupportedStart.configSnapshot.insert(QStringLiteral("profile"),
                                           QStringLiteral("invented-profile"));
    MutationResult unsupportedRun;
    test.check(arbitraryStore.appendRun(unsupportedStart, &unsupportedRun, &error),
               QStringLiteral("unsupported profile can be tracked while running: %1").arg(error));
    MutationResult unsupportedUpdate;
    test.check(arbitraryStore.updateRun(
                   unsupportedRun.run.id, unsupportedRun.run.revision,
                   profileUpdate({QStringLiteral("installation-boot")},
                                 arbitraryFixture.screenshot),
                   &unsupportedUpdate, &error),
               QStringLiteral("unsupported profile evidence is recorded: %1").arg(error));
    test.check(!arbitraryStore.completeRun(
                   unsupportedUpdate.run.id, unsupportedUpdate.run.revision,
                   RunStatus::Passed, {}, {}, nullptr, &error)
                   && error.contains(QStringLiteral("supported validation profile")),
               QStringLiteral("unsupported profile cannot be certified as passed: %1")
                   .arg(error));
}

void testPathSafetyAndExternalEvidence(TestRun &test, const QString &root)
{
    Fixture fixture(root, QStringLiteral("paths"));
    VmValidationStore store(fixture.project);
    QString error;
    MutationResult created;
    test.check(store.appendRun(fixture.start(), &created, &error),
               QStringLiteral("path safety fixture starts"));

    RunUpdate unsafe;
    unsafe.evidence = {EvidenceDraft{EvidenceKind::Screenshot, QStringLiteral("Outside"),
                                     fixture.outsideEvidence, {}, false, {}}};
    test.check(!store.updateRun(created.run.id, created.run.revision, unsafe, nullptr, &error)
                   && error.contains(QStringLiteral("Outside-project"), Qt::CaseInsensitive),
               QStringLiteral("outside evidence is rejected without explicit external declaration"));
    unsafe.evidence[0].external = true;
    test.check(!store.updateRun(created.run.id, created.run.revision, unsafe, nullptr, &error)
                   && error.contains(QStringLiteral("metadata"), Qt::CaseInsensitive),
               QStringLiteral("external flag alone is insufficient"));
    unsafe.evidence[0].externalMetadata = QJsonObject{
        {QStringLiteral("reason"), QStringLiteral("Captured by provider console")},
        {QStringLiteral("retention"), QStringLiteral("operator-managed")},
    };
    MutationResult externalAdded;
    test.check(store.updateRun(created.run.id, created.run.revision, unsafe, &externalAdded, &error),
               QStringLiteral("explicit external evidence metadata is accepted: %1").arg(error));
    const FileReference external = externalAdded.run.evidence.first().file;
    test.check(external.scope == ReferenceScope::External
                   && QDir::isAbsolutePath(external.path)
                   && !external.externalMetadata.isEmpty(),
               QStringLiteral("external evidence remains absolute and explicitly marked"));

    RunStart mismatch = fixture.start(QStringLiteral("bad-hash"));
    mismatch.isoSha256 = QString(64, QLatin1Char('0'));
    test.check(!store.appendRun(mismatch, nullptr, &error)
                   && error.contains(QStringLiteral("SHA-256")),
               QStringLiteral("artifact hash mismatch is rejected"));

    RunUpdate traversal;
    traversal.evidence = {EvidenceDraft{EvidenceKind::Report, QStringLiteral("Traversal"),
                                        QStringLiteral("../paths-outside/outside.png"), {},
                                        false, {}}};
    test.check(!store.updateRun(externalAdded.run.id, externalAdded.run.revision,
                                traversal, nullptr, &error),
               QStringLiteral("relative traversal cannot become project evidence"));
}

void testCasConcurrencyAndFiltering(TestRun &test, const QString &root)
{
    Fixture fixture(root, QStringLiteral("concurrent"));
    constexpr int ThreadCount = 8;
    std::array<std::atomic_bool, ThreadCount> succeeded{};
    std::array<std::thread, ThreadCount> threads;
    for (int index = 0; index < ThreadCount; ++index) {
        threads[static_cast<size_t>(index)] = std::thread([&, index] {
            VmValidationStore writer(fixture.project);
            QString localError;
            RunStart start = fixture.start(QStringLiteral("vm-%1").arg(index),
                                           index % 2 == 0
                                               ? QStringLiteral("virtualbox")
                                               : QStringLiteral("vmware-workstation"));
            succeeded[static_cast<size_t>(index)].store(
                writer.appendRun(start, nullptr, &localError));
        });
    }
    for (std::thread &thread : threads)
        thread.join();
    bool allSucceeded = true;
    for (const std::atomic_bool &value : succeeded)
        allSucceeded = allSucceeded && value.load();

    VmValidationStore store(fixture.project);
    QString error;
    StoreSnapshot snapshot;
    test.check(allSucceeded && store.load(&snapshot, &error)
                   && snapshot.runs.size() == ThreadCount,
               QStringLiteral("writer lock preserves every concurrent append: %1").arg(error));
    bool contiguous = true;
    for (qsizetype index = 0; index < snapshot.runs.size(); ++index)
        contiguous = contiguous && snapshot.runs.at(index).sequence == index + 1;
    test.check(contiguous, QStringLiteral("concurrent history sequences remain contiguous"));

    const QString staleRevision = snapshot.revision;
    MutationResult firstCas;
    test.check(store.appendRun(fixture.start(QStringLiteral("cas-one")), &firstCas, &error,
                               staleRevision),
               QStringLiteral("first CAS append succeeds"));
    test.check(!store.appendRun(fixture.start(QStringLiteral("cas-two")), nullptr, &error,
                                staleRevision)
                   && error.contains(QStringLiteral("changed on disk")),
               QStringLiteral("stale CAS append is rejected without overwriting"));

    RunFilter virtualBox;
    virtualBox.providerId = QStringLiteral("virtualbox");
    virtualBox.maximumCount = 3;
    const QList<ValidationRun> filtered = store.history(virtualBox, &error);
    test.check(filtered.size() == 3
                   && std::all_of(filtered.cbegin(), filtered.cend(), [](const ValidationRun &run) {
                          return run.vm.providerId == QStringLiteral("virtualbox");
                      })
                   && filtered.first().sequence > filtered.last().sequence,
               QStringLiteral("history filters provider, count, and newest-first order"));
    RunFilter search;
    search.vmId = QStringLiteral("cas-one");
    search.text = QStringLiteral("WINDOWS VALIDATION CAS-ONE");
    test.check(store.history(search, &error).size() == 1,
               QStringLiteral("history filters VM identity and case-insensitive text"));
}

void testCorruptionRecoveryAndRollback(TestRun &test, const QString &root)
{
    Fixture fixture(root, QStringLiteral("recovery"));
    VmValidationStore store(fixture.project);
    QString error;
    MutationResult created;
    test.check(store.appendRun(fixture.start(), &created, &error),
               QStringLiteral("recovery fixture starts"));
    RunUpdate update;
    update.logs = {LogDraft{QDateTime::currentDateTimeUtc().addSecs(-20),
                            QStringLiteral("provider"), QStringLiteral("healthy")}};
    MutationResult updated;
    test.check(store.updateRun(created.run.id, created.run.revision, update, &updated, &error),
               QStringLiteral("recovery backup advances with latest state"));

    const QByteArray corruptBytes("{ definitely-not-json\n");
    test.check(writeFile(store.stateFilePath(), corruptBytes),
               QStringLiteral("test corrupts primary store"));
    StoreSnapshot refused;
    test.check(!store.load(&refused, &error)
                   && error.contains(QStringLiteral("backup"), Qt::CaseInsensitive)
                   && readFile(store.stateFilePath()) == corruptBytes,
               QStringLiteral("corruption is reported and never silently replaced"));
    RecoveryResult recovered;
    test.check(store.recoverFromBackup(&recovered, &error)
                   && recovered.restoredRunCount == 1
                   && QFileInfo::exists(recovered.archivedCorruptPath)
                   && readFile(recovered.archivedCorruptPath) == corruptBytes
                   && recovered.warning.contains(QStringLiteral("preserved"))
                   && error.isEmpty(),
               QStringLiteral("explicit recovery preserves corrupt bytes and restores backup: %1")
                   .arg(error));
    StoreSnapshot restored;
    test.check(store.load(&restored, &error) && restored.runs.first().revision == 2
                   && restored.runs.first().logs.size() == 1,
               QStringLiteral("recovery restores the latest fully committed run"));

    test.check(QFile::remove(store.stateFilePath())
                   && !store.load(&restored, &error)
                   && error.contains(QStringLiteral("primary is missing"), Qt::CaseInsensitive),
               QStringLiteral("missing primary never silently becomes an empty history"));
    RecoveryResult missingPrimaryRecovery;
    test.check(store.recoverFromBackup(&missingPrimaryRecovery, &error)
                   && missingPrimaryRecovery.archivedCorruptPath.isEmpty()
                   && missingPrimaryRecovery.restoredRunCount == 1,
               QStringLiteral("missing primary is explicitly restored from the latest backup"));
    test.check(store.load(&restored, &error) && restored.runs.first().revision == 2,
               QStringLiteral("missing-primary recovery retains the committed run"));

    const QByteArray primaryBeforeFailure = readFile(store.stateFilePath());
    test.check(QFile::remove(store.backupFilePath())
                   && QDir().mkpath(store.backupFilePath()),
               QStringLiteral("test creates a backup write failure"));
    RunUpdate blocked;
    blocked.logs = {LogDraft{QDateTime::currentDateTimeUtc(), QStringLiteral("provider"),
                             QStringLiteral("must roll back")}};
    test.check(!store.updateRun(restored.runs.first().id, restored.runs.first().revision,
                                blocked, nullptr, &error)
                   && readFile(store.stateFilePath()) == primaryBeforeFailure,
               QStringLiteral("backup/write failure leaves primary state byte-for-byte unchanged"));
}

void testMigrationAndBounds(TestRun &test, const QString &root)
{
    Fixture fixture(root, QStringLiteral("migration"));
    VmValidationStore store(fixture.project);
    QString hashError;
    const QString isoHash = VmValidationStore::fileSha256(fixture.iso, &hashError);
    const QString imageHash = VmValidationStore::fileSha256(fixture.image, &hashError);
    const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(-120);
    const QJsonObject legacyRun{
        {QStringLiteral("sequence"), 1},
        {QStringLiteral("isoPath"), QStringLiteral("media/source.iso")},
        {QStringLiteral("isoSha256"), isoHash},
        {QStringLiteral("imagePath"), QStringLiteral("images/install.wim")},
        {QStringLiteral("imageSha256"), imageHash},
        {QStringLiteral("providerId"), QStringLiteral("virtualbox")},
        {QStringLiteral("providerVersion"), QStringLiteral("7.0")},
        {QStringLiteral("vmId"), QStringLiteral("legacy-vm")},
        {QStringLiteral("vmName"), QStringLiteral("Legacy validation")},
        {QStringLiteral("vmConfigPath"), QStringLiteral("vm/test.vmx")},
        {QStringLiteral("configSnapshot"), QJsonObject{{QStringLiteral("memoryMiB"), 4096}}},
        {QStringLiteral("startedAt"), start.toString(Qt::ISODateWithMs)},
        {QStringLiteral("status"), QStringLiteral("running")},
        {QStringLiteral("milestones"), QJsonArray{}},
        {QStringLiteral("logs"), QJsonArray{}},
        {QStringLiteral("evidence"), QJsonArray{}},
    };
    const QJsonObject legacyRoot{
        {QStringLiteral("schema"), QStringLiteral("wimforge.vm-validation-runs")},
        {QStringLiteral("version"), VmValidationStore::LegacyVersion},
        {QStringLiteral("generation"), 1},
        {QStringLiteral("runs"), QJsonArray{legacyRun}},
    };
    test.check(writeFile(store.stateFilePath(),
                         QJsonDocument(legacyRoot).toJson(QJsonDocument::Indented)),
               QStringLiteral("legacy fixture is written"));
    StoreSnapshot migrated;
    QString error;
    test.check(store.load(&migrated, &error)
                   && migrated.migratedFromVersion == VmValidationStore::LegacyVersion
                   && migrated.runs.size() == 1 && !migrated.runs.first().identityHash.isEmpty()
                   && migrated.runs.first().iso.path == QStringLiteral("media/source.iso")
                   && migrated.runs.first().vm.config.sha256
                       == VmValidationStore::fileSha256(fixture.config),
               QStringLiteral("legacy schema migrates deterministically in memory: %1").arg(error));
    StoreSnapshot secondLoad;
    test.check(store.load(&secondLoad, &error)
                   && secondLoad.revision == migrated.revision
                   && secondLoad.runs.first().id == migrated.runs.first().id,
               QStringLiteral("legacy migration produces stable ids and revision"));

    RunUpdate migrateWrite;
    migrateWrite.logs = {LogDraft{QDateTime::currentDateTimeUtc().addSecs(-30),
                                  QStringLiteral("migration"), QStringLiteral("upgraded")}};
    MutationResult upgraded;
    test.check(store.updateRun(migrated.runs.first().id, migrated.runs.first().revision,
                               migrateWrite, &upgraded, &error, migrated.revision),
               QStringLiteral("first mutation writes migrated schema: %1").arg(error));
    const QJsonDocument current = QJsonDocument::fromJson(readFile(store.stateFilePath()));
    test.check(current.object().value(QStringLiteral("version")).toInt()
                       == VmValidationStore::CurrentVersion
                   && current.object().value(QStringLiteral("revision")).toString().size() == 64,
               QStringLiteral("migration persists current deterministic schema and revision"));

    RunUpdate oversizedEntry;
    oversizedEntry.logs = {LogDraft{QDateTime::currentDateTimeUtc(), QStringLiteral("provider"),
                                    QString(VmValidationStore::MaxLogEntryBytes + 1,
                                            QLatin1Char('x'))}};
    test.check(!store.updateRun(upgraded.run.id, upgraded.run.revision,
                                oversizedEntry, nullptr, &error)
                   && error.contains(QStringLiteral("per-entry")),
               QStringLiteral("oversized individual log entry is rejected"));

    RunUpdate oversizedTotal;
    for (int index = 0; index < 18; ++index) {
        oversizedTotal.logs.append(LogDraft{
            QDateTime::currentDateTimeUtc(), QStringLiteral("provider"),
            QString(60 * 1024, QLatin1Char('y'))});
    }
    test.check(!store.updateRun(upgraded.run.id, upgraded.run.revision,
                                oversizedTotal, nullptr, &error)
                   && error.contains(QStringLiteral("per-run")),
               QStringLiteral("aggregate per-run log bound is enforced"));

    RunUpdate tooManyMilestones;
    for (qsizetype index = 0; index < VmValidationStore::MaxMilestonesPerRun + 1; ++index) {
        tooManyMilestones.milestones.append(MilestoneDraft{
            MilestonePhase::Boot, QStringLiteral("milestone-%1").arg(index),
            MilestoneStatus::Reached, QDateTime::currentDateTimeUtc(), {}, {}});
    }
    test.check(!store.updateRun(upgraded.run.id, upgraded.run.revision,
                                tooManyMilestones, nullptr, &error)
                   && error.contains(QStringLiteral("too many milestones")),
               QStringLiteral("milestone count bound is enforced"));

    Fixture largeConfigFixture(root, QStringLiteral("large-config"));
    VmValidationStore largeConfigStore(largeConfigFixture.project);
    RunStart largeConfig = largeConfigFixture.start();
    largeConfig.configSnapshot = QJsonObject{
        {QStringLiteral("payload"),
         QString(VmValidationStore::MaxConfigBytes + 1, QLatin1Char('z'))},
    };
    test.check(!largeConfigStore.appendRun(largeConfig, nullptr, &error)
                   && error.contains(QStringLiteral("snapshot")),
               QStringLiteral("configuration snapshot size bound is enforced"));

    Fixture oversizedStoreFixture(root, QStringLiteral("large-store"));
    VmValidationStore oversizedStore(oversizedStoreFixture.project);
    test.check(writeFile(oversizedStore.stateFilePath(),
                         QByteArray(VmValidationStore::MaxStoreBytes + 1, 'x')),
               QStringLiteral("oversized store fixture is written"));
    StoreSnapshot rejected;
    test.check(!oversizedStore.load(&rejected, &error)
                   && error.contains(QStringLiteral("size limit")),
               QStringLiteral("oversized persisted store is rejected before JSON parsing"));
}

void testFailedAndCancelledNotes(TestRun &test, const QString &root)
{
    Fixture fixture(root, QStringLiteral("outcomes"));
    VmValidationStore store(fixture.project);
    QString error;
    MutationResult failed;
    test.check(store.appendRun(fixture.start(QStringLiteral("failed")), &failed, &error),
               QStringLiteral("failed outcome fixture starts"));
    test.check(!store.completeRun(failed.run.id, failed.run.revision, RunStatus::Failed,
                                  {}, {}, nullptr, &error)
                   && error.contains(QStringLiteral("note")),
               QStringLiteral("failed outcome requires a note"));
    MutationResult failedDone;
    test.check(store.completeRun(failed.run.id, failed.run.revision, RunStatus::Failed,
                                 QStringLiteral("Guest did not reach WinPE."), {},
                                 &failedDone, &error),
               QStringLiteral("failed outcome records its note"));

    MutationResult cancelled;
    test.check(store.appendRun(fixture.start(QStringLiteral("cancelled")), &cancelled, &error),
               QStringLiteral("cancel outcome fixture starts"));
    test.check(!store.completeRun(cancelled.run.id, cancelled.run.revision,
                                  RunStatus::Cancelled, {}, {}, nullptr, &error),
               QStringLiteral("cancelled outcome requires a note"));
    MutationResult cancelledDone;
    test.check(store.completeRun(cancelled.run.id, cancelled.run.revision,
                                 RunStatus::Cancelled,
                                 QStringLiteral("Operator stopped the provider."), {},
                                 &cancelledDone, &error),
               QStringLiteral("cancelled outcome records its note"));

    RunFilter failures;
    failures.status = RunStatus::Failed;
    test.check(store.history(failures, &error).size() == 1,
               QStringLiteral("history filters completed status"));
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporary;
    TestRun test;
    test.check(temporary.isValid(), QStringLiteral("temporary test root is available"));
    if (!temporary.isValid())
        return test.result();

    testLifecycleAndPortableEvidence(test, temporary.path());
    testProfileAwarePassGates(test, temporary.path());
    testPathSafetyAndExternalEvidence(test, temporary.path());
    testCasConcurrencyAndFiltering(test, temporary.path());
    testCorruptionRecoveryAndRollback(test, temporary.path());
    testMigrationAndBounds(test, temporary.path());
    testFailedAndCancelledNotes(test, temporary.path());
    return test.result();
}
