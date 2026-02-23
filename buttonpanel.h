#pragma once
#include <QWidget>
#include <QIcon>

class QPushButton;

class ButtonPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ButtonPanel(QWidget *parent = nullptr);

    // Main Install Button Control
    void setUpdateEnabled(bool enabled);
    void setUpdateText(const QString& text);

    // Check/Return Button Control
    void setCheckEnabled(bool enabled);
    void setCheckText(const QString& text);
    void setCheckIcon(const QIcon& icon);

signals:
    void checkClicked();
    void installClicked();

private:
    QPushButton* m_checkButton;
    QPushButton* m_installButton;
};
