#pragma once

#include <QObject>
#include <QStringList>
#include <QList>
#include "dashboardwidget.h"

class CommandRunner;

class PackageManager : public QObject
{
    Q_OBJECT

public:
    explicit PackageManager(CommandRunner* runner, QObject* parent = nullptr);

    // High-level operations
    void checkSystemUpdates();
    void installSystemUpdates(bool offlineUpdate, bool autoCleanCache, int oldVersionsToKeep);
    void cancelScheduledUpdate();
    void fetchPackageList(DashboardWidget::PackageFilter filter);

    // Utilities
    void runRawCommand(const QString& cmd, const QString& desc);
    void cleanCache(int oldVersionsToKeep);
    void clearAllCache();
    void deleteCachedPackage(const QString& fileName);
    void repairKeyring();

    // Yay & AUR Tooling
    void installYay(const QString& variant);
    void uninstallYay();
    void updateAur();
    void cleanAurLeftovers();
    void installOfflineUpdater();
    void installOfflineUpdaterManual();

signals:
    void updatesCheckFinished(const QList<UpdatePackageInfo>& updates, bool errorFound);
    void packageListFetched(const QStringList& packages);
    void operationFinished(bool success, bool cancelled);
    void statusMessageChanged(const QString& message);

private:
    bool isCancelled(int exitCode);

    CommandRunner* m_runner;
};
