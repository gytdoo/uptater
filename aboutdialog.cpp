#include "aboutdialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QApplication> // <--- NEW: Needed to read global app data

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("About Uptater");
    setFixedSize(350, 360);

    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(10);

    auto *titleLabel = new QLabel("Tman's Uptater", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 16pt; font-weight: bold;");

    auto *iconLabel = new QLabel(this);
    QPixmap iconPixmap(":/icon.png");
    iconLabel->setPixmap(iconPixmap.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);

    auto *linkLabel = new QLabel("<a href='https://github.com/placeholder/uptater' style='color: #4CAF50; text-decoration: none;'>View Project on GitHub</a>", this);
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setOpenExternalLinks(true);

    // --- NEW: Dynamically fetch the version set in main.cpp ---
    QString versionText = QString("Version %1").arg(QApplication::applicationVersion());
    auto *versionLabel = new QLabel(versionText, this);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet("color: #888;");

    auto *closeBtn = new QPushButton("Close", this);
    closeBtn->setFixedWidth(100);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    layout->addWidget(titleLabel);
    layout->addWidget(iconLabel);
    layout->addWidget(linkLabel);
    layout->addWidget(versionLabel);
    layout->addSpacing(10);
    layout->addWidget(closeBtn, 0, Qt::AlignCenter);
}
