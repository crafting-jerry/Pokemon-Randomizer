#ifndef MODERNRANDOMIZERWINDOW_H
#define MODERNRANDOMIZERWINDOW_H

// ---------------------------------------------------------------------------
// ModernRandomizerWindow
// Deutsche Oberflaeche fuer Karmesin/Purpur mit Seitenleiste. Die Einstellungen
// werden vor dem Start in den Randomizer-Kern (SVRandomizerRunner) uebertragen.
// ---------------------------------------------------------------------------

#include <QWidget>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QComboBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <functional>
#include <memory>
#include <vector>

#include <QPushButton>

#include "modern_widgets.h"
#include "edit_pages.h"
#include "trainer_settings_editor.h"
#include "../sv_randomizer_headers/sv_runner.h"
#include "../sv_randomizer_headers/sv_session.h"

class ModernRandomizerWindow : public QWidget {
    Q_OBJECT
public:
    explicit ModernRandomizerWindow(QWidget* parent = nullptr);
    ~ModernRandomizerWindow();

signals:
    void backRequested();

private:
    enum GroupMode { SameAsBase = 0, Own = 1, KeepOriginal = 2 };

    struct GroupState {
        QString id;            // Schluessel fuer Vorlagen
        QString region;        // "paldea", "kitakami", "blueberry"
        QString title;
        QString info;
        trainerSettings* target = nullptr; // Gruppe im Randomizer
        bool inRegionAll = true;           // false: "Alle" der Region enthaelt diese Gruppe nicht
        int mode = SameAsBase;
        bool ownInitialized = false;
        trainerSettings own;

        modernui::SegmentedControl* selector = nullptr;
        QWidget* editorBox = nullptr;
        TrainerSettingsEditor* editor = nullptr;
        QWidget* row = nullptr;
    };

    std::unique_ptr<SVRandomizerRunner> runner; // Randomizer-Kern (vor code, wegen der Initialisierung)
    SVRandomizerCode& code;

    // Seitenaufbau
    QListWidget* nav = nullptr;
    QStackedWidget* pages = nullptr;
    QLabel* seedInfo = nullptr;
    QLabel* activeInfo = nullptr;

    // Start-Seite
    QLineEdit* seedEdit = nullptr;
    QCheckBox* autoPatch = nullptr;
    QCheckBox* spoilerLog = nullptr;

    // Ausgabe direkt in den Mod-Ordner
    QCheckBox* modDirect = nullptr;
    QLineEdit* modFolderEdit = nullptr;
    QWidget* modRow = nullptr;
    QString lastSeed;
    void findEmulator();
    void saveOutputSettings() const;
    void loadOutputSettings();
    void writeInfoFile(const QString& folder, const QString& seedText) const;

    // Bearbeiten-Modus (Ergebnis ansehen und aendern, Original bearbeiten, Mod oeffnen)
    modernui::EditWorkspace* workspace = nullptr;
    std::unique_ptr<svedit::EditSession> session;
    QString sessionSeed;
    bool editMode = false;
    bool editedByHand = false;
    QPushButton* backButton = nullptr;
    QPushButton* startButton = nullptr;
    QPushButton* editOriginalButton = nullptr;
    QPushButton* openModButton = nullptr;
    QLabel* sidebarSubtitle = nullptr;
    void editOriginal();
    void openMod();
    void enterEditMode();
    bool leaveEditMode(bool ask);
    void exportAndFinish();

    // Trainer-Seite
    bool trainersEnabled = false;
    bool regionPaldea = true;
    bool regionKitakami = true;
    bool regionBlueberry = true;
    bool typeTheme = true;
    trainerSettings base;
    TrainerSettingsEditor* baseEditor = nullptr;
    QCheckBox* masterSwitch = nullptr;
    QCheckBox* paldeaBox = nullptr;
    QCheckBox* kitakamiBox = nullptr;
    QCheckBox* blueberryBox = nullptr;
    QCheckBox* typeThemeBox = nullptr;
    QWidget* trainerContent = nullptr;
    std::vector<std::unique_ptr<GroupState>> groups;

    QWidget* buildSidebar();
    QWidget* buildBottomBar();
    QWidget* wrapPage(const QString& title, const QString& subtitle, QWidget* content);
    QWidget* buildStartPage();
    QWidget* buildTrainerPage();
    QWidget* buildStartersPage();
    QWidget* buildWildsPage();
    QWidget* buildFixedPage();
    QWidget* buildPersonalPage();
    QWidget* buildItemsPage();
    QWidget* buildRaidsBossesPage();

    // --- Verknuepfung von Schaltern mit Feldern des Randomizers ---
    struct BoolBinding {
        QString key;
        bool* field;
        QCheckBox* box;
        std::function<void(bool)> extra;
        bool defaultValue = false; // Wert, wenn eine Vorlage den Schluessel nicht kennt
    };
    struct LimiterBinding {
        QString key;
        allowedPokemonLimiter* field;
        LimiterEditor* editor;
        allowedPokemonLimiter defaultValue; // Wert, wenn eine Vorlage den Schluessel nicht kennt
    };
    std::vector<BoolBinding> boolBindings;
    std::vector<LimiterBinding> limiterBindings;

    QCheckBox* bindCheck(QBoxLayout* layout, const QString& key, const QString& text, const QString& info,
                         bool* field, std::function<void(bool)> extra = nullptr);
    void bindLimiter(QBoxLayout* layout, const QString& key, allowedPokemonLimiter* field);
    // Karte mit Hauptschalter; body ist nur aktiv, wenn der Schalter an ist
    modernui::Card* sectionCard(QBoxLayout* parentLayout, const QString& title, const QString& key,
                                const QString& switchText, const QString& info, bool* field,
                                QVBoxLayout*& body, std::function<void(bool)> extra = nullptr);

    // --- Wunsch-Starter ---
    struct StarterRow {
        QCheckBox* shiny = nullptr;
        QLineEdit* name = nullptr;
        QComboBox* form = nullptr;
        QComboBox* gender = nullptr;
        QComboBox* ball = nullptr;
        QLabel* hint = nullptr;
    };
    StarterRow starterRows[3];
    QHash<QString, QString> germanToEnglish;   // Kleinbuchstaben-Deutsch -> interner englischer Name
    QHash<QString, QString> englishToGerman;
    void loadPokemonNames();
    void updateStarterForms(int index);
    void updateStarterGenders(int index);
    void refreshStarters();
    QJsonArray startersToJson() const;
    void startersFromJson(const QJsonArray& array);

    // --- Statusleiste ---
    QStringList activeAreas() const;
    void updateStatus();
    void setupGroups();
    void updateTrainerStates();
    void updateSeedInfo();

public:
    // Uebertraegt alle Einstellungen in den Randomizer (auch fuer Tests)
    void applyToRandomizer();
    // Randomisiert (randomize = true) bzw. oeffnet das Original und legt die Bearbeiten-Sitzung an
    bool prepareSession(bool randomize, QString* error);
    // Oeffnet eine schon exportierte Mod (Ordner mit "romfs") als Sitzung
    bool openModSession(const QString& modFolder, QString* error);
    // Schreibt Mod, Spoiler-Log und Info.txt nach outFolder
    bool exportTo(const QString& outFolder, QString* error);
    svedit::EditSession* currentSession() { return session.get(); }
private:
    void startRandomizer();

    QJsonObject settingsToJson() const;
    void settingsFromJson(const QJsonObject& o);
    void refreshAll();
    void saveSettingsDialog();
    void loadSettingsDialog();
    QString autosavePath() const;
    void autosave() const;
    void loadAutosave();
};

#endif // MODERNRANDOMIZERWINDOW_H
