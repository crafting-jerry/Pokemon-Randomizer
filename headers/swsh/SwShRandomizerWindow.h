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
#include "swsh_wild.h"
#include "swsh_extras.h"
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
    swsh::WildSettings& wildSettingsRef() { return wildSettings; }
    swsh::RaidSettings& raidSettingsRef() { return raidSettings; }
    swsh::ItemSettings& itemSettingsRef() { return itemSettings; }
    swsh::PokemonDataSettings& dataSettingsRef() { return dataSettings; }
    swsh::FacilitySettings& facilitySettingsRef() { return facilitySettings; }
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

    // Wilde Pokemon
    swsh::WildSettings wildSettings;
    QCheckBox* wildSwitch = nullptr;
    QWidget* wildContent = nullptr;
    modernui::SegmentedControl* wildMode = nullptr;
    QCheckBox* wildLevel = nullptr;
    QCheckBox* wildType = nullptr;
    QCheckBox* wildStrength = nullptr;
    QCheckBox* wildLegends = nullptr;
    QWidget* buildWildPage();
    void refreshWildPage();

    // Dyna-Raids, Items, Pokemon-Daten
    swsh::RaidSettings raidSettings;
    swsh::ItemSettings itemSettings;
    swsh::PokemonDataSettings dataSettings;
    QCheckBox* raidSwitch = nullptr;
    QWidget* raidContent = nullptr;
    QCheckBox* raidLevel = nullptr;
    QCheckBox* raidGmax = nullptr;
    QCheckBox* raidType = nullptr;
    QCheckBox* raidStrength = nullptr;
    QCheckBox* raidLegends = nullptr;
    modernui::SegmentedControl* itemMode = nullptr;
    QCheckBox* itemField = nullptr;
    QCheckBox* itemHidden = nullptr;
    QCheckBox* itemShops = nullptr;
    QCheckBox* itemTrainers = nullptr;
    QCheckBox* dataTradeEvos = nullptr;
    QCheckBox* dataAbilities = nullptr;
    QCheckBox* dataTypes = nullptr;
    QCheckBox* dataStats = nullptr;
    QCheckBox* dataMoves = nullptr;
    QCheckBox* dataFamilies = nullptr;
    modernui::SegmentedControl* dataTMs = nullptr;
    swsh::FacilitySettings facilitySettings;
    QCheckBox* lairBox = nullptr;
    QCheckBox* lairLegendsBox = nullptr;
    QCheckBox* towerBox = nullptr;
    QCheckBox* towerEvolvedBox = nullptr;
    QCheckBox* towerLegendsBox = nullptr;
    QWidget* buildFacilityPage();
    QWidget* buildRaidPage();
    QWidget* buildItemPage();
    QWidget* buildDataPage();
    void refreshExtraPages();

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
