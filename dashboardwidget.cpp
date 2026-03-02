#include "dashboardwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStackedWidget>
#include <QFrame>
#include <QIcon>
#include <QProgressBar>
#include <QHeaderView>
#include <QPushButton>
#include <QComboBox>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QAction>
#include <algorithm>

namespace Style {
    const QString ColorGreen = "#4CAF50";
    const QString ColorRed = "#F44336";
    const QString ColorYellow = "#FFC107";
    const QString ColorGrey = "#ccc";
    const QString ColorDarkGrey = "#888";

    const QString HeaderFont = "font-weight: bold; font-size: 16px;";
    const QString SubHeaderFont = "font-weight: bold; color: #888;";
    const QString MessageFont = "font-size: 13px; color: #888; padding: 20px;";
    const QString BusyFont = "color: #aaa;";
    const QString ButtonStyle = "font-weight: bold; font-size: 14px;";
    const QString SeparatorStyle = "color: #444;";
}

DashboardWidget::DashboardWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void DashboardWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    setupHeaderUI(mainLayout);

    auto *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet(Style::SeparatorStyle);
    mainLayout->addWidget(line);

    m_contentStack = new QStackedWidget(this);

    m_messageLabel = new QLabel(this);
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setStyleSheet(Style::MessageFont);
    m_contentStack->addWidget(m_messageLabel);

    setupPackageListUI();
    m_contentStack->addWidget(m_packageList);

    setupBusyPageUI();
    m_contentStack->addWidget(m_busyPage);

    setupRebootPageUI();
    m_contentStack->addWidget(m_rebootPage);

    mainLayout->addWidget(m_contentStack, 1);
}

void DashboardWidget::setupHeaderUI(QVBoxLayout *mainLayout)
{
    auto *headerLayout = new QHBoxLayout();
    m_statusIconLabel = new QLabel(this);
    m_statusIconLabel->setFixedSize(48, 48);

    m_statusTextLabel = new QLabel(this);
    m_statusTextLabel->setStyleSheet(Style::HeaderFont);

    headerLayout->addWidget(m_statusIconLabel);
    headerLayout->addWidget(m_statusTextLabel);
    headerLayout->addStretch();

    m_filterComboBox = new QComboBox(this);
    m_filterComboBox->addItem("Official (Explicit)", QVariant::fromValue(PackageFilter::Official));
    m_filterComboBox->addItem("AUR (Foreign)", QVariant::fromValue(PackageFilter::Aur));
    m_filterComboBox->addItem("Orphans (Unused)", QVariant::fromValue(PackageFilter::Orphans));
    m_filterComboBox->addItem("All Installed", QVariant::fromValue(PackageFilter::All));
    m_filterComboBox->addItem("Cached Packages", QVariant::fromValue(PackageFilter::Cache));
    m_filterComboBox->setVisible(false);
    m_filterComboBox->setFixedWidth(180);

    connect(m_filterComboBox, QOverload<int>::of(&QComboBox::activated), this, [this](int index){
        emit filterChanged(m_filterComboBox->itemData(index).value<PackageFilter>());
    });

    headerLayout->addWidget(m_filterComboBox);
    mainLayout->addLayout(headerLayout);

    auto *statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(30);

    auto *l = new QVBoxLayout();
    l->setSpacing(2);
    auto *t = new QLabel("Last Updated:");
    t->setStyleSheet(Style::SubHeaderFont);
    m_lastUpdatedLabel = new QLabel("Never");
    m_lastUpdatedLabel->setStyleSheet("color: palette(text);");
    l->addWidget(t);
    l->addWidget(m_lastUpdatedLabel);

    statsLayout->addLayout(l);
    statsLayout->addStretch();
    mainLayout->addLayout(statsLayout);
}

void DashboardWidget::setupPackageListUI()
{
    m_packageList = new QTreeWidget(this);
    m_packageList->setColumnCount(2);
    m_packageList->setAlternatingRowColors(true);
    m_packageList->setRootIsDecorated(false);
    m_packageList->setStyleSheet("QTreeWidget { border: none; }");
    m_packageList->setFocusPolicy(Qt::NoFocus);

    m_packageList->header()->setStretchLastSection(false);
    m_packageList->header()->setMinimumSectionSize(100);
    m_packageList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_packageList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    connect(m_packageList, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int){
        if (!item || item->data(0, Qt::UserRole + 1).toInt() == 3) return; // Origin 3 = Cache

        QString pkgName = item->text(0);
        bool isCritical = item->data(0, Qt::UserRole).toBool();
        bool newState = !isCritical;

        QFont font = item->font(0);
        font.setBold(newState);
        item->setFont(0, font);
        item->setFont(1, font);
        item->setData(0, Qt::UserRole, newState);

        emit criticalPackageToggled(pkgName, newState);
    });

    m_packageList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_packageList, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTreeWidgetItem *item = m_packageList->itemAt(pos);
        if (!item) return;

        QString pkgName = item->text(0);
        int origin = item->data(0, Qt::UserRole + 1).toInt();
        QMenu menu(this);

        if (origin == 3) {
            QAction *deleteCacheAction = menu.addAction(QIcon::fromTheme("edit-delete"), "Delete");
            if (menu.exec(m_packageList->viewport()->mapToGlobal(pos)) == deleteCacheAction) {
                emit deleteCachedPackageRequested(pkgName);
            }
            return;
        }

        QAction *archPkgAction = (origin == 0 || origin == 2) ? menu.addAction(QIcon::fromTheme("internet-web-browser"), "Search Arch Packages") : nullptr;
        QAction *aurAction = (origin == 1 || origin == 2) ? menu.addAction(QIcon::fromTheme("internet-web-browser"), "Search AUR") : nullptr;
        menu.addSeparator();
        QAction *wikiAction = menu.addAction(QIcon::fromTheme("help-browser"), "Search Arch Wiki");

        QAction *selected = menu.exec(m_packageList->viewport()->mapToGlobal(pos));
        if (archPkgAction && selected == archPkgAction) QDesktopServices::openUrl(QUrl("https://archlinux.org/packages/?q=" + pkgName));
            else if (aurAction && selected == aurAction) QDesktopServices::openUrl(QUrl("https://aur.archlinux.org/packages/" + pkgName));
                else if (selected == wikiAction) QDesktopServices::openUrl(QUrl("https://wiki.archlinux.org/title/Special:Search?search=" + pkgName));
    });
}

void DashboardWidget::setupBusyPageUI()
{
    m_busyPage = new QWidget(this);
    auto *busyLayout = new QVBoxLayout(m_busyPage);
    busyLayout->setAlignment(Qt::AlignCenter);
    busyLayout->setSpacing(20);

    m_busyLabel = new QLabel("Processing...", m_busyPage);
    QFont busyFont = font();
    busyFont.setPointSize(12);
    m_busyLabel->setFont(busyFont);
    m_busyLabel->setStyleSheet(Style::BusyFont);

    m_busyProgressBar = new QProgressBar(m_busyPage);
    m_busyProgressBar->setRange(0, 0);
    m_busyProgressBar->setFixedWidth(300);
    m_busyProgressBar->setTextVisible(false);

    busyLayout->addWidget(m_busyLabel, 0, Qt::AlignCenter);
    busyLayout->addWidget(m_busyProgressBar, 0, Qt::AlignCenter);
}

void DashboardWidget::setupRebootPageUI()
{
    m_rebootPage = new QWidget(this);
    auto *rebootLayout = new QVBoxLayout(m_rebootPage);
    rebootLayout->setAlignment(Qt::AlignCenter);
    rebootLayout->setSpacing(20);

    QLabel *rebootInfo = new QLabel("Updates have been downloaded successfully.\nThey will be installed automatically during the next reboot.", m_rebootPage);
    rebootInfo->setAlignment(Qt::AlignCenter);
    rebootInfo->setStyleSheet(Style::MessageFont);

    m_rebootButton = new QPushButton(QIcon::fromTheme("system-reboot"), "Reboot and Update Now", m_rebootPage);
    m_rebootButton->setCursor(Qt::PointingHandCursor);
    m_rebootButton->setFixedSize(220, 50);
    m_rebootButton->setStyleSheet(Style::ButtonStyle);

    connect(m_rebootButton, &QPushButton::clicked, this, &DashboardWidget::rebootClicked);

    rebootLayout->addWidget(rebootInfo, 0, Qt::AlignCenter);
    rebootLayout->addWidget(m_rebootButton, 0, Qt::AlignCenter);
}

// --- Helper Methods ---

void DashboardWidget::setHeaderState(const QString& iconName, const QString& title, const QString& color)
{
    m_statusIconLabel->setPixmap(QIcon::fromTheme(iconName).pixmap(48, 48));
    m_statusTextLabel->setText(title);
    m_statusTextLabel->setStyleSheet("color: " + color + ";");
}

QTreeWidgetItem* DashboardWidget::createPackageItem(const QString& name, const QString& version, const QStringList& criticalPackages, int originFlag, bool isCache)
{
    auto *item = new QTreeWidgetItem(m_packageList);
    item->setText(0, name);
    item->setText(1, version);

    if (!isCache) {
        item->setToolTip(0, "Double-click to toggle Critical Status\nRight-click for online package info");
    }

    bool isCritical = criticalPackages.contains(name);
    item->setData(0, Qt::UserRole, isCritical);
    item->setData(0, Qt::UserRole + 1, originFlag);

    if (isCritical && !isCache) {
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
        item->setFont(1, f);
    }
    return item;
}

QString DashboardWidget::getRelativeTime(const QDateTime& dt)
{
    if (!dt.isValid()) return "Never";
    qint64 secs = dt.secsTo(QDateTime::currentDateTime());
    if (secs < 60) return "Just now";
    if (secs < 3600) return QString("%1 mins ago").arg(secs / 60);
    if (secs < 86400) return QString("%1 hours ago").arg(secs / 3600);
    qint64 days = secs / 86400;
    if (days == 1) return "Yesterday";
    if (days < 7) return QString("%1 days ago").arg(days);
    if (days < 30) return QString("%1 weeks ago").arg(days / 7);
    if (days < 365) return QString("%1 months ago").arg(days / 30);
    return "Over a year ago";
}

void DashboardWidget::setTimestamps(const QDateTime& lastUpdated)
{
    m_lastUpdatedLabel->setText(getRelativeTime(lastUpdated));
}

// --- UI State Methods ---

void DashboardWidget::showStatusUnknown()
{
    m_filterComboBox->setVisible(false);
    setHeaderState("dialog-information", "Status Unknown", Style::ColorGrey);
    m_messageLabel->setText("If checking for updates fails repeatedly on a good connection, your mirrorlist may be outdated.\n\nManually update it or use Reflector to refresh it in the menubar under 'Pacman'.");
    m_contentStack->setCurrentIndex(0);
}

void DashboardWidget::showUpToDate()
{
    m_filterComboBox->setVisible(false);
    setHeaderState("security-high", "System Up to Date", Style::ColorGreen);
    m_messageLabel->setText("Your system is running the latest available packages.");
    m_contentStack->setCurrentIndex(0);
}

void DashboardWidget::showUpdatesAvailable(const QList<UpdatePackageInfo>& packages, const QStringList& criticalPackages, int criticalCount)
{
    m_filterComboBox->setVisible(false);

    if (criticalCount > 0) {
        setHeaderState("security-low", QString("%1 Updates (%2 Critical)").arg(packages.size()).arg(criticalCount), Style::ColorRed);
    } else {
        setHeaderState("security-medium", QString("%1 Updates Available").arg(packages.size()), Style::ColorYellow);
    }

    m_packageList->setHeaderLabels({"Name", "Version"});
    m_packageList->clear();

    QList<UpdatePackageInfo> sortedPackages = packages;
    std::sort(sortedPackages.begin(), sortedPackages.end(), [&criticalPackages](const UpdatePackageInfo& a, const UpdatePackageInfo& b) {
        bool aCrit = criticalPackages.contains(a.name);
        bool bCrit = criticalPackages.contains(b.name);
        if (aCrit != bCrit) return aCrit;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    for (const auto& pkg : sortedPackages) {
        createPackageItem(pkg.name, QString("%1 -> %2").arg(pkg.oldVersion, pkg.newVersion), criticalPackages, 0, false);
    }

    m_contentStack->setCurrentIndex(1);
}

void DashboardWidget::showInstalledList(const QStringList& packages, const QStringList& criticalPackages, PackageFilter currentFilter)
{
    m_filterComboBox->setVisible(true);
    int index = m_filterComboBox->findData(QVariant::fromValue(currentFilter));
    if (index != -1) m_filterComboBox->setCurrentIndex(index);

    QString title;
    int originFlag = 2;
    switch(currentFilter) {
        case PackageFilter::Official: title = "Installed Official Packages"; originFlag = 0; break;
        case PackageFilter::Aur:      title = "Installed AUR Packages"; originFlag = 1; break;
        case PackageFilter::Orphans:  title = "Installed Orphan Packages"; break;
        case PackageFilter::All:      title = "All Installed Packages"; break;
        case PackageFilter::Cache:    title = "Cached Packages"; originFlag = 3; break;
    }

    setHeaderState("system-software-install", title, Style::ColorGrey);
    m_packageList->setHeaderLabels(currentFilter == PackageFilter::Cache ? QStringList{"File", "Size"} : QStringList{"Name", "Version"});
    m_packageList->clear();

    if (packages.isEmpty()) {
        m_messageLabel->setText("No packages found for this filter.");
        m_contentStack->setCurrentIndex(0);
    } else {
        bool isCache = (currentFilter == PackageFilter::Cache);
        for (const auto& line : packages) {
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            createPackageItem(parts.value(0), (parts.size() > 1) ? parts.value(1) : "", criticalPackages, originFlag, isCache);
        }
        m_contentStack->setCurrentIndex(1);
    }
}

void DashboardWidget::showRebootReadyState()
{
    m_filterComboBox->setVisible(false);
    setHeaderState("system-reboot", "Ready to Install", Style::ColorGreen);
    m_contentStack->setCurrentIndex(3);
}

void DashboardWidget::showErrorState()
{
    m_filterComboBox->setVisible(false);
    setHeaderState("dialog-error", "Operation Failed", Style::ColorRed);
    m_messageLabel->setText("If updating is failing under a good connection you may need to refresh your mirrorlist via reflector or repair the keyring under the \"Pacman\" menu.\n\nClick \"Show Terminal Output\" for detailed output.");
    m_contentStack->setCurrentIndex(0);
}

void DashboardWidget::showOperationCancelled()
{
    m_filterComboBox->setVisible(false);
    setHeaderState("dialog-warning", "Operation Cancelled", Style::ColorYellow);
    m_messageLabel->setText("The operation was cancelled by the user.");
    m_contentStack->setCurrentIndex(0);
}

void DashboardWidget::showBusyState(const QString& message)
{
    setHeaderState("view-refresh", "Working...", Style::ColorGrey);
    m_busyLabel->setText(message);
    m_contentStack->setCurrentIndex(2);
}

void DashboardWidget::updateBusyMessage(const QString& message)
{
    if (m_busyLabel) m_busyLabel->setText(message);
}
