#include <QApplication>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // 1.0 Application Metadata
    QApplication::setApplicationName("Uptater");
    QApplication::setApplicationVersion("1.0.1.2");
    QApplication::setWindowIcon(QIcon(":/icon.png"));

    MainWindow window;
    window.setWindowTitle("Uptater");
    window.resize(800, 600);
    window.show();

    return app.exec();
}
