#include <QApplication>
#include <QMessageBox>

#include "mainwindow.h"
#include "yetty.version.h"

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    QApplication::setOrganizationDomain("aa55.dev");
    QApplication::setApplicationName(PROJECT_NAME);
    // This sets the icon in wayland
    QApplication::setDesktopFileName(PROJECT_DOMAIN);
    // This is needed to show the icon in the "About" window
    a.setWindowIcon(QIcon::fromTheme(PROJECT_DOMAIN));

    try {
        MainWindow w;
        w.show();
        return a.exec();
    } catch (std::exception& e) {
        QMessageBox::critical(nullptr, "Error", e.what());
    }
}
