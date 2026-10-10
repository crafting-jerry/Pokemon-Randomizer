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

#include "swsh_files.h"
#include "swsh_encounters.h"

namespace swsh {

struct GameTexts;

// --------------------------------------------------- Pokemon-Daten

struct PokemonDataSettings {
    bool tradeEvolutions = false;   // Tausch-Entwicklungen ohne Tausch
    bool anyEnabled() const { return tradeEvolutions; }
};
QJsonObject pokemonDataSettingsToJson(const PokemonDataSettings& s);
void pokemonDataSettingsFromJson(const QJsonObject& json, PokemonDataSettings& s);

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

// ------------------------------------------------------------ Spoiler-Log

QList<SpoilerSection> extrasSpoiler(const QList<EvolutionChange>& evolutions, const RaidResult& raids,
                                    const ItemResult& items, const GameTexts& texts, Version version);

} // namespace swsh

#endif // SWSH_EXTRAS_H
