#include <QApplication>
#include <QDir>
#include <QScreen>
#include <QMutex>
#include <QSettings>
#include <QIcon>
#include <QDateTime>
#include <QFile>
#include "headers/qtwindows_headers/mainwindow.h"

// Log-Datei (logs/logs.log), wird von mehreren Threads beschrieben
QFile logFile;
QMutex logMutex;

void customMessageHandler(QtMsgType type, const QMessageLogContext&, const QString &msg) {
    QMutexLocker locker(&logMutex); // Ensure thread safety

    const QString logMessage = QString("[%1] %2")
                                   .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"), msg);

    #ifdef QT_DEBUG
        // In debug mode, also log to the terminal
        switch (type) {
        case QtDebugMsg:
        case QtInfoMsg:
            fprintf(stdout, "Debug: %s\n", logMessage.toLocal8Bit().constData());
            break;
        case QtWarningMsg:
            fprintf(stdout, "Warning: %s\n", logMessage.toLocal8Bit().constData());
            break;
        case QtCriticalMsg:
            fprintf(stderr, "Critical: %s\n", logMessage.toLocal8Bit().constData());
            break;
        case QtFatalMsg:
            fprintf(stderr, "Fatal: %s\n", logMessage.toLocal8Bit().constData());
            break;
        }
    #endif

    // Immer in die Datei schreiben. Nur Warnungen und Fehler sofort auf die Platte bringen
    // (jede Debug-Zeile einzeln zu schreiben bremst das Randomisieren stark aus).
    if (logFile.isOpen()) {
        logFile.write(logMessage.toUtf8());
        logFile.write("\n");
        if (type != QtDebugMsg && type != QtInfoMsg) {
            logFile.flush();
        }
    }
    if (type == QtFatalMsg) {
        logFile.close();
        abort();
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

#ifdef Q_OS_WIN
    // Alle Daten (SV_FLATBUFFERS, assets, thirdparty, logs) liegen neben der .exe. Arbeitsordner auf den
    // Programmordner setzen, damit der Randomizer auch per Verknuepfung oder aus einem anderen Ordner startet.
    QDir::setCurrent(QCoreApplication::applicationDirPath());
#endif

    // Set application-wide icon
    app.setWindowIcon(QIcon("assets/icons/main.ico"));

    #ifdef QT_DEBUG
        QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() + "/../plugins");  // Development path
    #else
        QCoreApplication::addLibraryPath("./plugins");  // Deployment path
    #endif

    MainWindow mainWindow;
    mainWindow.setWindowTitle("Pokémon Randomizer");
    mainWindow.setWindowIcon(QIcon("assets/icons/main.ico"));

    // Get the screen where the mouse is currently located
    QScreen *screen = nullptr;
    QList<QScreen *> screens = QApplication::screens();
    QCursor cursor;

    // Get the current screen based on mouse cursor position
    QPoint cursorPos = cursor.pos();

    for (QScreen *s : screens) {
        if (s->geometry().contains(cursorPos)) {
            screen = s;
            break;
        }
    }

    if (!screen) {
        screen = QApplication::primaryScreen();
    }

    QRect screenGeometry = screen->availableGeometry();

    // Calculate the center point
    int x = (screenGeometry.width() - mainWindow.width()) / 2 + screenGeometry.x();
    int y = (screenGeometry.height() - mainWindow.height()) / 2 + screenGeometry.y();

    // Move the window to the center
    mainWindow.move(x, y);

    // Log-Datei: bei jedem Start neu (die vom letzten Start bleibt als logs-alt.log erhalten),
    // damit sie nicht endlos waechst
    QDir().mkpath("logs");
    QFile::remove("logs/logs-alt.log");
    QFile::rename("logs/logs.log", "logs/logs-alt.log");
    logFile.setFileName("logs/logs.log");
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qCritical() << "Failed to open log file!";
    }

    // Creates Buffer of qDebugs/qCriticals/qFatals into logs folder for debug and release.
    qInstallMessageHandler(customMessageHandler);

    // Creates settings for Randomizer Options (things to be remember between launches)
    QSettings settings("Pokemon Randomizer", "Main");

    // Default to true for "Always on Top"
    bool alwaysOnTopValue = settings.value("AlwaysOnTop", false).toBool();
    mainWindow.alwaysOnTop(alwaysOnTopValue);

    const int result = app.exec();
    qInstallMessageHandler(nullptr);
    logFile.close(); // restliche Zeilen schreiben
    return result;
}
