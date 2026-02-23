#pragma once

#include <QObject>
#include <QTemporaryDir>
#include <QString>
#include <functional>

class TerminalWindow;
class QFileSystemWatcher;

class CommandRunner : public QObject
{
    Q_OBJECT

public:
    explicit CommandRunner(TerminalWindow* terminal, QObject* parent = nullptr);
    ~CommandRunner() = default;

    void run(const QString& command, const QString& description, bool captureOutput, bool requiresRoot, std::function<void(QString, int)> callback = nullptr);
    bool isBusy() const { return m_isBusy; }

signals:
    void commandStarted();
    void terminalReady();
    void commandFinished();

private slots:
    void onDirectoryChanged(const QString& path);

private:
    TerminalWindow* m_terminal;
    QTemporaryDir m_tempDir;
    QString m_startSignalFile;
    QString m_endSignalFile;
    QString m_outputFile;
    QString m_exitCodeFile;
    QFileSystemWatcher* m_watcher;

    bool m_isBusy;
    std::function<void(QString, int)> m_callback;
};
