#include "mainwindow.h"
#include "buttonpanel.h"
#include "terminalwindow.h"
#include "dashboardwidget.h"
#include "commandrunner.h"
#include "packagemanager.h"
#include "depcheck.h"
#include "aboutdialog.h"
#include "pacmanconfigmanager.h"
#include "reflectormanager.h"

#include <QVBoxLayout>
#include <QMenuBar>
#include <QInputDialog>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QWidgetAction>
#include <QSpinBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>
#include <QSettings>
#include <QStandardPaths>
#include <QMessageBox>
#include <QCloseEvent>
#include <QStackedWidget>
#include <QPushButton>

static const QStringList DEFAULT_CRITICAL_PACKAGES = {
    "linux", "linux-lts", "linux-zen", "linux-hardened", "linux-firmware",
    "amd-ucode", "intel-ucode", "systemd", "systemd-libs", "systemd-sysvcompat",
    "mkinitcpio", "grub", "efibootmgr", "glibc", "pacman", "archlinux-keyring",
    "coreutils", "bash", "sudo", "filesystem", "openssl", "gnutls", "ca-certificates",
    "networkmanager", "iproute2", "btrfs-progs", "e2fsprogs", "dosfstools", "xfsprogs",
    "ntfs-3g", "nvidia", "nvidia-lts", "nvidia-dkms", "nvidia-utils", "mesa",
    "vulkan-radeon", "vulkan-intel", "vulkan-icd-loader", "xf86-video-amdgpu",
    "xf86-video-intel", "xf86-video-nouveau", "xf86-video-vmware"
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
m_updateState(UpdateState::Idle), m_updateCount(0), m_cachedCriticalCount(0),
m_currentFilter(1), m_autoSwitchedToTerminal(false), m_verifyUpgrade(false),
m_viewingPackageList(false), m_hasCheckedThisSession(false), m_lastCheckFailed(false),
m_rebootPending(false)
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    m_settings = new QSettings(QDir(configPath).filePath("uptater.conf"), QSettings::IniFormat, this);

    loadSettings();

    m_terminalWindow = new TerminalWindow(this);
    m_runner = new CommandRunner(m_terminalWindow, this);
    m_packageManager = new PackageManager(m_runner, this);
    m_pacmanConfigManager = new PacmanConfigManager(this);
    m_reflectorManager = new ReflectorManager(this);

    setupUI();
    setupConnections();
    createMenuBar();

    setupInitialState();
    resetSystemUpdateState();
    initializeDashboardState();

    if (m_checkOnStartup && !m_rebootPending) {
        QTimer::singleShot(500, this, &MainWindow::onCheckButtonClicked);
    }
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_buttonPanel = new ButtonPanel(centralWidget);
    mainLayout->addWidget(m_buttonPanel, 0);

    m_stack = new QStackedWidget(centralWidget);
    m_dashboardWidget = new DashboardWidget(centralWidget);
    m_stack->addWidget(m_dashboardWidget);
    m_stack->addWidget(m_terminalWindow);
    m_stack->setCurrentIndex(0);
    mainLayout->addWidget(m_stack, 1);

    m_toggleViewButton = new QPushButton("Show Terminal Output", centralWidget);
    m_toggleViewButton->setFixedHeight(30);
    m_toggleViewButton->setCursor(Qt::PointingHandCursor);
    m_toggleViewButton->setStyleSheet(
        "QPushButton { background-color: #333; color: #DDD; border: none; border-top: 1px solid #555; font-weight: bold; }"
        "QPushButton:hover { background-color: #444; }"
    );
    mainLayout->addWidget(m_toggleViewButton, 0);
}

void MainWindow::setupConnections()
{
    // Runner State
    connect(m_runner, &CommandRunner::commandStarted, this, [this](){
        m_buttonPanel->setEnabled(false);
        menuBar()->setEnabled(false);
        m_terminalWindow->setInputEnabled(m_stack->currentIndex() == 1);
    });

    connect(m_runner, &CommandRunner::terminalReady, this, [this](){
        if (m_stack->currentIndex() == 1) m_terminalWindow->setFocusToTerminal();
    });

        connect(m_runner, &CommandRunner::commandFinished, this, [this](){
            m_terminalWindow->setInputEnabled(false);
            m_buttonPanel->setEnabled(true);
            menuBar()->setEnabled(true);
            if (m_autoSwitchedToTerminal) {
                if (m_stack->currentIndex() == 1) onToggleView();
                m_autoSwitchedToTerminal = false;
            }
            setupInitialState();
        });

        // Package Manager
        connect(m_packageManager, &PackageManager::updatesCheckFinished, this, &MainWindow::handleSystemUpdateCheckResult);
        connect(m_packageManager, &PackageManager::packageListFetched, this, [this](const QStringList& lines){
            m_dashboardWidget->showInstalledList(lines, m_criticalPackages, static_cast<DashboardWidget::PackageFilter>(m_currentFilter));
        });

        // UI Components
        connect(m_buttonPanel, &ButtonPanel::checkClicked, this, &MainWindow::onCheckButtonClicked);
        connect(m_buttonPanel, &ButtonPanel::installClicked, this, &MainWindow::onSystemUpdate);
        connect(m_toggleViewButton, &QPushButton::clicked, this, &MainWindow::onToggleView);

        connect(m_dashboardWidget, &DashboardWidget::criticalPackageToggled, this, &MainWindow::onCriticalPackageToggled);
        connect(m_dashboardWidget, &DashboardWidget::rebootClicked, this, &MainWindow::onRebootSystem);
        connect(m_dashboardWidget, &DashboardWidget::deleteCachedPackageRequested, this, &MainWindow::onDeleteCachedPackage);
        connect(m_dashboardWidget, &DashboardWidget::filterChanged, this, [this](DashboardWidget::PackageFilter f){ onFilterChanged(static_cast<int>(f)); });

        // Managers
        connect(m_pacmanConfigManager, &PacmanConfigManager::commandRequested, this, [this](const QString& cmd, const QString& desc){
            runPackageTask(desc, false, [this, cmd, desc](){ m_packageManager->runRawCommand(cmd, desc); });
        });

        connect(m_reflectorManager, &ReflectorManager::commandRequested, this, [this](const QString& cmd, const QString& desc){
            runPackageTask(desc, false, [this, cmd, desc](){ m_packageManager->runRawCommand(cmd, desc); });
        });

        connect(m_reflectorManager, &ReflectorManager::menuActionCompleted, this, [this](){
            if(m_pacmanMenu) m_pacmanMenu->hide();
        });
}

// --- Menu Bar Creation ---

void MainWindow::createMenuBar()
{
    auto *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Run Script from &URL...", this, &MainWindow::onRunScriptFromUrl);
    fileMenu->addAction("Run &Local Script...", this, &MainWindow::onRunLocalScript);

    createPacmanMenu();
    createPackagesMenu();
    createYayMenu();
    createSettingsMenu();
}

void MainWindow::createPacmanMenu()
{
    m_pacmanMenu = menuBar()->addMenu("&Pacman");
    setupPacmanMiscMenu();
    m_pacmanMenu->addSeparator();
    m_pacmanMenu->addActions(m_reflectorManager->getActions());
    m_pacmanMenu->addSeparator();

    m_pacmanMenu->addAction("Repair Keyring", this, [this](){
        runPackageTask("Repairing Pacman Keyring...", false, [this](){ m_packageManager->repairKeyring(); });
    });
}

void MainWindow::createPackagesMenu()
{
    auto* packagesMenu = menuBar()->addMenu("&Packages");
    packagesMenu->addAction("Show Installed", this, &MainWindow::onShowInstalledPackages);
    packagesMenu->addSeparator();

    auto* cacheMenu = packagesMenu->addMenu("&Cache");
    cacheMenu->addAction("Show Cached Packages", this, [this](){ fetchPackageList(4); });
    cacheMenu->addSeparator();

    auto* cleanAction = cacheMenu->addAction("Automatically Clean Cache");
    cleanAction->setCheckable(true);
    cleanAction->setChecked(m_autoCleanCache);
    connect(cleanAction, &QAction::toggled, this, [this](bool c){ m_autoCleanCache = c; saveSettings(); });

    auto* widget = new QWidget();
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(15, 2, 2, 2);
    layout->addWidget(new QLabel("Old Versions to Keep:"));
    auto* spinBox = new QSpinBox();
    spinBox->setMinimum(0); spinBox->setMaximum(10); spinBox->setValue(m_oldVersionsToKeep);
    connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value){ m_oldVersionsToKeep = value; saveSettings(); });
    layout->addWidget(spinBox);

    auto* widgetAction = new QWidgetAction(cacheMenu);
    widgetAction->setDefaultWidget(widget);
    cacheMenu->addAction(widgetAction);
    cacheMenu->addSeparator();

    m_cleanCacheAction = cacheMenu->addAction("Clean Package Cache Now", this, &MainWindow::onCleanPacmanCache);
    cacheMenu->addSeparator();

    m_clearAllCacheAction = cacheMenu->addAction("Clear All Package Cache");
    QFont boldFont = m_clearAllCacheAction->font(); boldFont.setBold(true);
    m_clearAllCacheAction->setFont(boldFont);

    connect(m_clearAllCacheAction, &QAction::triggered, this, [this](){
        if (QMessageBox::warning(this, "Clear All Package Cache", "Are you sure you want to completely clear the package cache?\n\nYou will not be able to downgrade packages locally.", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            runPackageTask("Clearing all package cache...", false, [this](){ m_packageManager->clearAllCache(); }, [this](){
                if (m_viewingPackageList && m_currentFilter == 4) fetchPackageList(4);
            });
        }
    });
}

void MainWindow::createYayMenu()
{
    m_yayMenu = menuBar()->addMenu("&Yay");

    m_yayUpdateAction = m_yayMenu->addAction("Update AUR Packages", this, [this](){ runPackageTask("", true, [this](){ m_packageManager->updateAur(); }); });
    m_yayCleanAction = m_yayMenu->addAction("Clean Leftovers (yay -Yc)", this, [this](){ runPackageTask("", true, [this](){ m_packageManager->cleanAurLeftovers(); }); });
    m_yayMenu->addSeparator();

    m_yayInstallMenu = m_yayMenu->addMenu("Install Yay");
    m_yayInstallBinAction = m_yayInstallMenu->addAction("AUR (Pre-Compiled)", this, [this](){ runPackageTask("Installing Yay...", false, [this](){ m_packageManager->installYay("yay-bin"); }, [this](){ setupYay(); }); });
    m_yayInstallSourceAction = m_yayInstallMenu->addAction("AUR (Source)", this, [this](){ runPackageTask("Installing Yay...", false, [this](){ m_packageManager->installYay("yay"); }, [this](){ setupYay(); }); });
    m_yayInstallGithubAction = m_yayInstallMenu->addAction("GitHub (Fallback)", this, [this](){ runPackageTask("Installing Yay...", false, [this](){ m_packageManager->installYay("github"); }, [this](){ setupYay(); }); });

    m_yayUninstallAction = m_yayMenu->addAction("Uninstall Yay", this, [this](){ runPackageTask("Uninstalling Yay...", false, [this](){ m_packageManager->uninstallYay(); }, [this](){ setupYay(); }); });
}

void MainWindow::createSettingsMenu()
{
    m_settingsMenu = menuBar()->addMenu("&Settings");

    auto* checkAction = m_settingsMenu->addAction("Check for Updates on Launch");
    checkAction->setCheckable(true);
    checkAction->setChecked(m_checkOnStartup);
    connect(checkAction, &QAction::toggled, this, [this](bool c){ m_checkOnStartup = c; saveSettings(); });

    m_setupOfflineAction = m_settingsMenu->addAction("Setup Updates on Reboot", this, &MainWindow::onSetupOfflineUpdates);
    m_toggleOfflineAction = m_settingsMenu->addAction("Install Updates on Reboot");
    m_toggleOfflineAction->setCheckable(true);
    connect(m_toggleOfflineAction, &QAction::toggled, this, &MainWindow::onToggleOfflineUpdates);

    updateMenuState();
    m_settingsMenu->addAction("Reset Critical Package List", this, &MainWindow::resetCriticalPackages);
    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction("&About", this, &MainWindow::onShowAboutDialog);
}

// --- Core App Logic ---

void MainWindow::runPackageTask(const QString& busyMessage, bool requiresTerminal, std::function<void()> task, std::function<void()> onFinish)
{
    if (requiresTerminal) switchToTerminal(true);
    else if (m_stack->currentIndex() == 0 && !m_viewingPackageList && !busyMessage.isEmpty()) {
        m_dashboardWidget->showBusyState(busyMessage);
    }

    QMetaObject::Connection *conn = new QMetaObject::Connection;
    *conn = connect(m_packageManager, &PackageManager::operationFinished, this, [this, onFinish, conn](bool success, bool cancelled){
        QObject::disconnect(*conn);
        delete conn;

        if (cancelled) {
            m_dashboardWidget->showOperationCancelled();
            QTimer::singleShot(2500, this, [this](){ if (!m_viewingPackageList) restoreDashboardState(); });
        } else if (!success) {
            m_dashboardWidget->showErrorState();
            updateCheckButtonState();
        } else {
            if (onFinish) onFinish();
            if (!m_viewingPackageList) restoreDashboardState();
        }
    });

    task();
}

void MainWindow::updateCheckButtonState()
{
    if (m_stack->currentIndex() == 1) {
        m_buttonPanel->setCheckText("Check for Updates");
        m_buttonPanel->setCheckIcon(QIcon::fromTheme("view-refresh"));
    }
    else if (m_viewingPackageList) {
        m_buttonPanel->setCheckText("Return to Updates");
        m_buttonPanel->setCheckIcon(QIcon::fromTheme("go-previous"));
    }
    else if (m_rebootPending) {
        m_buttonPanel->setCheckText("Cancel Updates");
        m_buttonPanel->setCheckIcon(QIcon::fromTheme("edit-delete"));
    }
    else {
        m_buttonPanel->setCheckText("Check for Updates");
        m_buttonPanel->setCheckIcon(QIcon::fromTheme("view-refresh"));
    }
}

void MainWindow::onCheckButtonClicked()
{
    if (m_runner->isBusy()) return;

    if (m_viewingPackageList && m_stack->currentIndex() == 0) {
        returnToDashboard();
        return;
    }

    if (m_rebootPending && m_stack->currentIndex() == 0) {
        runPackageTask("Cancelling scheduled update...", false, [this](){ m_packageManager->cancelScheduledUpdate(); }, [this](){ m_rebootPending = false; });
        return;
    }

    if (m_stack->currentIndex() == 0 && !m_viewingPackageList) {
        m_dashboardWidget->showBusyState("Checking for system updates...");
    }
    m_packageManager->checkSystemUpdates();
}

void MainWindow::returnToDashboard()
{
    m_viewingPackageList = false;
    restoreDashboardState();
}

void MainWindow::restoreDashboardState()
{
    if (m_cleanCacheAction) m_cleanCacheAction->setEnabled(!m_rebootPending);
    if (m_clearAllCacheAction) m_clearAllCacheAction->setEnabled(!m_rebootPending);

    if (m_rebootPending) {
        m_updateState = UpdateState::Idle;
        m_dashboardWidget->showRebootReadyState();
        m_buttonPanel->setUpdateEnabled(false);
    }
    else if (m_updateCount > 0 && !m_cachedUpdates.isEmpty()) {
        m_updateState = UpdateState::UpdatesAvailable;
        m_dashboardWidget->showUpdatesAvailable(m_cachedUpdates, m_criticalPackages, m_cachedCriticalCount);

        QString txt = (m_offlineUpdateEnabled && DepCheck::systemUpdatePacmanInstalled())
        ? QString("Download %1 Updates && Install Next Reboot").arg(m_updateCount)
        : QString("Download && Install %1 Updates").arg(m_updateCount);
        m_buttonPanel->setUpdateText(txt);
        m_buttonPanel->setUpdateEnabled(true);
    }
    else if (m_lastCheckFailed) {
        m_updateState = UpdateState::Idle;
        m_dashboardWidget->showErrorState();
        m_buttonPanel->setUpdateEnabled(false);
    }
    else if (m_hasCheckedThisSession) {
        m_updateState = UpdateState::Idle;
        m_dashboardWidget->showUpToDate();
        m_buttonPanel->setUpdateEnabled(false);
    }
    else {
        m_updateState = UpdateState::Idle;
        m_dashboardWidget->showStatusUnknown();
        m_buttonPanel->setUpdateEnabled(false);
    }
    updateCheckButtonState();
}

void MainWindow::onSystemUpdate()
{
    if (m_updateState != UpdateState::UpdatesAvailable) return;

    bool isOffline = m_offlineUpdateEnabled && DepCheck::systemUpdatePacmanInstalled();
    if (m_stack->currentIndex() == 0) m_dashboardWidget->showBusyState(isOffline ? "Downloading updates..." : "Installing updates...");

    QMetaObject::Connection *conn = new QMetaObject::Connection;
    *conn = connect(m_packageManager, &PackageManager::operationFinished, this, [this, isOffline, conn](bool success, bool cancelled){
        QObject::disconnect(*conn);
        delete conn;

        if (cancelled) {
            m_dashboardWidget->showOperationCancelled();
            QTimer::singleShot(2500, this, [this](){ restoreDashboardState(); });
        } else if (!success) {
            m_dashboardWidget->showErrorState();
            updateCheckButtonState();
        } else {
            if (isOffline) {
                m_lastUpgradedTime = QDateTime::currentDateTime();
                m_settings->setValue("stats/lastUpgraded", m_lastUpgradedTime);
                updateDashboardTimestamps();
                m_rebootPending = true;
                restoreDashboardState();
            } else {
                resetSystemUpdateState("Verifying...");
                m_verifyUpgrade = true;
                m_packageManager->checkSystemUpdates();
            }
        }
    });

    m_packageManager->installSystemUpdates(isOffline, m_autoCleanCache, m_oldVersionsToKeep);
}

void MainWindow::handleSystemUpdateCheckResult(const QList<UpdatePackageInfo>& updates, bool error)
{
    m_hasCheckedThisSession = true;
    m_lastCheckFailed = error;
    m_lastCheckedTime = QDateTime::currentDateTime();
    m_settings->setValue("stats/lastChecked", m_lastCheckedTime);
    updateDashboardTimestamps();

    m_viewingPackageList = false;
    updateCheckButtonState();

    m_cachedUpdates = updates;
    m_cachedCriticalCount = 0;
    for (const auto& pkg : updates) {
        if (m_criticalPackages.contains(pkg.name)) m_cachedCriticalCount++;
    }
    m_updateCount = updates.size();

    if (m_updateCount > 0) {
        m_updateState = UpdateState::UpdatesAvailable;
        restoreDashboardState();
        m_verifyUpgrade = false;
    }
    else if (error) {
        resetSystemUpdateState("Error checking for updates");
        m_dashboardWidget->showErrorState();
        m_verifyUpgrade = false;
    }
    else {
        resetSystemUpdateState();
        m_dashboardWidget->showUpToDate();
        if (m_verifyUpgrade) {
            m_lastUpgradedTime = QDateTime::currentDateTime();
            m_settings->setValue("stats/lastUpgraded", m_lastUpgradedTime);
            updateDashboardTimestamps();
            m_dashboardWidget->showUpToDate();
            m_verifyUpgrade = false;
        }
    }
}

void MainWindow::resetSystemUpdateState(const QString& message)
{
    m_updateState = UpdateState::Idle;
    m_buttonPanel->setUpdateEnabled(false);

    QString text = (m_offlineUpdateEnabled && DepCheck::systemUpdatePacmanInstalled())
    ? "Download Updates && Install Next Reboot"
    : "Download && Install Updates";

    if (!message.isEmpty()) {
        m_buttonPanel->setUpdateText(message);
        QTimer::singleShot(3000, this, [this, text](){
            if (m_updateState == UpdateState::Idle) m_buttonPanel->setUpdateText(text);
        });
    } else {
        m_buttonPanel->setUpdateText(text);
    }
}

// --- Menu Handlers & Utilities ---

void MainWindow::onCleanPacmanCache() { runPackageTask("Cleaning pacman cache...", false, [this](){ m_packageManager->cleanCache(m_oldVersionsToKeep); }); }
void MainWindow::onShowAboutDialog() { AboutDialog(this).exec(); }
void MainWindow::onShowInstalledPackages() { fetchPackageList(1); }
void MainWindow::onFilterChanged(int filter) { fetchPackageList(filter); }

void MainWindow::onDeleteCachedPackage(const QString& fileName) {
    if (m_rebootPending) {
        QMessageBox::warning(this, "Action Blocked", "Cannot delete cached packages while an offline update is pending.");
        return;
    }
    runPackageTask("", false, [this, fileName](){ m_packageManager->deleteCachedPackage(fileName); }, [this](){
        if (m_viewingPackageList && m_currentFilter == 4) fetchPackageList(4);
    });
}

void MainWindow::fetchPackageList(int filter) {
    m_currentFilter = filter;
    m_viewingPackageList = true;
    updateCheckButtonState();
    if (m_stack->currentIndex() == 0) m_dashboardWidget->showBusyState("Listing packages...");
    m_packageManager->fetchPackageList(static_cast<DashboardWidget::PackageFilter>(filter));
}

void MainWindow::onCriticalPackageToggled(const QString& name, bool isCritical) {
    if (isCritical) { if (!m_criticalPackages.contains(name)) m_criticalPackages.append(name); }
    else { m_criticalPackages.removeAll(name); }
    saveCriticalPackages();
}

void MainWindow::loadCriticalPackages() {
    m_criticalPackages = m_settings->value("updates/criticalPackages").toStringList();
    if (m_criticalPackages.isEmpty()) m_criticalPackages = DEFAULT_CRITICAL_PACKAGES;
}

void MainWindow::saveCriticalPackages() { m_settings->setValue("updates/criticalPackages", m_criticalPackages); }

void MainWindow::resetCriticalPackages() {
    if (QMessageBox::question(this, "Reset Critical List", "Reset list to default?", QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes) {
        m_criticalPackages = DEFAULT_CRITICAL_PACKAGES;
        saveCriticalPackages();
        QMessageBox::information(this, "Reset", "Critical packages list reset.");
    }
}

void MainWindow::updateDashboardTimestamps() { m_dashboardWidget->setTimestamps(m_lastUpgradedTime); }

void MainWindow::initializeDashboardState() {
    m_lastCheckedTime = m_settings->value("stats/lastChecked").toDateTime();
    m_lastUpgradedTime = m_settings->value("stats/lastUpgraded").toDateTime();
    updateDashboardTimestamps();
    m_rebootPending = QFileInfo("/system-update").isSymLink() || QFile::exists("/system-update");
    restoreDashboardState();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_runner->isBusy()) {
        if (QMessageBox::warning(this, "Operation in Progress", "Closing now may corrupt your system.\nIf you are installing updates, you may need to manually remove /var/lib/pacman/db.lck later.\nForce Close?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void MainWindow::onToggleView() {
    m_autoSwitchedToTerminal = false;
    if (m_stack->currentIndex() == 0) {
        m_stack->setCurrentIndex(1);
        m_toggleViewButton->setText("Hide Terminal Output");
        m_terminalWindow->setFocusToTerminal();
    } else {
        m_stack->setCurrentIndex(0);
        m_toggleViewButton->setText("Show Terminal Output");
    }
    updateCheckButtonState();
}

void MainWindow::switchToTerminal(bool autoSwitch) {
    if (m_stack->currentIndex() == 0) {
        m_stack->setCurrentIndex(1);
        m_toggleViewButton->setText("Hide Terminal Output");
        m_terminalWindow->setFocusToTerminal();
        if (autoSwitch) m_autoSwitchedToTerminal = true;
    } else {
        m_autoSwitchedToTerminal = false;
    }
    updateCheckButtonState();
}

void MainWindow::onRunScriptFromUrl() {
    bool ok;
    QString url = QInputDialog::getText(this, "Run Script from URL", "Enter URL:", QLineEdit::Normal, "", &ok);
    if (ok && !url.isEmpty()) {
        runPackageTask("", true, [this, url](){
            QString escapedUrl = url;
            escapedUrl.replace("'", "'\\''");
            m_packageManager->runRawCommand(QString("curl -sSL '%1' | bash").arg(escapedUrl), "Running script from URL...");
        });
    }
}

void MainWindow::onRunLocalScript() {
    QString filePath = QFileDialog::getOpenFileName(this, "Open Script", QDir::homePath(), "Shell Scripts (*.sh)");
    if (!filePath.isEmpty()) {
        runPackageTask("", true, [this, filePath](){
            QString escapedPath = filePath;
            escapedPath.replace("'", "'\\''");
            m_packageManager->runRawCommand("bash '" + escapedPath + "'", "Running local script: " + QFileInfo(filePath).fileName());
        });
    }
}

void MainWindow::updateMenuState() {
    bool toolInstalled = DepCheck::systemUpdatePacmanInstalled();
    m_setupOfflineAction->setVisible(!toolInstalled);
    m_toggleOfflineAction->setVisible(toolInstalled);
    if (toolInstalled) m_toggleOfflineAction->setChecked(m_offlineUpdateEnabled);
    else if (m_offlineUpdateEnabled) { m_offlineUpdateEnabled = false; saveSettings(); }
}

void MainWindow::onSetupOfflineUpdates() {
    runPackageTask("Installing offline update tool...", false, [this](){
        if (DepCheck::yayInstalled()) m_packageManager->installOfflineUpdater();
        else m_packageManager->installOfflineUpdaterManual();
    }, [this](){ updateMenuState(); });
}

void MainWindow::onToggleOfflineUpdates(bool checked) {
    m_offlineUpdateEnabled = checked;
    saveSettings();

    if (m_updateState == UpdateState::UpdatesAvailable) {
        restoreDashboardState();
    } else {
        resetSystemUpdateState();
    }
}

void MainWindow::onRebootSystem() { QProcess::startDetached("systemctl", {"reboot"}); }

void MainWindow::setupInitialState() {
    loadCriticalPackages();
    setupYay();
    m_reflectorManager->setup();
    updateMenuState();
}

void MainWindow::setupYay() {
    bool yayInstalled = DepCheck::yayInstalled();
    if (m_yayInstallMenu) m_yayInstallMenu->setTitle(yayInstalled ? "Reinstall Yay" : "Install Yay");
    if (m_yayUninstallAction) m_yayUninstallAction->setVisible(yayInstalled);
    if (m_yayUpdateAction) m_yayUpdateAction->setVisible(yayInstalled);
    if (m_yayCleanAction) m_yayCleanAction->setVisible(yayInstalled);
}

void MainWindow::setupPacmanMiscMenu() {
    auto* miscMenu = m_pacmanMenu->addMenu("&Miscellaneous");
    for (const QString& option : PACMAN_TOGGLE_OPTIONS) {
        auto* action = miscMenu->addAction(option);
        action->setCheckable(true);
        action->setChecked(m_pacmanConfigManager->isOptionEnabled(option));
        connect(action, &QAction::triggered, this, [this, option](){ m_pacmanConfigManager->toggleOption(option); });
    }
    auto* parallelMenu = m_pacmanMenu->addMenu("&Parallel Downloads");
    auto* enableAction = parallelMenu->addAction("Enable");
    enableAction->setCheckable(true);
    enableAction->setChecked(m_pacmanConfigManager->isOptionEnabled("ParallelDownloads"));
    connect(enableAction, &QAction::triggered, this, [this](bool checked){ m_pacmanConfigManager->enableParallelDownloads(checked); });

    auto* widget = new QWidget();
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(15, 2, 2, 2);
    layout->addWidget(new QLabel("Threads:"));
    auto* spinBox = new QSpinBox();
    spinBox->setMinimum(1); spinBox->setValue(m_pacmanConfigManager->getParallelDownloadsCount());
    connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value){ m_pacmanConfigManager->setParallelDownloadsCount(value); });
    layout->addWidget(spinBox);

    auto* widgetAction = new QWidgetAction(parallelMenu);
    widgetAction->setDefaultWidget(widget);
    parallelMenu->addAction(widgetAction);
}

void MainWindow::loadSettings() {
    m_oldVersionsToKeep = m_settings->value("cache/oldVersionsToKeep", 1).toInt();
    m_autoCleanCache = m_settings->value("updates/cleanAfterUpdate", true).toBool();
    m_checkOnStartup = m_settings->value("updates/checkOnStartup", false).toBool();
    m_offlineUpdateEnabled = m_settings->value("updates/offlineUpdateEnabled", false).toBool();
}

void MainWindow::saveSettings() {
    m_settings->setValue("cache/oldVersionsToKeep", m_oldVersionsToKeep);
    m_settings->setValue("updates/cleanAfterUpdate", m_autoCleanCache);
    m_settings->setValue("updates/checkOnStartup", m_checkOnStartup);
    m_settings->setValue("updates/offlineUpdateEnabled", m_offlineUpdateEnabled);
}
