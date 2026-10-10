#ifndef SWSH_FILES_H
#define SWSH_FILES_H

// ---------------------------------------------------------------------------
// Pokemon Schwert/Schild: Lesen und Schreiben der Spieldateien aus dem Dump.
//
// Alle Formate sind mit pkNX als Nachschlagewerk dokumentiert und gegen einen
// echten Dump (Version 1.3.2) geprueft. Daten werden roh gehalten, damit
// unveraenderte Dateien byteidentisch zurueckgeschrieben werden.
// ---------------------------------------------------------------------------

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include "../common/game_data.h"

namespace swsh {

// ------------------------------------------------------------- Dump-Pruefung

enum class Version { Unknown, Sword, Shield };

QString versionName(Version version);
QString titleId(Version version); // Ordnername fuer den Mod (atmosphere/contents/<id>)

struct DumpCheck {
    bool ok = false;               // RomFS vollstaendig und Version 1.3.x
    Version version = Version::Unknown;
    QStringList errors;            // verhindern das Randomisieren
    QStringList warnings;          // Hinweise, Randomisieren trotzdem moeglich
};

// romfs: Ordner, der "bin" enthaelt; exefs: Ordner mit "main.npdm" (optional)
DumpCheck checkDump(const QString& romfs, const QString& exefs);

// Relative Pfade innerhalb von romfs
namespace path {
const QString Personal = "bin/pml/personal/personal_total.bin";
const QString Learnsets = "bin/pml/waza_oboe/wazaoboe_total.bin";
const QString Evolutions = "bin/pml/evolution";
const QString Moves = "bin/pml/waza";
const QString TrainerData = "bin/trainer/trainer_data";
const QString TrainerPoke = "bin/trainer/trainer_poke";
const QString Gifts = "bin/script_event_data/add_poke.bin";
const QString Statics = "bin/script_event_data/event_encount_data.bin";
const QString Trades = "bin/script_event_data/field_trade.bin";
const QString Placement = "bin/archive/field/resident/placement.gfpak";
}

QByteArray readFile(const QString& path, bool* ok = nullptr);

// Spieltexte (bin/message/<Sprache>/common/<name>.dat). Deutsch, sonst Englisch, sonst leer.
// Index = Zeile, z. B. monsname -> Nationaldex, wazaname -> Attacken-ID, trname -> Trainer-Index.
QStringList readMessage(const QString& romfs, const QString& name);
QStringList decodeMessage(const QByteArray& data);
bool writeFile(const QString& path, const QByteArray& data);

// -------------------------------------------------- FlatBuffer-Archive
// Archive der Form { Table:[Eintrag] } mit Eintraegen, die nur Zahlen enthalten
// (Geschenke, statische Begegnungen, Tausch). Felder werden ueber ihren Index
// angesprochen; fehlende Felder sind 0. Beim Schreiben entsteht ein neuer,
// gueltiger FlatBuffer mit allen Feldern.

struct FlatRecord {
    QVector<qint64> values;
    qint64 get(int field) const { return field < values.size() ? values[field] : 0; }
    void set(int field, qint64 value) {
        if (field >= values.size()) values.resize(field + 1);
        values[field] = value;
    }
};

QList<FlatRecord> readFlatArchive(const QByteArray& data, const QVector<int>& fieldSizes, bool* ok = nullptr);
QByteArray writeFlatArchive(const QList<FlatRecord>& records, const QVector<int>& fieldSizes);

// Feldnummern laut Schema (pkNX dient nur als Nachschlagewerk)
namespace gift {
enum Field { IsEgg, Form, DynamaxLevel, Ball, Field04, Hash1, CanGigantamax, HeldItem, Level, Species, Field0A,
             MemoryCode, MemoryData, MemoryFeel, MemoryLevel, OtName, OtGender, ShinyLock, Nature, Gender,
             IvSpe, IvAtk, IvDef, IvHp, IvSpa, IvSpd, Ability, SpecialMove };
extern const QVector<int> kSizes;
}
namespace encounter {
enum Field { BackgroundFar, BackgroundNear, EvSpe, EvAtk, EvDef, EvHp, EvSpa, EvSpd, Form, DynamaxLevel, Field0A,
             EncounterId, Field0C, CanGigantamax, HeldItem, Level, Scenario, Species, ShinyLock, Nature, Gender,
             IvSpe, IvAtk, IvDef, IvHp, IvSpa, IvSpd, Ability, Move0, Move1, Move2, Move3 };
extern const QVector<int> kSizes;
}
namespace trade {
enum Field { Form, DynamaxLevel, Ball, Field03, Hash0, CanGigantamax, HeldItem, Level, Species, Hash1, TrainerId,
             Memory, TextVar, Feeling, Intensity, Hash2, OtGender, RequiredForm, RequiredSpecies, RequiredNature,
             UnknownRequirement, ShinyLock, Nature, Gender, IvSpe, IvAtk, IvDef, IvHp, IvSpa, IvSpd, AbilityNumber,
             Relearn1, Relearn2, Relearn3, Relearn4 };
extern const QVector<int> kSizes;
}

// ---------------------------------------------------------------- GFPAK
// Archiv-Format der Feld-Daten (z. B. placement.gfpak). Dateien sind mit LZ4
// komprimiert; geaenderte Dateien werden als gueltiger LZ4-Block ohne
// Kompression gespeichert, alle anderen bleiben unveraendert.

class GfPak {
public:
    bool load(const QByteArray& data);
    int indexOf(const QString& fileName) const; // -1 = nicht gefunden
    int count() const { return entries.size(); }
    QByteArray file(int index) const;
    void setFile(int index, const QByteArray& data);
    QByteArray save() const;

private:
    struct Entry {
        quint16 level = 9;
        quint8 type = 0;
        QByteArray stored;   // so wie im Archiv (komprimiert)
        int rawSize = 0;
    };
    QByteArray header;       // alles vor der Dateitabelle
    QList<Entry> entries;
    QList<QPair<quint64,int>> nameHashes; // (FNV-1a des Dateinamens, Index)
};

QByteArray lz4Decompress(const QByteArray& src, int rawSize);
QByteArray lz4StoreUncompressed(const QByteArray& raw);

// ----------------------------------------------------------- Pokemon-Daten

constexpr int kMaxSpecies = 898;     // Schwert/Schild kennt Pokemon bis Nr. 898
constexpr int kPersonalSize = 0xB0;
constexpr int kPersonalCount132 = 1192; // Eintraege ab Version 1.3.0

class PersonalTable {
public:
    bool load(const QByteArray& data);
    QByteArray save() const { return raw; }
    int count() const { return raw.size() / kPersonalSize; }

    // Index im Table fuer (Art, Form); -1 wenn es die Form nicht gibt
    int indexOf(int species, int form) const;
    // Umkehrung: (Art, Form) eines Index
    QPair<int,int> speciesForm(int index) const { return reverse.value(index, qMakePair(0, 0)); }

    int stat(int index, int which) const;  // 0 KP, 1 Ang, 2 Vert, 3 Init, 4 SpAng, 5 SpVert
    int type1(int index) const { return byteAt(index, 0x06); }
    int type2(int index) const { return byteAt(index, 0x07); }
    int evoStage(int index) const { return byteAt(index, 0x09); }
    int formCount(int index) const { return byteAt(index, 0x20); }
    bool isPresent(int index) const { return (byteAt(index, 0x21) >> 6) & 1; }
    bool canDynamax(int index) const { return ((byteAt(index, 0x5A) >> 2) & 1) == 0; }
    int ability(int index, int slot) const; // 0, 1, 2 = versteckt
    bool canLearnTM(int index, int tm) const;  // 0..99
    bool canLearnTR(int index, int tr) const;  // 0..99

private:
    int byteAt(int index, int offset) const;
    int u16At(int index, int offset) const;
    void buildIndex();

    QByteArray raw;
    QHash<int, QPair<int,int>> reverse;
};

// Attacken-IDs der TMs und TRs (Reihenfolge wie die Bits in den Pokemon-Daten)
extern const QList<int> kTMMoves;
extern const QList<int> kTRMoves;
// Arten mit Gigadynamax-Form
extern const QList<int> kGigantamaxSpecies;

// -------------------------------------------------------------- Lernsets

constexpr int kLearnsetSize = 0x104;

class LearnsetTable {
public:
    bool load(const QByteArray& data);
    QByteArray save() const { return raw; }
    int count() const { return raw.size() / kLearnsetSize; }
    // (Attacke, Level); Level 0 = beim Entwickeln
    QList<QPair<int,int>> get(int index) const;
    void set(int index, const QList<QPair<int,int>>& moves);

private:
    QByteArray raw;
};

// --------------------------------------------------------- Entwicklungen

struct Evolution {
    int method = 0;
    int argument = 0;
    int species = 0;
    int form = 0;
    int level = 0;
};

class EvolutionTable {
public:
    // Liest evo_000.bin, evo_001.bin, ... aus dem Ordner
    bool load(const QString& folder, int count);
    QList<Evolution> get(int index) const;
    QByteArray rawFile(int index) const { return files.value(index); }
    // Ersetzt die Entwicklungen eines Eintrags (max. so viele, wie die Datei Platz hat)
    bool set(int index, const QList<Evolution>& evolutions);
    // Schreibt nur geaenderte Dateien
    bool save(const QString& outRomfs) const;
    const QList<int>& changedIndexes() const { return changed; }
    static QString fileName(int index);

private:
    QVector<QByteArray> files;
    QList<int> changed;
};

// --------------------------------------------------------------- Attacken

struct Move {
    int id = 0;
    bool canUse = false;
    int type = 0;
    int category = 0;
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
    bool load(const QString& folder);
    const QList<Move>& all() const { return moves; }
    Move get(int id) const;

private:
    QList<Move> moves;
};

// ----------------------------------------------------------------- Trainer

constexpr int kTrainerDataSize = 0x14;
constexpr int kTrainerPokeSize = 0x20;

struct TrainerPoke {
    QByteArray raw = QByteArray(kTrainerPokeSize, 0);

    int level() const;
    void setLevel(int value);
    int species() const;
    void setSpecies(int value);
    int form() const;
    void setForm(int value);
    int heldItem() const;
    void setHeldItem(int value);
    int move(int slot) const;               // 0..3
    void setMove(int slot, int value);
    int dynamaxLevel() const { return static_cast<quint8>(raw[0x08]); }
    void setDynamaxLevel(int value) { raw[0x08] = static_cast<char>(qBound(0, value, 10)); }
    bool canGigantamax() const { return raw[0x09] != 0; }
    void setCanGigantamax(bool value) { raw[0x09] = value ? 1 : 0; }
    int gender() const { return static_cast<quint8>(raw[0x00]) & 0x3; }
    void setGender(int value);
    int ability() const { return (static_cast<quint8>(raw[0x00]) >> 4) & 0x3; }
    void setAbility(int value);
    bool shiny() const;
    void setShiny(bool value);
    bool dynamaxAllowed() const;            // Bit 31 der DVs: darf im Stadion dynamaximieren
    void setDynamaxAllowed(bool value);
    void setPerfectIVs();
};

struct Trainer {
    int index = 0;
    QByteArray data = QByteArray(kTrainerDataSize, 0);
    QList<TrainerPoke> team;
    int loadedTeamSize = 0; // Eintraege in trainer_poke beim Laden (Platzhalter-Trainer haben 1 Eintrag, aber Teamgroesse 0)

    int trainerClass() const;
    int battleMode() const { return static_cast<quint8>(data[2]); } // 0 Einzel, 1 Doppel
    int teamSize() const { return static_cast<quint8>(data[3]); }
    quint32 ai() const;                      // KI-Flags (1 Basis, 2 Stark, 4 Experte, 8 Doppel, 0x20 Items, 0x40 Wechseln)
    void setAi(quint32 value);
    bool isPlaceholder() const { return teamSize() == 0; } // nicht im Spiel verwendet
    QByteArray pokeFile() const;
};

class TrainerTable {
public:
    bool load(const QString& romfs);
    QList<Trainer>& all() { return trainers; }
    const QList<Trainer>& all() const { return trainers; }
    // Schreibt trainer_data_XXX.bin und trainer_poke_XXX.bin fuer die angegebenen Trainer
    bool save(const QString& outRomfs, const QList<int>& indexes) const;

    static QString dataFile(int index);
    static QString pokeFile(int index);

private:
    QList<Trainer> trainers;
};

// ------------------------------------------------------------ Gesamtpaket

struct GameFiles {
    PersonalTable personal;
    LearnsetTable learnsets;
    EvolutionTable evolutions;
    MoveTable moves;
    TrainerTable trainers;

    // Laedt alles aus dem RomFS; Fehlermeldungen landen in errors
    bool load(const QString& romfs, QStringList* errors);

    // Spielneutrales Modell fuer die Team-Logik
    GameData toGameData() const;
};

} // namespace swsh

#endif // SWSH_FILES_H
