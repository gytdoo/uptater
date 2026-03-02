#pragma once

#include <QWidget>
#include <QDateTime>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QProgressBar;
class QPushButton;
class QComboBox;
class QVBoxLayout;

struct UpdatePackageInfo {
    QString name;
    QString oldVersion;
    QString newVersion;
    bool ignored;
};

class DashboardWidget : public QWidget
{
    Q_OBJECT
public:
    enum class PackageFilter { All, Official, Aur, Orphans, Cache };

    explicit DashboardWidget(QWidget *parent = nullptr);

    void setTimestamps(const QDateTime& lastUpdated);

    void showStatusUnknown();
    void showUpToDate();
    void showUpdatesAvailable(const QList<UpdatePackageInfo>& packages, const QStringList& criticalPackages, int criticalCount);
    void showRebootReadyState();
    void showErrorState();
    void showOperationCancelled();
    void showBusyState(const QString& message);
    void updateBusyMessage(const QString& message);

    void showInstalledList(const QStringList& packages, const QStringList& criticalPackages, PackageFilter currentFilter);

signals:
    void criticalPackageToggled(const QString& packageName, bool isCritical);
    void rebootClicked();
    void filterChanged(DashboardWidget::PackageFilter filter);
    void deleteCachedPackageRequested(const QString& fileName);

private:
    void setupUi();
    void setupHeaderUI(QVBoxLayout *mainLayout);
    void setupPackageListUI();
    void setupBusyPageUI();
    void setupRebootPageUI();

    void setHeaderState(const QString& iconName, const QString& title, const QString& color);
    QTreeWidgetItem* createPackageItem(const QString& name, const QString& version, const QStringList& criticalPackages, int originFlag, bool isCache);
    QString getRelativeTime(const QDateTime& dt);

    QLabel *m_statusIconLabel;
    QLabel *m_statusTextLabel;
    QLabel *m_lastUpdatedLabel;
    QComboBox *m_filterComboBox;
    QStackedWidget *m_contentStack;
    QLabel *m_messageLabel;
    QTreeWidget *m_packageList;
    QWidget *m_busyPage;
    QLabel *m_busyLabel;
    QProgressBar *m_busyProgressBar;
    QWidget *m_rebootPage;
    QPushButton *m_rebootButton;
};
