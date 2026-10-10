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
