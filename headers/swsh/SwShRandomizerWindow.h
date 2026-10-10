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
#include "swsh_trainers.h"
#include "swsh_encounters.h"
#include <QComboBox>

class SwShTrainerEditor;
namespace modernui { class SegmentedControl; }

class SwShRandomizerWindow : public QWidget {
    Q_OBJECT
public:
    explicit SwShRandomizerWindow(QWidget* parent = nullptr);
    ~SwShRandomizerWindow();

    // Fuer Tests: Pfade setzen und pruefen
    void setDumpPaths(const QString& romfs, const QString& exefs);
    bool dumpReady() const { return files != nullptr; }
    // Randomisiert und schreibt romfs + Spoiler-Log nach outputFolder. Fehlertext in error.
    bool randomizeTo(const QString& outputFolder, QString* error);
    swsh::TrainerSettings& trainerSettingsRef() { return trainerSettings; }
    swsh::EncounterSettings& encounterSettingsRef() { return encounterSettings; }
    void refreshAllPages();
    void showPage(int index);

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

    // Trainer
    swsh::TrainerSettings trainerSettings;
    bool ownInitialized[swsh::GroupCount] = {};
    QCheckBox* trainerSwitch = nullptr;
    QCheckBox* typeThemeBox = nullptr;
    QWidget* trainerContent = nullptr;
    SwShTrainerEditor* baseEditor = nullptr;
    SwShTrainerEditor* groupEditors[swsh::GroupCount] = {};
    QWidget* groupEditorBoxes[swsh::GroupCount] = {};
    modernui::SegmentedControl* groupSelectors[swsh::GroupCount] = {};

    // Starter & Geschenke
    swsh::EncounterSettings encounterSettings;
    modernui::SegmentedControl* starterMode = nullptr;
    modernui::SegmentedControl* starterTypes = nullptr;
    QCheckBox* starterStages = nullptr;
    QCheckBox* starterStrength = nullptr;
    QWidget* starterRandomBox = nullptr;
    QWidget* starterWishBox = nullptr;
    QComboBox* wishCombos[3] = {};
    QCheckBox* giftsBox = nullptr;
    QCheckBox* staticsBox = nullptr;
    QCheckBox* overworldBox = nullptr;
    QCheckBox* tradesBox = nullptr;
    QCheckBox* strengthBox = nullptr;
    QCheckBox* legendBox = nullptr;
    QWidget* buildStartersPage();
    void refreshStartersPage();
    void fillStarterCombos();
    QStringList activeAreas() const;

    QString lastSeed;
    swsh::DumpCheck check;
    std::unique_ptr<swsh::GameFiles> files;

    QWidget* buildSidebar();
    QWidget* buildBottomBar();
    QWidget* wrapPage(const QString& title, const QString& subtitle, QWidget* content);
    QWidget* buildStartPage();
    QWidget* buildTrainerPage();
    void refreshTrainerPage();
    void startRandomizer();
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
