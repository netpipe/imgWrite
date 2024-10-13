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
#include <QRegularExpression>
#include <QScrollBar>
#include <qDebug>

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
    QString getDiskForVolume(const QString &volumePath);
    void executeDD(const QString &ddCommand);

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
    connect(ddProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus exitStatus){
        if(exitStatus == QProcess::CrashExit){
            outputTextEdit->append("Process crashed.");
        } else {
            outputTextEdit->append(QString("Process finished with exit code %1").arg(exitCode));
        }
    });

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
    mainLayout->addWidget(new QLabel("Output:", this));
    mainLayout->addWidget(outputTextEdit);

    setLayout(mainLayout);
    setWindowTitle("DD Image Writer");
    resize(600, 400);
}

void DDImageWriter::populateDrives() {
    driveComboBox->clear();
    currentDrives.clear();

    // List available storage drives and exclude root drive "/dev/disk0"
    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady() && storage.rootPath() != "/") {
            qDebug() << storage.displayName();

            QString volumePath = storage.rootPath();
            QString diskPath = storage.device();
qDebug() << volumePath << diskPath << storage.device() ;
QRegularExpression re("(/dev/disk\\d+)(s\\d+)?");
QRegularExpressionMatch match = re.match(diskPath);
QString baseDiskName;
if (match.hasMatch()) {
    baseDiskName = match.captured(1); // This captures /dev/diskX
  //  baseDiskName.remove("/dev/");      // Remove "/dev/" to get "diskX"
}
            // Only add valid /dev/disk entries (exclude /dev/disk0)
            if (!diskPath.isEmpty() && !diskPath.contains("/dev/disk0")) {
                QString driveDescription = QString("%1 (%2)").arg(baseDiskName, volumePath);
                driveComboBox->addItem(driveDescription, baseDiskName);
                currentDrives.insert(diskPath);
            }
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
            QString volumePath = storage.rootPath();
            QString diskPath = storage.device();
            QRegularExpression re("(/dev/disk\\d+)(s\\d+)?");
            QRegularExpressionMatch match = re.match(diskPath);
            QString baseDiskName;
            if (match.hasMatch()) {
                baseDiskName = match.captured(1); // This captures /dev/diskX
             //   baseDiskName.remove("/dev/");      // Remove "/dev/" to get "diskX"
            }
            if (!diskPath.isEmpty() && !diskPath.contains("/dev/disk0")) {
                newDrives.insert(baseDiskName);
                if (!currentDrives.contains(baseDiskName)) {
                    // New drive detected
                    QString driveDescription = QString("%1 (%2)").arg(baseDiskName, volumePath);
                    driveComboBox->addItem(driveDescription, baseDiskName);
                    outputTextEdit->append("Drive connected: " + driveDescription);
                }
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

QString DDImageWriter::getDiskForVolume(const QString &volumePath) {
    // Execute diskutil info for the given volumePath to get the device node
    QProcess diskutilProcess;
    diskutilProcess.start("diskutil", QStringList() << "info" << volumePath);
    diskutilProcess.waitForFinished();

    QString output = diskutilProcess.readAllStandardOutput();

    // Use regex to find the "Device Node" line
    QRegularExpression re(QStringLiteral("^\\s*Device Node:\\s+(\\/dev\\/disk\\d+)"));
    QRegularExpressionMatch match = re.match(output);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    return ""; // Return empty string if no match is found
}

void DDImageWriter::selectImageFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Select Image File", "", "Disk Images (*.img *.iso)");
    if (!fileName.isEmpty()) {
        imageFileLineEdit->setText(fileName);
    }
}

void DDImageWriter::startDDProcess() {
    QString drive = driveComboBox->currentData().toString();
    QString blockSize = bsComboBox->currentText();
    QString imageFile = imageFileLineEdit->text();

    if (drive.isEmpty() || imageFile.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please select both a valid drive and an image file.");
        return;
    }

    // Confirm the operation
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm Operation",
                                  QString("Are you sure you want to write the image to %1?\nThis will erase all data on the drive.")
                                      .arg(drive),
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    // Construct the dd command
    QString ddCommand = QString("sudo dd if=\"%1\" of=\"%2\" bs=%3 status=progress").arg(imageFile, drive, blockSize);

    // Use osascript to execute the dd command with administrator privileges
    QString osascriptCommand = QString("do shell script \"%1\" with administrator privileges").arg(ddCommand.replace("\"", "\\\""));

    executeDD(osascriptCommand);
}

void DDImageWriter::executeDD(const QString &ddCommand) {
    // Clear previous output
    outputTextEdit->append("Executing: " + ddCommand);

    // Use osascript to run the dd command with admin privileges
    QString fullCommand = QString("osascript -e '%1'").arg(ddCommand);

    // Start the process
    ddProcess->start("bash", QStringList() << "-c" << fullCommand);

    if (!ddProcess->waitForStarted()) {
        QMessageBox::critical(this, "Error", "Failed to start the dd process.");
        return;
    }
}

void DDImageWriter::onDDOutput() {
    QString output = ddProcess->readAllStandardOutput();
    if (!output.isEmpty()) {
        outputTextEdit->append(output.trimmed());
        // Auto-scroll to the bottom
        QScrollBar *bar = outputTextEdit->verticalScrollBar();
        bar->setValue(bar->maximum());
    }
}

void DDImageWriter::onDDError() {
    QString errorOutput = ddProcess->readAllStandardError();
    if (!errorOutput.isEmpty()) {
        outputTextEdit->append("<span style='color:red;'>Error: " + errorOutput.trimmed() + "</span>");
        // Auto-scroll to the bottom
        QScrollBar *bar = outputTextEdit->verticalScrollBar();
        bar->setValue(bar->maximum());
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    DDImageWriter window;
    window.show();

    return app.exec();
}

#include "main.moc"
