#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "portselectiondialog.h"
#include "triggersetupdialog.h"
#include "yetty.version.h"

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QApplication>
#include <QKeyEvent>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QSound>
#include <QTimer>
#include <QVBoxLayout>
#include <kstandardshortcut.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serialPort(new QSerialPort(parent))
    , sound(new QSound(":/notify.wav", parent))
    , timer(new QTimer(parent))
{

    ui->setupUi(this);

    editor = KTextEditor::Editor::instance();
    doc = editor->createDocument(this);

    view = doc->createView(this);
    doc->setHighlightingMode("Log File (advanced)");

    ui->verticalLayout->insertWidget(0, view);

    setWindowTitle(PROJECT_NAME);

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

    connect(ui->scrollToEndButton, &QPushButton::pressed, this, &MainWindow::handleScrollToEnd);
    ui->scrollToEndButton->setIcon(QIcon::fromTheme("go-bottom"));

    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::handleAboutAction);
    ui->actionAbout->setIcon(QIcon::fromTheme("help-about"));

    connect(ui->actionTrigger, &QAction::triggered, this, &MainWindow::handleTriggerSetupAction);
    ui->actionTrigger->setIcon(QIcon::fromTheme("mail-thread-watch"));

    connect(ui->startStopButton, &QPushButton::pressed, this, &MainWindow::handleStartStopButton);

    connect(serialPort, &QSerialPort::readyRead, this, &MainWindow::handleReadyRead);
    connect(serialPort, &QSerialPort::errorOccurred, this, &MainWindow::handleError);

    connect(timer, &QTimer::timeout, this, &MainWindow::handleRetryConnection);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setProgramState(const ProgramState newState)
{
    if (newState == currentProgramState) {
        return;
    }

    if (newState == ProgramState::Started) {
        qInfo() << "Program started";
        ui->startStopButton->setText("Stop");
        ui->startStopButton->setIcon(QIcon::fromTheme("media-playback-stop"));
    } else if (newState == ProgramState::Stopped) {
        qInfo() << "Program stopped";
        ui->startStopButton->setText("Start");
        ui->startStopButton->setIcon(QIcon::fromTheme("media-playback-start"));
    } else {
        throw std::runtime_error("Unknown state");
    }

    currentProgramState = newState;
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

    if (triggerActive) {
        if (d.contains(triggerKeyword)) {
            triggerMatchCount++;
            ui->statusbar->showMessage(QString("%1 matches").arg(triggerMatchCount), 3000);
            sound->play();
        }
    }

    doc->setReadWrite(true);
    doc->insertText(doc->documentEnd(), d);
    doc->setReadWrite(false);
}

void MainWindow::handleError(const QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::SerialPortError::NoError) {
        return;
    }
    auto errMsg = "Error: " + QVariant::fromValue(error).toString();

    const auto& portName = "/dev/" + serialPort->portName();

    if (!QFile::exists(portName)) {
        errMsg += QString(": %1 detached").arg(portName);
    }
    qCritical() << "Serial port error: " << error;
    ui->statusbar->showMessage(errMsg);
    setProgramState(ProgramState::Stopped);

    if (serialPort->isOpen()) {
        serialPort->close();
    }

    if (!timer->isActive()) {
        timer->start(1000);
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
    handleClearAction();
    connectToDevice(port, baud);
}

void MainWindow::handleTriggerSetupAction()
{
    if (!triggerSetupDialog) {
        triggerSetupDialog = new TriggerSetupDialog(this);
        connect(triggerSetupDialog, &QDialog::finished, this, &MainWindow::handleTriggerSetupDialogFinished);
    }
    triggerSetupDialog->open();
}

void MainWindow::handleTriggerSetupDialogFinished(int result)
{
    if (result == QDialog::Accepted) {

        const auto newKeyword = triggerSetupDialog->getKeyword().toUtf8();
        if (newKeyword != triggerKeyword) {
            qInfo() << "Setting new trigger keyword: " << triggerKeyword << " " << newKeyword;
            triggerKeyword = newKeyword;
            triggerActive = !newKeyword.isEmpty();
            triggerMatchCount = 0;
        }
    }
}

void MainWindow::handleStartStopButton()
{
    if (currentProgramState == ProgramState::Started) {
        qInfo() << "Closing connection on button press";
        serialPort->close();
        setProgramState(ProgramState::Stopped);
    } else {
        qInfo() << "Starting connection on button press";
        connectToDevice(serialPort->portName(), serialPort->baudRate());
    }
}

void MainWindow::handleRetryConnection()
{
    if (serialPort->error() != QSerialPort::NoError) {
        qInfo() << "Retrying connection";
        connectToDevice(serialPort->portName(), serialPort->baudRate(), false);
    }
}

void MainWindow::connectToDevice(const QString& port, const int baud, const bool showMsgOnOpenErr)
{
    serialPort->setPortName(port);
    serialPort->setBaudRate(baud);

    setWindowTitle(PROJECT_NAME + QString(" ") + port);
    ui->portInfoLabel->setText(QString("%1 │ %2").arg(serialPort->portName(), QString::number(serialPort->baudRate())));

    qInfo() << "Connecting to: " << port << baud;
    serialPort->clearError();
    if (serialPort->open(QIODevice::ReadOnly)) {
        ui->startStopButton->setEnabled(true);
        ui->statusbar->showMessage("Running...");
        setProgramState(ProgramState::Started);
    } else {
        // We allow the user to open non-serial, static plain text files.
        qInfo() << "Opening as ordinary file";
        QFile file(port);
        if (!file.open(QIODevice::ReadOnly) && showMsgOnOpenErr) {
            QMessageBox::warning(this,
                tr("Failed to open file"),
                tr("Failed to open file") + ": " + port + ' ' + strerror(errno));
        }
        doc->setText(file.readAll());
        ui->startStopButton->setEnabled(false);
    }
}
