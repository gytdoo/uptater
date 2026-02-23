#pragma once

#include <QObject>
#include <QStringList>
#include <QMap>

const QStringList PACMAN_TOGGLE_OPTIONS = {
    "UseSyslog", "Color", "ILoveCandy", "CheckSpace", "VerbosePkgLists"
};
const QString PACMAN_CONF_PATH = "/etc/pacman.conf";

class PacmanConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit PacmanConfigManager(QObject *parent = nullptr);

    void readConfig();

    bool isOptionEnabled(const QString& optionName) const;
    int getParallelDownloadsCount() const;

public slots:
    void toggleOption(const QString& optionName);
    void setParallelDownloadsCount(int value);
    void enableParallelDownloads(bool enabled);

signals:
    void commandRequested(const QString& command, const QString& description);

private:
    void writeConfig();
    void reset();

    QMap<QString, bool> m_pacmanConfigToggles;
    int m_parallelDownloadsCount = 5;
    bool m_backupCreated = false;
};
