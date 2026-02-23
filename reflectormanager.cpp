#include "reflectormanager.h"
#include "depcheck.h"
#include <QMenu>
#include <QProcess>
#include <QRegularExpression>
#include <QWidgetAction>
#include <QListWidget>

ReflectorManager::ReflectorManager(QObject *parent) : QObject(parent)
{
    m_reflectorListProcess = new QProcess(this);
    connect(m_reflectorListProcess, &QProcess::finished, this, &ReflectorManager::onCountryListFetched);

    m_installAction = new QAction("Install Reflector", this);
    connect(m_installAction, &QAction::triggered, this, &ReflectorManager::onInstallReflector);

    m_refreshMenu = new QMenu("Refresh Mirrorlist");
    connect(m_refreshMenu, &QMenu::aboutToShow, this, &ReflectorManager::onPopulateMirrorlistMenu);
}

QList<QAction*> ReflectorManager::getActions()
{
    return {m_installAction, m_refreshMenu->menuAction()};
}

void ReflectorManager::setup()
{
    bool reflectorInstalled = DepCheck::reflectorInstalled();
    m_installAction->setVisible(!reflectorInstalled);
    m_refreshMenu->menuAction()->setVisible(reflectorInstalled);
}

void ReflectorManager::onInstallReflector()
{
    emit commandRequested("pacman -S reflector --noconfirm", "Installing Reflector...");
}

void ReflectorManager::onPopulateMirrorlistMenu()
{
    if (m_countryListFetched) return;

    if (m_refreshMenu->actions().isEmpty()) {
        m_refreshMenu->addAction("Downloading List...")->setEnabled(false);
        m_reflectorListProcess->start("reflector", {"--list-countries"});
    }
}

void ReflectorManager::onCountryListFetched(int exitCode)
{
    m_refreshMenu->clear();

    if (exitCode != 0) {
        m_refreshMenu->addAction("Error fetching list")->setEnabled(false);
        return;
    }

    QString countryData = m_reflectorListProcess->readAllStandardOutput();
    auto* countryListWidget = new QListWidget(m_refreshMenu);
    countryListWidget->setMaximumHeight(300);
    countryListWidget->setAlternatingRowColors(true);

    static const QRegularExpression whitespaceRegex("\\s{2,}");

    for (const QString &fullLine : countryData.split('\n', Qt::SkipEmptyParts)) {
        if (fullLine.contains("---") || fullLine.startsWith("Country")) continue;

        int splitPos = fullLine.indexOf(whitespaceRegex);
        QString countryName = (splitPos == -1) ? fullLine.trimmed() : fullLine.left(splitPos).trimmed();

        if (!countryName.isEmpty()) {
            countryListWidget->addItem(countryName);
        }
    }

    connect(countryListWidget, &QListWidget::itemClicked, this, [this, countryListWidget](QListWidgetItem* item){
        QString countryName = item->text();
        QString command = QString("reflector --verbose --country \"%1\" --protocol https --sort rate --save /etc/pacman.d/mirrorlist").arg(countryName);
        QString description = QString("Refreshing mirrorlist for \"%1\"...").arg(countryName);

        emit commandRequested(command, description);

        countryListWidget->clearSelection();
        if (countryListWidget->count() > 0) {
            countryListWidget->scrollToItem(countryListWidget->item(0));
        }

        emit menuActionCompleted();
    });

    auto* listAction = new QWidgetAction(m_refreshMenu);
    listAction->setDefaultWidget(countryListWidget);
    m_refreshMenu->addAction(listAction);

    m_countryListFetched = true;
}
