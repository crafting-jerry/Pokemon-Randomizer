#ifndef SWSH_SESSION_H
#define SWSH_SESSION_H

// ---------------------------------------------------------------------------
// Pokemon Schwert/Schild: Bearbeiten-Modus.
//
// Eine EditSession haelt alle Spieldaten nach dem Randomisieren (oder das
// Original) im Speicher. Der Nutzer kann sie im Bearbeiten-Modus aendern, erst
// exportSession() schreibt die Mod-Dateien und das Spoiler-Log.
// ---------------------------------------------------------------------------

#include <QByteArray>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include "../common/edit_model.h"
#include "swsh_files.h"
#include "swsh_trainers.h"
#include "swsh_encounters.h"
#include "swsh_wild.h"
#include "swsh_extras.h"

namespace swsh {

struct EditSession {
    bool randomized = false;      // false = "Original bearbeiten" oder geoeffnete Mod
    bool reopened = false;        // schon exportierte Mod wieder geoeffnet
    QString romfs;
    Version version = Version::Unknown;
    GameTexts texts;
    QStringList placeNames;

    GameFiles original;           // Dump, unveraendert
    GameFiles work;               // Stand nach dem Randomisieren, wird bearbeitet

    // Wilde Pokemon: encount_k, encount_symbol_k, encount_t, encount_symbol_t
    WildArchive wild[4];
    WildArchive wildStart[4];     // Stand nach dem Randomisieren (erkennt Aenderungen von Hand)
    WildArchive wildOriginal[4];  // Dump
    QByteArray dataTable;         // data_table.gfpak nach dem Randomisieren (wilde Pokemon + Raids)
    bool dataTableChanged = false;

    // Geschenke (inkl. Starter), statische Begegnungen, Tausch
    QList<FlatRecord> gifts, statics, trades;
    QList<FlatRecord> giftsOriginal, staticsOriginal, tradesOriginal;
    QByteArray placementBase;     // placement.gfpak mit randomisierten Items, ohne Starter-Modelle (leer = Original)

    // Dyna-Raids (nest_hole_encount.bin in data_table.gfpak)
    QList<NestTable> nests, nestsStart, nestsOriginal;

    // Items: Boden/versteckt (placement.gfpak) und Laeden (shop_data.bin)
    ItemInfo itemInfo;
    bool itemInfoLoaded = false;
    QList<ItemSpot> spots, spotsStart, spotsOriginal;
    QList<ShopSlot> shopSlots, shopSlotsStart, shopSlotsOriginal;

    // Pokemon-Daten: Entwicklungen nach dem Randomisieren (erkennt Aenderungen von Hand)
    EvolutionTable evolutionsStart;

    // Dyna-Hoehle und Kampfturm
    QList<FlatRecord> lair, lairStart, lairOriginal, tower, towerStart, towerOriginal;

    // Ergebnisse des Randomizers (fuer Info und Spoiler-Log)
    TrainerRandomizerResult trainerResult;
    QList<EvolutionChange> evolutionChanges;
    PokemonDataResult dataResult;
    RaidResult raids;
    FacilityResult facilities;
    ItemResult items;
};

// Liest alle bearbeitbaren Tabellen. Fuer "Original bearbeiten" ist work == original und
// dataTable/placementBase sind leer. Nach dem Randomisieren muessen work, dataTable,
// placementBase und die Ergebnisse vorher gesetzt sein; gifts/statics/trades werden aus
// den Randomizer-Dateien gelesen, sofern vorhanden.
bool openSession(EditSession& session, const EncounterResult* encounters, QString* error);

// Schon exportierte Mod (Ordner mit romfs) als Arbeitsstand laden und die Sitzung oeffnen.
// Vorher setzen: romfs (Dump), version, original und work (= Dump).
bool loadModInto(EditSession& session, const QString& modFolder, QString* error);

// Bereiche fuer den Bearbeiten-Modus
QList<edit::Section> buildSections(EditSession& session);
edit::Catalog buildCatalog(const EditSession& session);

// Wurde irgendetwas gegenueber dem Dump geaendert?
bool sessionHasChanges(const EditSession& session);

// Schreibt romfs (+ Spoiler-Log, wenn spoilerPath nicht leer). Info.txt schreibt das Fenster.
bool exportSession(const EditSession& session, const QString& romfsOut, const QString& spoilerPath,
                   const TrainerSettings& trainerSettings, const QString& seedText, QString* error);

// Anzahl geaenderter Trainer bzw. Starter/Geschenke/Begegnungen/Tausch (fuer Info.txt)
int changedTrainerCount(const EditSession& session);
int changedEncounterCount(const EditSession& session);

} // namespace swsh

#endif // SWSH_SESSION_H
