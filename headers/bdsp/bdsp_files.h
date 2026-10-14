#ifndef BDSP_FILES_H
#define BDSP_FILES_H

// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Spieldaten aus dem Dump.
//
// Alle Tabellen werden ueber die Type-Trees der Unity-Bundles gelesen (siehe
// bdsp_unity.h). Geaenderte Tabellen werden in eine Kopie des Original-Bundles
// zurueckgeschrieben; alles andere bleibt byte-genau erhalten.
// Gegen einen echten Dump (Strahlender Diamant, Update 1.3.0) geprueft.
// ---------------------------------------------------------------------------

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QSet>
#include <functional>

#include "bdsp_unity.h"
#include "../common/game_data.h"

namespace bdsp {

// ------------------------------------------------------------- Dump-Pruefung

enum class Version { Unknown, Diamond, Pearl };

QString versionName(Version version);
QString titleId(Version version); // Ordnername fuer den Mod

struct DumpCheck {
    bool ok = false;
    Version version = Version::Unknown;
    QStringList errors;   // verhindern das Randomisieren
    QStringList warnings; // Hinweise
};

// romfs: Ordner, der "Data" enthaelt; exefs: Ordner mit "main.npdm"
DumpCheck checkDump(const QString& romfs, const QString& exefs);

// Relative Pfade innerhalb von romfs
namespace path {
const QString Assets = "Data/StreamingAssets/AssetAssistant/";
const QString Personal = Assets + "Pml/personal_masterdatas";
const QString Master = Assets + "Dpr/masterdatas";
const QString GameSettings = Assets + "Dpr/scriptableobjects/gamesettings";
const QString EvScript = Assets + "Dpr/ev_script";
const QString MessageCommon = Assets + "Message/common_msbt";
const QString MessageGerman = Assets + "Message/german";
const QString MessageEnglish = Assets + "Message/english";
}

QByteArray readFile(const QString& path, bool* ok = nullptr);
bool writeFile(const QString& path, const QByteArray& data);

// Ein geladenes Bundle mit seinen Tabellen (Objekt-Index -> Baum)
class BundleData {
public:
    bool load(const QString& romfs, const QString& relPath, const QStringList& tables, QString* error);
    // Laedt alle MonoBehaviour-Objekte, fuer die keep() true liefert (z. B. alle Skripte)
    bool loadWhere(const QString& romfs, const QString& relPath, const std::function<bool(const UValue&)>& keep,
                   QString* error);
    QStringList tableNames() const { QStringList n = values.keys(); n.sort(); return n; } // feste Reihenfolge
    UValue& table(const QString& name) { return values[name]; }
    const UValue& table(const QString& name) const;
    bool has(const QString& name) const { return values.contains(name); }
    // Alle Objekte, deren Name mit prefix beginnt (nur lesen)
    QList<QPair<QString, UValue>> readPrefixed(const QString& prefix) const;
    // Schreibt das Bundle mit den angegebenen (geaenderten) Tabellen nach outRomfs
    bool save(const QString& outRomfs, const QStringList& changedTables, QString* error) const;
    const QString& relativePath() const { return rel; }

private:
    QString rel;
    QByteArray raw;
    std::shared_ptr<UnityBundle> bundle; // haelt die Type-Trees der Werte am Leben
    QHash<QString, int> objectIndex;
    QHash<QString, UValue> values;
};

// Alle Skripte (ev_script): werden einmal geladen, von mehreren Bereichen geaendert und einmal gespeichert
struct ScriptData {
    BundleData bundle;
    QSet<QString> changed;
    bool loaded = false;
    bool ensureLoaded(const QString& romfs, QString* error);
    bool save(const QString& outRomfs, QString* error) const;
};

// ----------------------------------------------------------- Pokemon-Daten

constexpr int kMaxSpecies = 493;

class PersonalTable {
public:
    void attach(UValue* root);
    int count() const;
    int indexOf(int species, int form) const;   // -1 = gibt es nicht
    QPair<int,int> speciesForm(int index) const { return reverse.value(index, qMakePair(0, 0)); }

    int get(int index, const char* field) const;
    void set(int index, const char* field, int value);

    int stat(int index, int which) const;       // 0 KP, 1 Ang, 2 Vert, 3 Init, 4 SpAng, 5 SpVert
    void setStat(int index, int which, int value);
    int type1(int index) const { return get(index, "type1"); }
    int type2(int index) const { return get(index, "type2"); }
    int ability(int index, int slot) const;     // 0, 1, 2 = versteckt
    void setAbility(int index, int slot, int value);
    int formCount(int index) const { return qMax(1, get(index, "form_max")); }
    bool isPresent(int index) const { return index > 0 && get(index, "valid_flag") != 0; }
    bool canLearnTM(int index, int machineNo) const; // 1..100
    void setTM(int index, int machineNo, bool value);
    int genderRatio(int index) const { return get(index, "sex"); } // 0 nur maennlich, 254 nur weiblich, 255 geschlechtslos

private:
    void buildIndex();
    UValue* root = nullptr;
    QHash<qint64, int> forward;
    QHash<int, QPair<int,int>> reverse;
};

struct Evolution {
    int method = 0;
    int argument = 0;
    int species = 0;
    int form = 0;
    int level = 0;
};

class EvolutionTable {
public:
    void attach(UValue* root) { this->root = root; }
    int count() const;
    QList<Evolution> get(int index) const;
    void set(int index, const QList<Evolution>& evolutions);

private:
    UValue* root = nullptr;
};

class LearnsetTable {
public:
    void attach(UValue* root) { this->root = root; }
    int count() const;
    // (Attacke, Level) wie bei Schwert/Schild
    QList<QPair<int,int>> get(int index) const;
    void set(int index, const QList<QPair<int,int>>& moves);

private:
    UValue* root = nullptr;
};

struct Move {
    int id = 0;
    bool canUse = false;
    int type = 0;
    int category = 0; // 0 Status, 1 Physisch, 2 Speziell
    int power = 0;
    int accuracy = 0;
    int priority = 0;
    int hitMin = 0;
    int hitMax = 0;
    bool charge = false;
    bool recharge = false;
};

class MoveTable {
public:
    void load(const UValue& root);
    const QList<Move>& all() const { return moves; }
    Move get(int id) const;

private:
    QList<Move> moves;
};

// ----------------------------------------------------------------- Trainer

struct TrainerMon {
    int species = 0;
    int form = 0;
    bool shiny = false;
    int level = 1;
    int sex = 3;      // 3 = zufaellig/Standard
    int nature = 0;
    int ability = 0;  // Faehigkeits-ID
    int moves[4] = {0, 0, 0, 0};
    int item = 0;
    int ball = 0;
    int seal = -1;
    int ivs[6] = {0, 0, 0, 0, 0, 0};
    int evs[6] = {0, 0, 0, 0, 0, 0};
    bool locked = false; // vom Trainer-Randomizer nicht aendern (z. B. Starter des Rivalen)
};

struct Trainer {
    int index = 0;
    int typeId = -1;     // Trainerklasse (Index in TrainerType)
    int fightType = 0;   // 0 Einzel, 1 Doppel
    int aiBit = 0;
    QString nameLabel;
    QString classLabel;
    QList<TrainerMon> team;
    bool changed = false;
};

class TrainerTable {
public:
    bool load(const UValue& root);
    void store(UValue& root) const; // geaenderte Trainer zurueckschreiben
    QList<Trainer>& all() { return trainers; }
    const QList<Trainer>& all() const { return trainers; }
    bool anyChanged() const;

private:
    QList<Trainer> trainers;
};

// ------------------------------------------------------------ Gesamtpaket

class GameFiles {
public:
    GameFiles() = default;
    GameFiles(const GameFiles& other) { *this = other; }
    GameFiles& operator=(const GameFiles& other);

    BundleData personalBundle;
    BundleData masterBundle;

    PersonalTable personal;
    EvolutionTable evolutions;
    LearnsetTable learnsets;
    MoveTable moves;
    TrainerTable trainers;
    QHash<int, int> tmMoves;                 // TM-Nummer -> Attacke
    QHash<int, QStringList> trainerZones;    // Trainer -> Kartennamen (aus PlaceData)

    bool load(const QString& romfs, QStringList* errors);

    // Aenderungen markieren (welche Tabellen geschrieben werden)
    bool personalChanged = false;
    bool learnsetsChanged = false;
    bool evolutionsChanged = false;
    bool tradesChanged = false; // LocalKoukanData (Tausch) in masterdatas
    bool shopsChanged = false;  // ShopTable (Laeden) in masterdatas

    // Schreibt alle geaenderten Bundles nach outRomfs
    bool save(const QString& outRomfs, QString* error);

    GameData toGameData() const;

private:
    void attachTables();
};

// ------------------------------------------------------------- Spieltexte

struct GameTexts {
    QStringList pokemon, moves, items, types, abilities;
    QHash<QString, QString> trainerNames;   // Label -> Name
    QHash<QString, QString> trainerClasses; // Label -> Klasse
    bool load(const QString& romfs);
    QString pokemonName(int species, int form) const;
    QString moveName(int id) const;
    QString itemName(int id) const;
    QString typeName(int id) const;
    QString trainerLabel(const Trainer& trainer) const; // z. B. "Arenaleiter Veit"
};

// Liest eine Text-Tabelle (MsgDataFile) aus einem Message-Bundle: Index -> Text
QStringList messageLines(const UValue& msg);
QHash<QString, QString> messageLabels(const UValue& msg);

// Ortsnamen je Zone (MapInfo.ZoneData[zone].PokePlaceName -> Spieltext).
// mapInfo: bereits geladene MapInfo-Tabelle aus gamesettings; nullptr = selbst laden
QHash<int, QString> zoneNames(const QString& romfs, const UValue* mapInfo = nullptr);

} // namespace bdsp

#endif // BDSP_FILES_H
