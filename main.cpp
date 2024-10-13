#include <QApplication>
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QProcess>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QStorageInfo>
#include <QMessageBox>
#include <QTimer>
#include <QSet>

class DDImageWriter : public QWidget {
    Q_OBJECT

public:
    DDImageWriter(QWidget *parent = nullptr);

private slots:
    void selectImageFile();
    void startDDProcess();
    void onDDOutput();
    void onDDError();
    void scanForDrives();

private:
    void populateDrives();

    QComboBox *driveComboBox;
    QComboBox *bsComboBox;
    QLineEdit *imageFileLineEdit;
    QTextEdit *outputTextEdit;
    QProcess *ddProcess;
    QTimer *driveScanTimer;
    QSet<QString> currentDrives; // To keep track of existing drives
};

DDImageWriter::DDImageWriter(QWidget *parent)
    : QWidget(parent), ddProcess(new QProcess(this)), driveScanTimer(new QTimer(this)) {

    // Create UI elements
    QLabel *driveLabel = new QLabel("Select Drive:", this);
    driveComboBox = new QComboBox(this);
    populateDrives();

    QLabel *bsLabel = new QLabel("Select Block Size:", this);
    bsComboBox = new QComboBox(this);
    bsComboBox->addItems({"512", "1024", "4096", "8192", "16384"});

    QLabel *imageFileLabel = new QLabel("Image File:", this);
    imageFileLineEdit = new QLineEdit(this);
    QPushButton *browseButton = new QPushButton("Browse...", this);
    connect(browseButton, &QPushButton::clicked, this, &DDImageWriter::selectImageFile);

    QPushButton *startButton = new QPushButton("Start", this);
    connect(startButton, &QPushButton::clicked, this, &DDImageWriter::startDDProcess);

    outputTextEdit = new QTextEdit(this);
    outputTextEdit->setReadOnly(true);

    // Connect process signals
    connect(ddProcess, &QProcess::readyReadStandardOutput, this, &DDImageWriter::onDDOutput);
    connect(ddProcess, &QProcess::readyReadStandardError, this, &DDImageWriter::onDDError);

    // Set up the timer to scan for drives every 5 seconds
    connect(driveScanTimer, &QTimer::timeout, this, &DDImageWriter::scanForDrives);
    driveScanTimer->start(5000); // 5000 milliseconds = 5 seconds

    // Initial drive scan
    scanForDrives();

    // Layout setup
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *driveLayout = new QHBoxLayout();
    driveLayout->addWidget(driveLabel);
    driveLayout->addWidget(driveComboBox);

    QHBoxLayout *bsLayout = new QHBoxLayout();
    bsLayout->addWidget(bsLabel);
    bsLayout->addWidget(bsComboBox);

    QHBoxLayout *imageFileLayout = new QHBoxLayout();
    imageFileLayout->addWidget(imageFileLabel);
    imageFileLayout->addWidget(imageFileLineEdit);
    imageFileLayout->addWidget(browseButton);

    mainLayout->addLayout(driveLayout);
    mainLayout->addLayout(bsLayout);
    mainLayout->addLayout(imageFileLayout);
    mainLayout->addWidget(startButton);
    mainLayout->addWidget(outputTextEdit);

    setLayout(mainLayout);
}

void DDImageWriter::populateDrives() {
    driveComboBox->clear();
    currentDrives.clear();

    // List available storage drives and exclude root drive "/"
    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady() && storage.rootPath() != "/") {
            QString drivePath = storage.rootPath();
            QString driveDescription = QString("%1 (%2)").arg(storage.displayName(), drivePath);
            driveComboBox->addItem(driveDescription, drivePath);
            currentDrives.insert(drivePath);
        }
    }

    // If no drives are found, inform the user
    if (driveComboBox->count() == 0) {
        driveComboBox->addItem("No drives available", "");
    }
}

void DDImageWriter::scanForDrives() {
    QSet<QString> newDrives;

    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady() && storage.rootPath() != "/") {
            QString drivePath = storage.rootPath();
            newDrives.insert(drivePath);
            if (!currentDrives.contains(drivePath)) {
                // New drive detected
                QString driveDescription = QString("%1 (%2)").arg(storage.displayName(), drivePath);
                driveComboBox->addItem(driveDescription, drivePath);
                outputTextEdit->append("Drive connected: " + driveDescription);
            }
        }
    }

    // Check for removed drives
    QSet<QString> removedDrives = currentDrives - newDrives;
    foreach (const QString &drive, removedDrives) {
        // Find and remove the drive from the combo box
        for (int i = 0; i < driveComboBox->count(); ++i) {
            if (driveComboBox->itemData(i).toString() == drive) {
                QString driveDescription = driveComboBox->itemText(i);
                driveComboBox->removeItem(i);
                outputTextEdit->append("Drive disconnected: " + driveDescription);
                break;
            }
        }
    }

    currentDrives = newDrives;

    // If no drives are present, inform the user
    if (currentDrives.isEmpty()) {
        driveComboBox->clear();
        driveComboBox->addItem("No drives available", "");
    }
}

void DDImageWriter::selectImageFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Select Image File", "", "Disk Images (*.img *.iso)");
    if (!fileName.isEmpty()) {
        imageFileLineEdit->setText(fileName);
    }
}

void DDImageWriter::startDDProcess() {
    QString drive = driveComboBox->currentData().toString();
    QString bs = bsComboBox->currentText();
    QString imageFile = imageFileLineEdit->text();

    if (drive.isEmpty() || imageFile.isEmpty() || drive == "/") {
        QMessageBox::warning(this, "Input Error", "Please select both a valid drive and an image file.");
        return;
    }

    // Use osascript with do shell script for secure password handling on macOS
    QString ddCommand = QString("dd if='%1' of='%2' bs=%3 status=progress").arg(imageFile, drive, bs);

    // Construct the osascript command to execute dd with administrator privileges
    QString osascriptCommand = QString(R"(
        /usr/bin/osascript -e 'do shell script "%1" with administrator privileges'
    )").arg(ddCommand.replace("\"", "\\\"")); // Escape double quotes

    outputTextEdit->append("Executing: " + ddCommand);
    ddProcess->start("bash", QStringList() << "-c" << osascriptCommand);
}

void DDImageWriter::onDDOutput() {
    QString output = ddProcess->readAllStandardOutput();
    outputTextEdit->append(output);
}

void DDImageWriter::onDDError() {
    QString errorOutput = ddProcess->readAllStandardError();
    outputTextEdit->append("Error: " + errorOutput);
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    DDImageWriter window;
    window.setWindowTitle("DD Image Writer");
    window.resize(600, 400);
    window.show();

    return app.exec();
}

#include "main.moc"
