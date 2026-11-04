#ifndef SWSH_TRAINERS_H
#define SWSH_TRAINERS_H

// ---------------------------------------------------------------------------
// Pokemon Schwert/Schild: Trainer randomisieren.
//
//  - Typ-Trainer (Arenen, Arena-Challenger, Sophora/Saverio ...) behalten ihren Typ
//  - Vollentwickelte Pokemon ab einstellbarem Level, level-passende Entwicklungen
//  - Starke Movesets (gemeinsame Logik mit Karmesin/Purpur)
//  - Keine doppelten Arten in einem Team
//  - Gigadynamax: Asse, die gigadynamaximieren, bekommen wieder ein Pokemon mit Gigadynamax-Form
// ---------------------------------------------------------------------------

#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include "swsh_files.h"
#include "swsh_encounters.h"

namespace swsh {

// Einstellungen fuer eine Trainer-Gruppe
struct TrainerOptions {
    int teamSize = 0;             // 0 Original, 1 + Zufaellig, 2 Immer 6
    int fullyEvolvedLevel = 0;    // ab diesem Level nur Endstufen (0 = aus)
    bool levelAppropriateEvos = true;
    bool smartMovesets = true;
    bool smartTMs = true;
    bool keepGigantamax = true;   // Gigadynamax-Asse bleiben Gigadynamax-faehig
    double legendaryChance = 0.0; // Chance in Prozent je Pokemon: Legendaer, Mysterioes oder Ultrabestie
    bool smartAI = false;
    bool perfectIVs = false;
    double shinyChance = 0.0;     // Chance in Prozent je Pokemon: schillernd
};

// Alte Vorlagen hatten Schalter statt Prozentwerten: "an" wird zu diesen Werten
constexpr double kOldLegendaryChance = 10.0;
constexpr double kOldShinyChance = 50.0;
// Liest einen Prozentwert; fehlt er, wird der alte Schalter (oldKey) umgerechnet
double chanceFromJson(const QJsonObject& j, const QString& key, const QString& oldKey, double oldOnValue,
                      double fallback);

QJsonObject optionsToJson(const TrainerOptions& o);
TrainerOptions optionsFromJson(const QJsonObject& json, const TrainerOptions& fallback);

// Trainer-Gruppen (Reihenfolge = Anzeige)
enum TrainerGroup {
    GroupRivals = 0,
    GroupGymLeaders,
    GroupGymTrainers,
    GroupImportant,
    GroupVillains,
    GroupRoutes,
    GroupIsle,
    GroupTundra,
    GroupStarTournament,
    GroupCount
};

struct GroupInfo {
    QString id;      // Schluessel zum Speichern
    QString title;
    QString info;
};
GroupInfo groupInfo(int group);

enum GroupMode { SameAsBase = 0, OwnSettings = 1, KeepOriginal = 2 };

struct TrainerSettings {
    bool enabled = false;
    bool keepTypeTheme = true;
    TrainerOptions base;
    int modes[GroupCount] = {};
    TrainerOptions own[GroupCount];

    const TrainerOptions* optionsFor(int group) const; // nullptr = Gruppe bleibt original
};

QJsonObject settingsToJson(const TrainerSettings& s);
void settingsFromJson(const QJsonObject& json, TrainerSettings& s);

// Regeln fuer zufaellige Pokemon (gelten fuer Trainer, Geschenke, Begegnungen)
bool isLegendary(int species);               // Legendaere, Mysterioese, Ultrabestien
bool isExcludedForm(int species, int form);  // nur im Kampf oder nur mit Item
bool canGigantamax(int species, int form);

// Einordnung eines Trainers
int trainerGroup(const Trainer& trainer);
int trainerTheme(const Trainer& trainer); // Typ-ID oder -1

// Namen aus den Spieltexten fuer Log und Oberflaeche
struct GameTexts {
    QStringList pokemon, moves, items, types, trainerNames, trainerClasses, abilities;
    void load(const QString& romfs);
    QString pokemonName(int species, int form) const;
    QString moveName(int id) const;
    QString itemName(int id) const;
    QString typeName(int id) const;
    QString trainerLabel(const Trainer& trainer) const; // z. B. "Arenaleiter Yarro"
};

struct TrainerRandomizerResult {
    int randomized = 0;
    QList<int> changedIndexes;
};

// Randomisiert die Trainer in files.trainers (in place)
TrainerRandomizerResult randomizeTrainers(GameFiles& files, const TrainerSettings& settings, quint64 seed);

// HTML-Spoiler-Log (editedTrainers: von Hand geaenderte Trainer, erscheinen immer): zuerst die zusaetzlichen Abschnitte (Starter, Geschenke ...), dann alle Trainer-Teams
bool writeSpoiler(const QString& path, const GameFiles& files, const GameTexts& texts,
                  const TrainerSettings& settings, const QList<SpoilerSection>& extra,
                  const QString& seedText, Version version, const QSet<int>& editedTrainers = QSet<int>(),
                  const QString& editedNote = QString("von Hand bearbeitet"));

} // namespace swsh

#endif // SWSH_TRAINERS_H
