#pragma once
#include <QWidget>

class QTermWidget;

class TerminalWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalWindow(QWidget *parent = nullptr);

    void runCommand(const QString& command);
    void setFocusToTerminal();
    void setInputEnabled(bool enabled);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QTermWidget *m_terminal;
    bool m_inputEnabled = false;
};
