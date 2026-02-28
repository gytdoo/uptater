#include "pacmanconfigmanager.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>

PacmanConfigManager::PacmanConfigManager(QObject *parent) : QObject(parent)
{
    readConfig();
}

void PacmanConfigManager::reset()
{
    m_parallelDownloadsCount = 5;
    m_pacmanConfigToggles.clear();
    for(const QString& option : PACMAN_TOGGLE_OPTIONS) {
        m_pacmanConfigToggles[option] = false;
    }
    m_pacmanConfigToggles["ParallelDownloads"] = false;
}

void PacmanConfigManager::readConfig()
{
    reset();

    QFile configFile(PACMAN_CONF_PATH);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&configFile);
    bool inOptionsSection = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        QString trimmedLine = line.trimmed();

        if (trimmedLine.startsWith("[options]")) { inOptionsSection = true; continue; }
        if (trimmedLine.startsWith("[")) { inOptionsSection = false; continue; }
        if (!inOptionsSection || trimmedLine.isEmpty()) continue;

        bool isCommented = trimmedLine.startsWith('#');
        QString cleanLine = isCommented ? trimmedLine.mid(1).trimmed() : trimmedLine;

        for (const QString& option : PACMAN_TOGGLE_OPTIONS) {
            if (cleanLine.startsWith(option)) {
                m_pacmanConfigToggles[option] = !isCommented;
            }
        }

        if (cleanLine.startsWith("ParallelDownloads")) {
            m_pacmanConfigToggles["ParallelDownloads"] = !isCommented;
            if (!isCommented) {
                QString valueStr = cleanLine.section('=', 1).trimmed();
                bool ok;
                int val = valueStr.toInt(&ok);
                if (ok) m_parallelDownloadsCount = val;
            }
        }
    }
}

void PacmanConfigManager::writeConfig()
{
    QFile inputFile(PACMAN_CONF_PATH);
    if (!inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QStringList lines = QTextStream(&inputFile).readAll().split('\n');
    inputFile.close();

    QStringList optionsFound;
    bool inOptionsSection = false;

    for (int i = 0; i < lines.size(); ++i) {
        QString trimmedLine = lines[i].trimmed();
        if (trimmedLine.startsWith("[options]")) { inOptionsSection = true; continue; }
        if (trimmedLine.startsWith("[")) { inOptionsSection = false; continue; }
        if (!inOptionsSection) continue;

        bool isCommented = trimmedLine.startsWith('#');
        QString cleanLine = isCommented ? trimmedLine.mid(1).trimmed() : trimmedLine;

        for (const QString& option : PACMAN_TOGGLE_OPTIONS) {
            if (cleanLine.startsWith(option)) {
                lines[i] = m_pacmanConfigToggles[option] ? option : "#" + option;
                optionsFound << option;
            }
        }

        if (cleanLine.startsWith("ParallelDownloads")) {
            QString newLine = QString("ParallelDownloads = %1").arg(m_parallelDownloadsCount);
            if (!m_pacmanConfigToggles["ParallelDownloads"]) newLine.prepend("#");
            lines[i] = newLine;
            optionsFound << "ParallelDownloads";
        }
    }

    static const QRegularExpression optionsRegex("^\\s*\\[options\\]\\s*$");
    int optionsIndex = lines.indexOf(optionsRegex);

    if (optionsIndex != -1) {
        for (const QString& option : PACMAN_TOGGLE_OPTIONS) {
            if (m_pacmanConfigToggles[option] && !optionsFound.contains(option)) {
                lines.insert(optionsIndex + 1, option);
            }
        }
    }

    QString tempPath = QDir::temp().filePath("pacman.conf.new");
    QFile outFile(tempPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outFile);
        out << lines.join('\n');
        outFile.close();

        QString command;
        if (!m_backupCreated) {
            command = QString("cp %1 %1.uptater.bak && cp %2 %1").arg(PACMAN_CONF_PATH, tempPath);
            m_backupCreated = true;
        } else {
            command = QString("cp %1 %2").arg(tempPath, PACMAN_CONF_PATH);
        }

        // Clean emission without redundant "sudo sh -c"
        emit commandRequested(command, "Applying pacman.conf changes...");
    }
}

bool PacmanConfigManager::isOptionEnabled(const QString &optionName) const
{
    return m_pacmanConfigToggles.value(optionName, false);
}

int PacmanConfigManager::getParallelDownloadsCount() const
{
    return m_parallelDownloadsCount;
}

void PacmanConfigManager::toggleOption(const QString &optionName)
{
    m_pacmanConfigToggles[optionName] = !m_pacmanConfigToggles[optionName];
    writeConfig();
}

void PacmanConfigManager::setParallelDownloadsCount(int value)
{
    m_parallelDownloadsCount = value;
    if (!m_pacmanConfigToggles["ParallelDownloads"]) {
        m_pacmanConfigToggles["ParallelDownloads"] = true;
    }
    writeConfig();
}

void PacmanConfigManager::enableParallelDownloads(bool enabled)
{
    m_pacmanConfigToggles["ParallelDownloads"] = enabled;
    writeConfig();
}
