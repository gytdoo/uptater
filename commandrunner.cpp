#include "commandrunner.h"
#include "terminalwindow.h"
#include <QFileSystemWatcher>
#include <QFile>
#include <QTextStream>

CommandRunner::CommandRunner(TerminalWindow* terminal, QObject* parent)
: QObject(parent), m_terminal(terminal), m_isBusy(false)
{
    if (m_tempDir.isValid()) {
        m_startSignalFile = m_tempDir.filePath("uptater-start-signal");
        m_endSignalFile = m_tempDir.filePath("uptater-end-signal");
        m_outputFile = m_tempDir.filePath("output.txt");
        m_exitCodeFile = m_tempDir.filePath("exitcode.txt");

        m_watcher = new QFileSystemWatcher(this);
        m_watcher->addPath(m_tempDir.path());
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &CommandRunner::onDirectoryChanged);
    }
}

void CommandRunner::run(const QString& command, const QString& description, bool captureOutput, bool requiresRoot, std::function<void(QString, int)> callback)
{
    if (m_isBusy) return;

    m_isBusy = true;
    m_callback = callback;

    QFile::remove(m_startSignalFile);
    QFile::remove(m_endSignalFile);
    QFile::remove(m_outputFile);
    QFile::remove(m_exitCodeFile);

    emit commandStarted();

    QString historyBlock = !m_keepBashHistory ? " set +o history; export HISTFILE=/dev/null; " : "";

    QString finalCmd = command;
    if (requiresRoot) {
        if (finalCmd.startsWith("sudo ")) {
            finalCmd = finalCmd.mid(5);
        }
        QString escapedCmd = historyBlock + finalCmd;
        escapedCmd.replace("'", "'\\''");
        finalCmd = "pkexec sh -c '" + escapedCmd + "'";
    }

    QString compositeCmd;
    QTextStream out(&compositeCmd);

    if (!m_keepBashHistory) out << " ";

    out << "set +H; " << historyBlock << "(";
    out << " export LC_ALL=C;";
    out << " trap 'touch \"" << m_endSignalFile << "\"' EXIT;";

    QString escapedDesc = description;
    escapedDesc.replace("\"", "\\\"");
    out << " echo -e \"\\n\\033[1;36m>>> " << escapedDesc << "\\033[0m\";";
    out << " touch \"" << m_startSignalFile << "\";";

    if (captureOutput) {
        out << " { " << finalCmd << " 2>&1 | tee \"" << m_outputFile << "\"; } ; echo ${PIPESTATUS[0]} > \"" << m_exitCodeFile << "\";";
    } else {
        out << " { " << finalCmd << "; } ; echo $? > \"" << m_exitCodeFile << "\";";
    }

    out << " )";

    m_terminal->runCommand(compositeCmd);
}

void CommandRunner::onDirectoryChanged(const QString& /*path*/)
{
    if (QFile::exists(m_startSignalFile)) {
        QFile::remove(m_startSignalFile);
        emit terminalReady();
    }
    else if (QFile::exists(m_endSignalFile)) {
        QFile::remove(m_endSignalFile);

        QString output;
        if (QFile::exists(m_outputFile)) {
            QFile file(m_outputFile);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                output = QTextStream(&file).readAll();
            }
        }

        int exitCode = 1;
        if (QFile::exists(m_exitCodeFile)) {
            QFile file(m_exitCodeFile);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString codeStr = QTextStream(&file).readAll().trimmed();
                if (!codeStr.isEmpty()) exitCode = codeStr.toInt();
            }
        }

        m_isBusy = false;

        auto cb = m_callback;
        m_callback = nullptr;

        emit commandFinished();

        if (cb) {
            cb(output, exitCode);
        }
    }
}
