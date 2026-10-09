#ifndef SWSHRANDOMIZERWINDOW_H
#define SWSHRANDOMIZERWINDOW_H

// ---------------------------------------------------------------------------
// SwShRandomizerWindow
// Oberflaeche fuer Pokemon Schwert/Schild im gleichen Stil wie Karmesin/Purpur.
// Die Spieldaten kommen aus dem eigenen Dump (RomFS und ExeFS), die Pfade werden
// auf der Start-Seite angegeben und gemerkt.
// ---------------------------------------------------------------------------

#include <QWidget>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>

#include "swsh_files.h"

class SwShRandomizerWindow : public QWidget {
    Q_OBJECT
public:
    explicit SwShRandomizerWindow(QWidget* parent = nullptr);
    ~SwShRandomizerWindow();

    // Fuer Tests: Pfade setzen und pruefen
    void setDumpPaths(const QString& romfs, const QString& exefs);
    bool dumpReady() const { return files != nullptr; }

signals:
    void backRequested();

private:
    QListWidget* nav = nullptr;
    QStackedWidget* pages = nullptr;

    // Spieldateien
    QLineEdit* romfsEdit = nullptr;
    QLineEdit* exefsEdit = nullptr;
    QLabel* gameLabel = nullptr;
    QLabel* versionLabel = nullptr;
    QLabel* contentLabel = nullptr;
    QVBoxLayout* messageBox = nullptr;
    QWidget* messageArea = nullptr;

    // Allgemein
    QLineEdit* seedEdit = nullptr;
    QCheckBox* spoilerLog = nullptr;

    // Statusleiste
    QLabel* activeInfo = nullptr;
    QLabel* detailInfo = nullptr;
    QPushButton* startButton = nullptr;

    swsh::DumpCheck check;
    std::unique_ptr<swsh::GameFiles> files;

    QWidget* buildSidebar();
    QWidget* buildBottomBar();
    QWidget* wrapPage(const QString& title, const QString& subtitle, QWidget* content);
    QWidget* buildStartPage();
    QWidget* buildComingSoonPage(const QString& title, const QString& subtitle, const QString& text);
    QHBoxLayout* pathRow(QWidget* parent, const QString& caption, const QString& info, QLineEdit*& edit,
                         bool romfs);

    void browse(bool romfs);
    void checkPaths();
    void showMessages();
    void updateStatus();
    void saveSettings() const;
    void loadSettings();
};

#endif // SWSHRANDOMIZERWINDOW_H
