#pragma once

#include <QMainWindow>
#include <QStringList>
#include <QDateTime>
#include <QList>
#include <functional>
#include <QMetaObject>
#include "dashboardwidget.h"

class QStackedWidget;
class ButtonPanel;
class TerminalWindow;
class QSettings;
class PacmanConfigManager;
class ReflectorManager;
class CommandRunner;
class PackageManager;
class QMenu;
class QAction;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Core Actions
    void onCheckButtonClicked();
    void onSystemUpdate();
    void onRebootSystem();
    void onToggleView();
    void onShowAboutDialog();

    // Package & Cache Management
    void onCleanPacmanCache();
    void onDeleteCachedPackage(const QString& fileName);
    void onShowInstalledPackages();
    void onFilterChanged(int filter);
    void onCriticalPackageToggled(const QString& name, bool isCritical);
    void resetCriticalPackages();

    // Settings & Scripts
    void onSetupOfflineUpdates();
    void onToggleOfflineUpdates(bool checked);
    void onRunScriptFromUrl();
    void onRunLocalScript();

private:
    // --- 1.0 Modular Setup Methods ---
    void setupUI();
    void setupConnections();
    void setupInitialState();

    // Menu Bar Builders
    void createMenuBar();
    void createPacmanMenu();
    void createPackagesMenu();
    void createYayMenu();
    void createSettingsMenu();

    void setupYay();
    void setupPacmanMiscMenu();
    void updateMenuState();

    // --- Core Logic ---
    void runPackageTask(const QString& busyMessage, bool requiresTerminal, std::function<void()> task, std::function<void()> onFinish = nullptr);
    void handleSystemUpdateCheckResult(const QList<UpdatePackageInfo>& updates, bool error);
    void resetSystemUpdateState(const QString& message = QString());
    void updateDashboardTimestamps();
    void initializeDashboardState();

    void loadSettings();
    void saveSettings();
    void loadCriticalPackages();
    void saveCriticalPackages();
    void fetchPackageList(int filter);
    void returnToDashboard();
    void restoreDashboardState();
    void switchToTerminal(bool autoSwitch);
    void updateCheckButtonState();

    // --- UI Components ---
    ButtonPanel *m_buttonPanel;
    QStackedWidget *m_stack;
    DashboardWidget *m_dashboardWidget;
    TerminalWindow *m_terminalWindow;
    QPushButton *m_toggleViewButton;

    // --- Managers ---
    CommandRunner *m_runner;
    PackageManager *m_packageManager;
    PacmanConfigManager *m_pacmanConfigManager;
    ReflectorManager *m_reflectorManager;
    QSettings *m_settings;

    // --- Menu Actions ---
    QMenu *m_pacmanMenu;
    QMenu *m_yayMenu;
    QMenu *m_settingsMenu;
    QMenu *m_yayInstallMenu;

    QAction *m_cleanCacheAction;
    QAction *m_clearAllCacheAction;
    QAction *m_yayUpdateAction;
    QAction *m_yayCleanAction;
    QAction *m_yayInstallBinAction;
    QAction *m_yayInstallSourceAction;
    QAction *m_yayInstallGithubAction;
    QAction *m_yayUninstallAction;
    QAction *m_setupOfflineAction;
    QAction *m_toggleOfflineAction;

    // --- State Tracking ---
    enum class UpdateState { Idle, Checking, UpdatesAvailable };
    UpdateState m_updateState;

    int m_updateCount;
    int m_cachedCriticalCount;
    int m_currentFilter;
    int m_oldVersionsToKeep;

    bool m_autoSwitchedToTerminal;
    bool m_verifyUpgrade;
    bool m_viewingPackageList;
    bool m_hasCheckedThisSession;
    bool m_lastCheckFailed;
    bool m_rebootPending;
    bool m_autoCleanCache;
    bool m_checkOnStartup;
    bool m_offlineUpdateEnabled;

    QStringList m_criticalPackages;
    QList<UpdatePackageInfo> m_cachedUpdates;
    QDateTime m_lastCheckedTime;
    QDateTime m_lastUpgradedTime;
};
