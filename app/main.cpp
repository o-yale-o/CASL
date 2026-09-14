#include "MainWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("CASLStudio");
    app.setApplicationName("CASLStudio");
    CMainWindow wndMain;
    wndMain.show();
    return app.exec();
}
