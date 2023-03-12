#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "portselectiondialog.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include "yetty.version.h"
#include <QApplication>
#include <QKeyEvent>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QVBoxLayout>
#include <kstandardshortcut.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serialPort(new QSerialPort(parent))
{

    ui->setupUi(this);

    editor = KTextEditor::Editor::instance();
    doc = editor->createDocument(this);

    view = doc->createView(this);
    doc->setHighlightingMode("Log File (advanced)");

    ui->verticalLayout->insertWidget(0, view);

    const auto [port, baud] = getPortFromUser(QApplication::arguments().size() == 3);
    connectToDevice(port, baud);

    qInfo() << "Opening: " << port << " " << baud;

    connect(ui->actionConnectToDevice, &QAction::triggered, this, &MainWindow::handleConnectAction);
    ui->actionConnectToDevice->setShortcut(QKeySequence::Open);
    ui->actionConnectToDevice->setIcon(QIcon::fromTheme("document-open"));

    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::handleSaveAction);
    ui->actionSave->setIcon(QIcon::fromTheme("document-save"));

    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::handleQuitAction);
    ui->actionQuit->setShortcut(QKeySequence::Quit);
    ui->actionQuit->setIcon(QIcon::fromTheme("application-exit"));

    connect(ui->actionClear, &QAction::triggered, this, &MainWindow::handleClearAction);
    ui->actionClear->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_K));
    ui->actionClear->setIcon(QIcon::fromTheme("edit-clear-all"));

    connect(ui->pushButton, &QPushButton::pressed, this, &MainWindow::handleScrollToEnd);

    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::handleAboutAction);
    ui->actionAbout->setIcon(QIcon::fromTheme("help-about"));

    ui->pushButton->setIcon(QIcon::fromTheme("go-bottom"));

    connect(serialPort, &QSerialPort::readyRead, this, &MainWindow::handleReadyRead);
    connect(serialPort, &QSerialPort::errorOccurred, this, &MainWindow::handleError);
}

MainWindow::~MainWindow()
{
    delete ui;
}

std::pair<QString, int> MainWindow::getPortFromUser(const bool useCmdLineArgs) const
{

    if (useCmdLineArgs) {
        const auto args = QApplication::arguments();

        Q_ASSERT(args.size() == 3);

        bool ok {};
        const auto portInt = args[2].toInt(&ok);
        if (!ok) {
            throw std::runtime_error("Invalid port: " + args[2].toStdString());
        }
        return { args[1], portInt };

    } else {
        PortSelectionDialog dlg;
        if (!dlg.exec()) {
            qInfo() << "No port selection made";
            throw std::runtime_error("No selection made");
        }
        const auto baud = dlg.getSelectedBaud();
        return { dlg.getSelectedPortLocation(), baud };
    }
}

void MainWindow::handleReadyRead()
{
    const auto d = serialPort->readAll();

    doc->setReadWrite(true);
    doc->insertText(doc->documentEnd(), d);
    doc->setReadWrite(false);
}

void MainWindow::handleError(const QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ReadError) {
        const auto errMsg = "ERROR: " + QVariant::fromValue(error).toString().toStdString();
        qCritical() << error;
        throw std::runtime_error(errMsg);
    }
}

void MainWindow::handleSaveAction()
{
    doc->documentSave();
}

void MainWindow::handleClearAction()
{
    doc->setReadWrite(true);
    doc->clear();
    doc->setReadWrite(false);
}

void MainWindow::handleQuitAction()
{
    qInfo() << "Exiting application";
    QCoreApplication::quit();
}

void MainWindow::handleScrollToEnd()
{
    view->setFocus();
    QCoreApplication::postEvent(view, new QKeyEvent(QEvent::KeyPress, Qt::Key_End, Qt::ControlModifier));
}

void MainWindow::handleAboutAction()
{
    QMessageBox::about(this, "About", QString("%1\t\t\n%2\t").arg(PROJECT_NAME, PROJECT_VERSION));
}

void MainWindow::handleConnectAction()
{
    serialPort->close();
    const auto [port, baud] = getPortFromUser(false);
    qInfo() << "Connecting to new port: " << port << " " << baud;
    handleClearAction();
    connectToDevice(port, baud);
}

void MainWindow::connectToDevice(const QString& port, const int baud)
{
    Q_ASSERT(!serialPort->isOpen());
    serialPort->setPortName(port);
    serialPort->setBaudRate(baud);

    if (serialPort->open(QIODevice::ReadOnly)) {
        //
    } else {
        // We allow the user to open non-serial, static plain text files.
        qInfo() << "Opening as ordinary file";
        QFile file(port);
        if (!file.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("Failed to open file: " + port.toStdString() + "\n" + strerror(errno));
        }
        doc->setText(file.readAll());
    }
}
