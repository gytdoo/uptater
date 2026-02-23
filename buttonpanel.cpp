#include "buttonpanel.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QIcon>

ButtonPanel::ButtonPanel(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(10, 10, 10, 5);

    m_checkButton = new QPushButton(QIcon::fromTheme("view-refresh"), "Check for Updates");
    m_checkButton->setCursor(Qt::PointingHandCursor);
    m_checkButton->setMinimumHeight(40);

    m_installButton = new QPushButton(QIcon::fromTheme("system-software-install"), "Download & Install Updates");
    m_installButton->setCursor(Qt::PointingHandCursor);
    m_installButton->setMinimumHeight(40);
    m_installButton->setEnabled(false);

    layout->addWidget(m_checkButton, 1);
    layout->addWidget(m_installButton, 3);

    connect(m_checkButton, &QPushButton::clicked, this, &ButtonPanel::checkClicked);
    connect(m_installButton, &QPushButton::clicked, this, &ButtonPanel::installClicked);
}

void ButtonPanel::setUpdateEnabled(bool enabled)
{
    m_installButton->setEnabled(enabled);
}

void ButtonPanel::setCheckEnabled(bool enabled)
{
    m_checkButton->setEnabled(enabled);
}

void ButtonPanel::setUpdateText(const QString& text)
{
    m_installButton->setText(text);
}

void ButtonPanel::setCheckText(const QString& text)
{
    m_checkButton->setText(text);
}

void ButtonPanel::setCheckIcon(const QIcon& icon)
{
    m_checkButton->setIcon(icon);
}
