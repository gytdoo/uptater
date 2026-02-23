#include "packagemanager.h"
#include "commandrunner.h"
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

PackageManager::PackageManager(CommandRunner* runner, QObject* parent)
: QObject(parent), m_runner(runner)
{
}

static QString stripAnsi(const QString& input) {
    static const QRegularExpression ansiRegex(R"(\x1B\[[0-9;]*[a-zA-Z])");
    QString result = input;
    result.remove(ansiRegex);
    return result;
}

QString PackageManager::writeScriptToTemp(const QString& resourcePath) {
    QFile resFile(resourcePath);
    if (!resFile.open(QIODevice::ReadOnly | QIODevice::Text)) return "";

    QString content = QTextStream(&resFile).readAll();
    QString tempPath = QDir::tempPath() + "/uptater_" + QFileInfo(resourcePath).fileName();

    QFile outFile(tempPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream(&outFile) << content;
        outFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        outFile.close();
        return tempPath;
    }
    return "";
}

bool PackageManager::isCancelled(int exitCode) {
    return (exitCode == 126 || exitCode == 127 || exitCode == 130);
}

void PackageManager::runRawCommand(const QString& cmd, const QString& desc)
{
    m_runner->run(cmd, desc, false, true, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::checkSystemUpdates()
{
    m_runner->run("checkupdates", "Checking for system updates...", true, false, [this](QString output, int exitCode){
        QString cleanContent = stripAnsi(output);
        QStringList lines = cleanContent.split('\n', Qt::SkipEmptyParts);

        QList<UpdatePackageInfo> updates;
        bool errorFound = (exitCode != 0 && exitCode != 2);

        for (const QString& line : lines) {
            if (line.contains("->")) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 4) {
                    updates.append({parts[0], parts[1], parts[3], false});
                }
            } else if (line.contains("error", Qt::CaseInsensitive) || line.contains("failed", Qt::CaseInsensitive)) {
                errorFound = true;
            }
        }

        emit updatesCheckFinished(updates, errorFound);
    });
}

void PackageManager::installSystemUpdates(bool offlineUpdate, bool autoCleanCache, int oldVersionsToKeep)
{
    QString cmdChain;
    QString descriptionSuffix = "";
    QString pacCmd = offlineUpdate ? "pacman -Syuw --noconfirm" : "pacman -Syu --noconfirm";

    cmdChain += pacCmd;

    if (autoCleanCache) {
        cmdChain += QString(" && paccache -rk%1").arg(oldVersionsToKeep + 1);
        descriptionSuffix = " & cleaning cache";
    }

    if (offlineUpdate) {
        cmdChain += " && schedule-system-update";
    }

    QString desc = offlineUpdate ? "Downloading updates" : "Installing updates";
    desc += descriptionSuffix + "...";

    m_runner->run("bash -c '" + cmdChain + "'", desc, false, true, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::cancelScheduledUpdate()
{
    m_runner->run("rm -f /system-update", "Cancelling scheduled update...", false, true, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::fetchPackageList(DashboardWidget::PackageFilter filter)
{
    QString command;
    QString desc;
    switch(filter) {
        case DashboardWidget::PackageFilter::Official: command = "pacman -Qen"; desc = "Listing Official Packages..."; break;
        case DashboardWidget::PackageFilter::Aur: command = "pacman -Qm"; desc = "Listing AUR Packages..."; break;
        case DashboardWidget::PackageFilter::Orphans: command = "pacman -Qdt"; desc = "Listing Orphan Packages..."; break;
        case DashboardWidget::PackageFilter::Cache: command = "ls -lh /var/cache/pacman/pkg | awk 'NR>1 && !/\\.sig$/ {print $9, $5}'"; desc = "Listing Cached Packages..."; break;
        case DashboardWidget::PackageFilter::All:
        default: command = "pacman -Q"; desc = "Listing All Installed Packages..."; break;
    }

    m_runner->run(command, desc, true, false, [this](QString output, int){
        QString cleanContent = stripAnsi(output);
        QStringList lines = cleanContent.split('\n', Qt::SkipEmptyParts);
        emit packageListFetched(lines);
    });
}

void PackageManager::cleanCache(int oldVersionsToKeep)
{
    QString cmd = QString("paccache -rk%1").arg(oldVersionsToKeep + 1);
    m_runner->run("bash -c '" + cmd + "'", "Cleaning pacman cache...", false, true, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::clearAllCache()
{
    m_runner->run("rm -rf /var/cache/pacman/pkg/*", "Completely clearing pacman cache...", false, true, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::deleteCachedPackage(const QString& fileName)
{
    QString script = QString("rm -f /var/cache/pacman/pkg/%1 /var/cache/pacman/pkg/%1.sig").arg(fileName);
    m_runner->run(script, "Deleting cached package: " + fileName, false, true, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::repairKeyring()
{
    m_runner->run("pacman -Sy archlinux-keyring --noconfirm && pacman-key --populate archlinux", "Repairing Pacman Keyring...", false, true, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::installYay(const QString& variant) {
    QString tempPath = writeScriptToTemp(":/scripts/install_yay.sh");
    if (tempPath.isEmpty()) return;

    m_runner->run("bash " + tempPath + " " + variant, "Installing yay...", false, false, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::uninstallYay() {
    QString tempPath = writeScriptToTemp(":/scripts/uninstall_yay.sh");
    if (tempPath.isEmpty()) return;

    m_runner->run("bash " + tempPath, "Uninstalling yay...", false, false, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::updateAur() {
    m_runner->run("yay --aur", "Updating AUR Packages...", false, false, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::cleanAurLeftovers() {
    m_runner->run("yay -Yc", "Cleaning AUR leftovers...", false, false, [this](QString, int exitCode){
        bool success = (exitCode == 0 || exitCode == 1);
        emit operationFinished(success, isCancelled(exitCode));
    });
}

void PackageManager::installOfflineUpdater() {
    QString cmd = "yay -S systemd-system-update-pacman --noconfirm --sudo pkexec --sudoflags \"\"";
    m_runner->run(cmd, "Installing offline update tool...", false, false, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::installOfflineUpdaterManual() {
    QString script =
    "echo \"Starting manual install...\" && WORK_DIR=$(mktemp -d) && cd \"$WORK_DIR\" && "
    "git clone https://aur.archlinux.org/systemd-system-update-pacman.git && "
    "cd systemd-system-update-pacman && "
    "echo 'options=(!debug)' >> PKGBUILD && "
    "makepkg -c --noconfirm && "
    "pkexec pacman -U \"$PWD\"/*.pkg.tar.zst --noconfirm";

    m_runner->run(script, "Manually installing offline update tool...", false, false, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}
