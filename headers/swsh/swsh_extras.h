#ifndef SWSH_EXTRAS_H
#define SWSH_EXTRAS_H

// ---------------------------------------------------------------------------
// Pokemon Schwert/Schild: weitere Bereiche
//  - Tausch-Entwicklungen ohne Tausch (Pokemon-Daten)
//  - Dyna-Raids in der Naturzone (nest_hole_encount.bin in data_table.gfpak)
//  - Items: Gegenstaende auf dem Boden, versteckte Items, Shops, Items der
//    Trainer-Pokemon
// ---------------------------------------------------------------------------

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>
#include <QRandomGenerator>

#include "swsh_files.h"
#include "swsh_encounters.h"

namespace swsh {

struct GameTexts;

// --------------------------------------------------- Pokemon-Daten

struct PokemonDataSettings {
    bool tradeEvolutions = false;   // Tausch-Entwicklungen ohne Tausch
    bool abilities = false;         // Faehigkeiten zufaellig
    bool types = false;             // Typen zufaellig
    bool stats = false;             // Basiswerte mischen (Summe bleibt)
    bool levelMoves = false;        // Level-Attacken zufaellig
    int tmMode = 0;                 // 0 Original, 1 Zufaellig, 2 Alle lernen alle TMs/TPs
    bool keepFamilies = true;       // Entwicklungsreihen bleiben einheitlich (Faehigkeiten, Typen, Werte)
    bool anyEnabled() const { return tradeEvolutions || abilities || types || stats || levelMoves || tmMode != 0; }
};
QJsonObject pokemonDataSettingsToJson(const PokemonDataSettings& s);
void pokemonDataSettingsFromJson(const QJsonObject& json, PokemonDataSettings& s);

// Faehigkeiten, Typen, Basiswerte, Level-Attacken, TM/TP. Veraendert files.personal und files.learnsets.
struct PokemonDataResult {
    bool personalChanged = false;
    bool learnsetsChanged = false;
    int abilities = 0, types = 0, stats = 0, learnsets = 0, tms = 0; // Anzahl geaenderter Eintraege
};
PokemonDataResult randomizePokemonData(GameFiles& files, const PokemonDataSettings& settings, quint64 seed);

// --------------------------------------------------- Tausch-Entwicklungen

struct EvolutionChange {
    int fromSpecies = 0, fromForm = 0;
    int toSpecies = 0, toForm = 0;
    QString before;   // z. B. "Tausch"
    QString after;    // z. B. "ab Level 37"
};

// Ersetzt Tausch-Entwicklungen in files.evolutions. Gibt die Aenderungen zurueck.
QList<EvolutionChange> removeTradeEvolutions(GameFiles& files, const GameTexts& texts, int level = 37);

// ---------------------------------------------------------- Dyna-Raids

struct RaidSettings {
    bool enabled = false;
    bool sameType = false;          // optional
    bool levelAppropriate = true;
    bool similarStrength = false;   // optional
    bool legendaries = false;
    bool keepGigantamax = true;     // Gigadynamax-Raids bleiben Gigadynamax-Raids
};
QJsonObject raidSettingsToJson(const RaidSettings& s);
void raidSettingsFromJson(const QJsonObject& json, RaidSettings& s);

struct RaidResult {
    struct Den {
        int index = 0;      // Nummer der Tabelle innerhalb der Version
        bool shield = false;
        QList<QPair<StarterChoice, int>> entries; // (neues Pokemon, niedrigster Stern), mit Gigadynamax als form + 1000
    };
    QList<Den> dens;
};

// Raid-Tabellen (nest_hole_encount.bin) lesen und schreiben
struct NestEntry {
    qint64 entryIndex = 0, species = 0, form = 0, levelTable = 0, ability = 0, gigantamax = 0, dropTable = 0,
           bonusTable = 0, gender = 0, flawlessIVs = 0;
    QVector<qint64> probabilities;   // Chance je Sternzahl (1-5 Sterne)
};
struct NestTable {
    qint64 tableId = 0, version = 0;  // version: 0 beide, 1 Schwert, 2 Schild
    QList<NestEntry> entries;
};
bool readNestTables(const QByteArray& raw, QList<NestTable>& tables);
QByteArray writeNestTables(const QList<NestTable>& tables);

// dataTable: Inhalt von data_table.gfpak (wird veraendert zurueckgegeben)
bool randomizeRaids(QByteArray& dataTable, const GameFiles& files, const RaidSettings& settings, quint64 seed,
                    RaidResult& result, QString* error);

// --------------------------------------------------------------- Items

struct ItemSettings {
    bool fieldItems = false;   // Gegenstaende auf dem Boden (Pokeball-Symbole)
    bool hiddenItems = false;  // versteckte Items (glitzernde Stellen)
    int mode = 0;              // 0 Mischen (Original-Items neu verteilen), 1 komplett zufaellig
    bool shops = false;
    bool trainerItems = false; // Trainer-Pokemon mit Item bekommen ein zufaelliges Kampf-Item
    bool anyEnabled() const { return fieldItems || hiddenItems || shops || trainerItems; }
};
QJsonObject itemSettingsToJson(const ItemSettings& s);
void itemSettingsFromJson(const QJsonObject& json, ItemSettings& s);

struct ItemResult {
    int fieldItems = 0, hiddenItems = 0, shopItems = 0, trainerItems = 0;
    QHash<int, int> placedItems;   // Item -> Anzahl (Boden + versteckt), fuers Spoiler-Log
    QByteArray placement;          // neue placement.gfpak (leer = unveraendert)
    QByteArray shops;              // neue shop_data.bin (leer = unveraendert)
};

// Item-Daten (Taschen, Preise, Hashes in placement.gfpak)
struct ItemInfo {
    QHash<int, int> pouch;
    QHash<int, quint32> price;
    QHash<quint64, int> idByHash;
    QHash<int, quint64> hashById;
    bool load(const QString& romfs);
    bool isProtected(int id) const;   // TMs/TPs und Basis-Items
};

// Items in placement.gfpak (Boden und versteckt) fuer den Bearbeiten-Modus
struct ItemSpot {
    int file = 0;           // Datei im Archiv
    quint64 zoneId = 0;     // Gebiet (fuer den Ortsnamen)
    bool hidden = false;
    int hashPos = 0;
    int quantityPos = -1;   // -1 = keine Anzahl
    int item = 0;
    int quantity = 1;
};
QList<ItemSpot> readItemSpots(const QByteArray& placement, const ItemInfo& info);
QByteArray writeItemSpots(const QByteArray& placement, const QList<ItemSpot>& spots, const ItemInfo& info);

// Angebote in shop_data.bin (feste Anzahl, werden direkt ersetzt)
struct ShopSlot {
    int shop = 0;
    int stage = -1;          // -1 = Laden ohne Stufen, sonst Stufe (z. B. nach Orden)
    int pos = 0;
    int item = 0;
};
QList<ShopSlot> readShopSlots(const QByteArray& shop);
QByteArray writeShopSlots(const QByteArray& shop, const QList<ShopSlot>& shopSlots);

// placement: aktuelle placement.gfpak (ggf. schon mit neuen Starter-Modellen)
bool randomizeItems(const QString& romfs, const QByteArray& placement, GameFiles& work, QList<int>* changedTrainers,
                    const ItemSettings& settings, quint64 seed, ItemResult& result, QString* error);

// ------------------------------------------- Dynamax-Abenteuer und Kampfturm

struct FacilitySettings {
    bool maxLair = false;           // Leih- und Gegner-Pokemon in der Dyna-Hoehle
    bool maxLairLegends = false;    // Legendaere am Ende der Hoehle (bleiben legendaer)
    bool tower = false;             // Teams im Kampfturm
    bool towerFullyEvolved = true;  // nur voll entwickelte Pokemon
    bool towerLegendaries = false;
    bool anyEnabled() const { return maxLair || maxLairLegends || tower; }
};
QJsonObject facilitySettingsToJson(const FacilitySettings& s);
void facilitySettingsFromJson(const QJsonObject& json, FacilitySettings& s);

// Dateien und Felder (fuer den Bearbeiten-Modus)
extern const QString kLairFile;    // underground_exploration_poke.bin
extern const QString kTowerFile;   // battle_tower_poke_table.bin
// EncounterUnderground
namespace lair {
enum Field { IsSingleCapture, CaptureFlag, Field02, Form, GigantamaxState, Ball, IndexNum, Level, Species, UiMessage,
             OtGender, Version, Shiny, IvSpe, IvAtk, IvDef, IvHp, IvSpa, IvSpd, Ability, StoryGated,
             Move0, Move1, Move2, Move3 };
extern const QVector<int> kSizes;
}
// BattleTowerPoke
namespace tower {
enum Field { F00, F01, F02, F03, F04, F05, F06, Form, F08, HeldItem, Species, EntryId, F0C, Nature, F0E,
             IvHp, IvAtk, IvDef, IvSpa, IvSpd, IvSpe, F15, Move0, Move1, Move2, Move3 };
extern const QVector<int> kSizes;
}

struct FacilityResult {
    QList<StarterChoice> lairPokemon;   // neue Leih-/Gegner-Pokemon (form + 1000 = Gigadynamax)
    QList<StarterChoice> lairLegends;
    QList<StarterChoice> towerPokemon;
    QByteArray lair;                    // underground_exploration_poke.bin (leer = unveraendert)
    QByteArray tower;                   // battle_tower_poke_table.bin
};
bool randomizeFacilities(const QString& romfs, const GameFiles& files, const FacilitySettings& settings, quint64 seed,
                         FacilityResult& result, QString* error);
bool writeFacilities(const QString& outRomfs, const FacilityResult& result);

// Zufaelliges Kampf-Item (Ueberreste, Leben-Orb, Wahlschal ...)
int randomBattleItem(QRandomGenerator& rng);

// ------------------------------------------------------------ Spoiler-Log

QList<SpoilerSection> extrasSpoiler(const QList<EvolutionChange>& evolutions, const RaidResult& raids,
                                    const ItemResult& items, const GameTexts& texts, Version version);
QList<SpoilerSection> dataSpoiler(const PokemonDataResult& data, const FacilityResult& facilities, const GameFiles& files,
                                  const GameTexts& texts);

} // namespace swsh

#endif // SWSH_EXTRAS_H
