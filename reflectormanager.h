#pragma once

#include <QObject>
#include <QStringList>

class QMenu;
class QProcess;
class QAction;

class ReflectorManager : public QObject
{
    Q_OBJECT
public:
    explicit ReflectorManager(QObject *parent = nullptr);

    QList<QAction*> getActions();
    void setup();

signals:
    void commandRequested(const QString& command, const QString& description);
    void menuActionCompleted();

private slots:
    void onInstallReflector();
    void onPopulateMirrorlistMenu();
    void onCountryListFetched(int exitCode);

private:
    QAction* m_installAction;
    QMenu* m_refreshMenu;
    QProcess* m_reflectorListProcess;

    bool m_countryListFetched = false;
};
