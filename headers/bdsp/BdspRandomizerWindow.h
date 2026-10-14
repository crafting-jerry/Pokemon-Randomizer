#ifndef BDSPRANDOMIZERWINDOW_H
#define BDSPRANDOMIZERWINDOW_H

// ---------------------------------------------------------------------------
// BdspRandomizerWindow
// Oberflaeche fuer Pokemon Strahlender Diamant / Leuchtende Perle im gleichen
// Stil wie Schwert/Schild. Die Spieldaten kommen aus dem eigenen Dump (RomFS
// und ExeFS), die Pfade werden auf der Start-Seite angegeben und gemerkt.
// ---------------------------------------------------------------------------

#include <QCheckBox>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "bdsp_files.h"
#include "bdsp_trainers.h"
#include "bdsp_encounters.h"
#include <QComboBox>

class SwShTrainerEditor;
namespace modernui { class SegmentedControl; }

class BdspRandomizerWindow : public QWidget {
    Q_OBJECT
public:
    explicit BdspRandomizerWindow(QWidget* parent = nullptr);
    ~BdspRandomizerWindow();

    // Fuer Tests
    void setDumpPaths(const QString& romfs, const QString& exefs);
    bool dumpReady() const { return files != nullptr; }
    bool randomizeTo(const QString& outputFolder, QString* error);
    bdsp::TrainerSettings& trainerSettingsRef() { return trainerSettings; }
    bdsp::EncounterSettings& encounterSettingsRef() { return encounterSettings; }
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

    // Ausgabe direkt in den Mod-Ordner
    QCheckBox* modDirect = nullptr;
    QLineEdit* modFolderEdit = nullptr;
    QWidget* modRow = nullptr;
    void findEmulator();
    bool copyToModFolder(const QString& output, QString* error) const;

    // Vorlagen
    QJsonObject settingsJson() const;
    void applySettingsJson(const QJsonObject& json);
    void saveTemplate();
    void loadTemplate();

    // Statusleiste
    QLabel* activeInfo = nullptr;
    QLabel* detailInfo = nullptr;
    QPushButton* startButton = nullptr;

    // Trainer
    bdsp::TrainerSettings trainerSettings;
    bool ownInitialized[bdsp::GroupCount] = {};
    QCheckBox* trainerSwitch = nullptr;
    QCheckBox* typeThemeBox = nullptr;
    QWidget* trainerContent = nullptr;
    SwShTrainerEditor* baseEditor = nullptr;
    SwShTrainerEditor* groupEditors[bdsp::GroupCount] = {};
    QWidget* groupEditorBoxes[bdsp::GroupCount] = {};
    modernui::SegmentedControl* groupSelectors[bdsp::GroupCount] = {};

    // Starter & Geschenke
    bdsp::EncounterSettings encounterSettings;
    modernui::SegmentedControl* starterMode = nullptr;
    modernui::SegmentedControl* starterTypes = nullptr;
    QCheckBox* starterStages = nullptr;
    QCheckBox* starterStrength = nullptr;
    QWidget* starterRandomBox = nullptr;
    QWidget* starterWishBox = nullptr;
    QComboBox* wishCombos[3] = {};
    QCheckBox* giftsBox = nullptr;
    QCheckBox* staticsBox = nullptr;
    QCheckBox* tradesBox = nullptr;
    QCheckBox* strengthBox = nullptr;
    QCheckBox* legendBox = nullptr;
    QWidget* buildStartersPage();
    void refreshStartersPage();
    void fillStarterCombos();
    bool lastHadExefsPatch = false;

    QString lastSeed;
    bdsp::DumpCheck check;
    std::unique_ptr<bdsp::GameFiles> files;

    QWidget* buildSidebar();
    QWidget* buildBottomBar();
    QWidget* wrapPage(const QString& title, const QString& subtitle, QWidget* content);
    QWidget* buildStartPage();
    QWidget* buildTrainerPage();
    QWidget* buildComingSoonPage(const QString& title, const QString& subtitle, const QString& text);
    QHBoxLayout* pathRow(QWidget* parent, const QString& caption, const QString& info, QLineEdit*& edit, bool romfs);
    void refreshTrainerPage();
    QStringList activeAreas() const;

    void browse(bool romfs);
    void checkPaths();
    void showMessages();
    void updateStatus();
    void startRandomizer();
    void saveSettings() const;
    void loadSettings();
};

#endif // BDSPRANDOMIZERWINDOW_H
