#include "packagemanager.h"
#include "commandrunner.h"
#include <QRegularExpression>

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

// --- Multi-Step Chained Operations ---

void PackageManager::installYay(const QString& variant) {
    emit statusMessageChanged("Cleaning previous installations and fetching dependencies...");

    // Dynamically assign dependencies: yay (source) requires 'go', the others do not
    QString deps = (variant == "yay") ? "git base-devel go" : "git base-devel";
    QString homeCache = QDir::homePath() + "/.cache/yay";

    // Step 1: The Ultimate Clean Slate (Full uninstall + install dependencies)
    QString step1 = QString(
        "pacman -Rns yay --noconfirm 2>/dev/null || true; "
        "pacman -Rns yay-bin --noconfirm 2>/dev/null || true; "
        "pacman -Rns yay-git --noconfirm 2>/dev/null || true; "
        "pacman -Rns yay-debug --noconfirm 2>/dev/null || true; "
        "pacman -Rns yay-bin-debug --noconfirm 2>/dev/null || true; "
        "rm -f /usr/bin/yay 2>/dev/null || true; "
        "rm -rf '%1' 2>/dev/null || true; "
        "pacman -S --needed %2 --noconfirm"
    ).arg(homeCache, deps);

    m_runner->run(step1, "Preparing system...", false, true, [this, variant](QString, int exitCode){
        if (exitCode != 0) { emit operationFinished(false, isCancelled(exitCode)); return; }

        // --- GITHUB FALLBACK ROUTE ---
        if (variant == "github") {
            emit statusMessageChanged("Downloading pre-compiled release from GitHub...");
            QString step2 = "rm -rf /tmp/yay_github && mkdir -p /tmp/yay_github && cd /tmp/yay_github && "
            "curl -s https://api.github.com/repos/Jguer/yay/releases/latest | grep browser_download_url | grep _x86_64.tar.gz | cut -d '\"' -f 4 | wget -qi - && "
            "tar -xzf *.tar.gz";

            m_runner->run(step2, "Downloading release...", false, false, [this](QString, int exitCode){
                if (exitCode != 0) { emit operationFinished(false, isCancelled(exitCode)); return; }

                emit statusMessageChanged("Waiting for password to install GitHub binary...");
                QString step3 = "install -Dm755 /tmp/yay_github/*/yay /usr/bin/yay";

                m_runner->run(step3, "Installing package...", false, true, [this](QString, int exitCode){
                    emit operationFinished(exitCode == 0, isCancelled(exitCode));
                });
            });
            return;
        }

        // --- STANDARD AUR ROUTE (yay & yay-bin) ---
        emit statusMessageChanged(QString("Cloning %1 from AUR...").arg(variant));
        QString repoUrl = QString("https://aur.archlinux.org/%1.git").arg(variant);
        QString step2 = QString("rm -rf /tmp/%1 && git clone %2 /tmp/%1").arg(variant, repoUrl);

        m_runner->run(step2, "Cloning repository...", false, false, [this, variant](QString, int exitCode){
            if (exitCode != 0) { emit operationFinished(false, isCancelled(exitCode)); return; }

            emit statusMessageChanged(QString("Building %1 package (this may take a minute)...").arg(variant));

            // Step 3: Explicitly disable debug package generation before compiling
            QString step3 = QString("cd /tmp/%1 && echo 'options=(!debug)' >> PKGBUILD && makepkg -c --noconfirm").arg(variant);

            m_runner->run(step3, "Building package...", false, false, [this, variant](QString, int exitCode){
                if (exitCode != 0) { emit operationFinished(false, isCancelled(exitCode)); return; }

                emit statusMessageChanged("Waiting for password to install final package...");
                QString step4 = QString("pacman -U /tmp/%1/*.pkg.tar.zst --noconfirm").arg(variant);

                m_runner->run(step4, "Installing package...", false, true, [this](QString, int exitCode){
                    emit operationFinished(exitCode == 0, isCancelled(exitCode));
                });
            });
        });
    });
}

void PackageManager::uninstallYay() {
    emit statusMessageChanged("Waiting for password to uninstall Yay...");

    // Get the actual user's home directory so we don't accidentally clear /root/.cache
    QString homeCache = QDir::homePath() + "/.cache/yay";

    // Step 1: Attempt to remove all possible variants natively using -Rns to clean dependencies
    // Step 2: Force remove the binary in case they used the untracked "GitHub" fallback
    // Step 3: Nuke the build cache
    QString cmd = QString(
        "pacman -Rns yay --noconfirm 2>/dev/null || true; "
        "pacman -Rns yay-bin --noconfirm 2>/dev/null || true; "
        "pacman -Rns yay-git --noconfirm 2>/dev/null || true; "
        "pacman -Rns yay-debug --noconfirm 2>/dev/null || true; "
        "pacman -Rns yay-bin-debug --noconfirm 2>/dev/null || true; "
        "rm -f /usr/bin/yay 2>/dev/null || true; "
        "rm -rf '%1' 2>/dev/null || true"
    ).arg(homeCache);

    m_runner->run(cmd, "Uninstalling yay...", false, true, [this](QString, int exitCode){
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
    // Yay handles the build steps internally and will prompt for pkexec when ready
    emit statusMessageChanged("Delegating build process to Yay...");
    QString cmd = "yay -S systemd-system-update-pacman --noconfirm --sudo pkexec --sudoflags \"\"";

    m_runner->run(cmd, "Installing offline update tool...", false, false, [this](QString, int exitCode){
        emit operationFinished(exitCode == 0, isCancelled(exitCode));
    });
}

void PackageManager::installOfflineUpdaterManual() {
    emit statusMessageChanged("Installing build dependencies...");

    // Step 1: Ensure git and base-devel are installed before trying to build
    QString step1 = "pacman -S --needed git base-devel --noconfirm";

    m_runner->run(step1, "Preparing system...", false, true, [this](QString, int exitCode){
        if (exitCode != 0) { emit operationFinished(false, isCancelled(exitCode)); return; }

        emit statusMessageChanged("Cloning offline updater from AUR...");
        QString step2 = "rm -rf /tmp/sysup && git clone https://aur.archlinux.org/systemd-system-update-pacman.git /tmp/sysup";

        // Step 2: Clone Repo (User Space)
        m_runner->run(step2, "Cloning repository...", false, false, [this](QString, int exitCode){
            if(exitCode != 0) { emit operationFinished(false, isCancelled(exitCode)); return; }

            emit statusMessageChanged("Building offline update package...");

            // Step 3: Compile and explicitly skip debug packages (User Space)
            QString step3 = "cd /tmp/sysup && echo 'options=(!debug)' >> PKGBUILD && makepkg -c --noconfirm";

            m_runner->run(step3, "Building package...", false, false, [this](QString, int exitCode){
                if(exitCode != 0) { emit operationFinished(false, isCancelled(exitCode)); return; }

                emit statusMessageChanged("Waiting for password to install final package...");
                QString step4 = "pacman -U /tmp/sysup/*.pkg.tar.zst --noconfirm";

                // Step 4: Install the final compiled package (Requires Root)
                m_runner->run(step4, "Installing package...", false, true, [this](QString, int exitCode){
                    emit operationFinished(exitCode == 0, isCancelled(exitCode));
                });
            });
        });
    });
}
