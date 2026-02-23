#include "terminalwindow.h"
#include <qtermwidget.h>
#include <QVBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>

TerminalWindow::TerminalWindow(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_terminal = new QTermWidget;
    m_terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);
    m_terminal->setColorScheme("WhiteOnBlack");
    m_terminal->setTerminalFont(QFont("Monospace", 10));

    // Expand history buffer so large system updates don't get cut off
    m_terminal->setHistorySize(8192);

    // Context Menu for Copying
    m_terminal->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_terminal, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos){
        QMenu menu(this);
        QAction *copyAction = menu.addAction(QIcon::fromTheme("edit-copy"), "Copy");

        connect(copyAction, &QAction::triggered, this, [this](){
            m_terminal->copyClipboard();
        });

        menu.exec(m_terminal->mapToGlobal(pos));
    });

    // Install event filters
    m_terminal->installEventFilter(this);
    const auto children = m_terminal->findChildren<QWidget *>();
    for (QWidget *child : children) {
        child->installEventFilter(this);
    }

    layout->addWidget(m_terminal);
}

void TerminalWindow::runCommand(const QString& command)
{
    if (!m_terminal) return;

    QByteArray clearSequence;
    clearSequence.append(static_cast<char>(0x05)); // Ctrl+E
    clearSequence.append(static_cast<char>(0x15)); // Ctrl+U

    m_terminal->sendText(clearSequence);
    m_terminal->sendText(command + "\n");
}

void TerminalWindow::setFocusToTerminal()
{
    m_terminal->setFocus();
}

void TerminalWindow::setInputEnabled(bool enabled)
{
    m_inputEnabled = enabled;
}

bool TerminalWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Force Mouse Wheel to Scroll the Output Buffer
    if (event->type() == QEvent::Wheel) {

        // If the mouse is directly over the actual scrollbar, let it scroll natively
        if (qobject_cast<QScrollBar*>(obj)) {
            return false;
        }

        // Otherwise, intercept the wheel event and manually push the scrollbar
        QScrollBar *scrollBar = m_terminal->findChild<QScrollBar *>();
        if (scrollBar) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            int delta = wheelEvent->angleDelta().y();
            if (delta != 0) {
                // Scroll 3 lines per notch. Positive delta = scroll up (negative value)
                int steps = (delta > 0) ? -3 : 3;
                scrollBar->setValue(scrollBar->value() + steps);
            }
        }

        // Swallow the event so QTermWidget doesn't translate it into Up/Down Arrow keys
        return true;
    }

    // If input is explicitly enabled, let everything pass through
    if (m_inputEnabled) {
        return QWidget::eventFilter(obj, event);
    }

    // Block middle-click paste for safety, but allow left-click highlighting and right-click menus
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick || event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            return true;
        }
    }

    // Safely filter keyboard events to allow UI interaction while blocking shell input
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // Allow keyboard terminal scrolling (Shift + PageUp / Shift + PageDown)
        if ((keyEvent->key() == Qt::Key_PageUp || keyEvent->key() == Qt::Key_PageDown) && (keyEvent->modifiers() & Qt::ShiftModifier)) {
            return false;
        }

        // Block all other keystrokes from going to the shell
        return true;
    }

    return QWidget::eventFilter(obj, event);
}
