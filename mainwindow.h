#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace KTextEditor {
class Editor;
class Document;
class View;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow* ui {};

    KTextEditor::Editor* editor {};
    KTextEditor::Document* doc {};
    KTextEditor::View* view {};

    [[nodiscard]] std::pair<QString, int> getPortFromUser(const bool useCmdLineArgs) const;

private slots:
    void handleReadyRead();
    void handleError(const QSerialPort::SerialPortError error);

    void handleSaveAction();
    void handleClearAction();
    void handleQuitAction();
    void handleScrollToEnd();
    void handleAboutAction();
    void handleConnectAction();

private:
    void connectToDevice(const QString& port, const int baud);
    QSerialPort* serialPort {};
};
#endif // MAINWINDOW_H
