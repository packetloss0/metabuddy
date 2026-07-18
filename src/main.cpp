#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("MetaBuddy");
    QApplication::setOrganizationName("MetaBuddy");

    MainWindow w;
    w.show();

    return app.exec();
}
