#ifndef BDSP_SESSION_H
#define BDSP_SESSION_H

// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant/Leuchtende Perle: Bearbeiten-Modus.
//
// Eine EditSession haelt alle Spieldaten nach dem Randomisieren (oder das
// Original bzw. eine schon exportierte Mod) im Speicher. Der Nutzer kann sie im
// Bearbeiten-Modus aendern, erst exportSession() schreibt die Mod-Dateien und
// das Spoiler-Log. Geschrieben wird alles, was vom Dump abweicht.
// ---------------------------------------------------------------------------

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include "../common/edit_model.h"
#include "../common/trainer_smart.h"
#include "bdsp_files.h"
#include "bdsp_trainers.h"
#include "bdsp_encounters.h"
#include "bdsp_items.h"
#include "bdsp_pokedata.h"
#include "bdsp_wild.h"

namespace bdsp {

// Stelle eines Arguments in den Skripten (ev_script)
struct ScriptPos {
    QString zone;     // Skript-Objekt (Karte)
    int script = 0;   // Index in Scripts
    int command = 0;  // Index in Commands
    int arg = 0;      // Index in Arg
};

struct EditSession {
    bool randomized = false;   // false = Original oder geoeffnete Mod
    bool reopened = false;     // schon exportierte Mod wieder geoeffnet
    QString romfs, exefs;
    Version version = Version::Unknown;
    GameTexts texts;
    QHash<int, QString> zoneNames;
    TrainerSmartData smart;    // Standard-Attacken (Starter-Linie bei Rivale/Lucius)

    GameFiles original;        // Dump
    GameFiles work;            // wird bearbeitet
    GameFiles start;           // Stand beim Oeffnen (erkennt Aenderungen von Hand)

    ScriptData scripts, scriptsOriginal, scriptsStart;
    BundleData field, fieldOriginal, fieldStart;       // gamesettings: FieldEncountTable_d/_p
    BundleData ug, ugOriginal, ugStart;                // Untergrund (ugdata)

    // Starter (Koffer-Plaetze 0-2) und Evoli von Bebe (Programmcode)
    int starters[3] = {387, 390, 393};
    int startersStart[3] = {387, 390, 393};
    int eevee = 133, eeveeStart = 133;
    QList<ScriptPos> starterPos[3];

    // Eier (Skript-Befehl _ADD_TAMAGO)
    QList<ScriptPos> eggPos;

    // Statische Begegnungen: je Original-Art alle Stellen in den Skripten
    struct Static {
        int species = 0;
        int level = 0;
        QStringList places;
        QList<ScriptPos> battle;     // Kampf-Befehle
        QList<ScriptPos> positions;  // alle Stellen (Kampf, Ruf, Name, Pruefungen)
    };
    QList<Static> statics;

    // Items (Skripte fld_item/hide_item)
    struct ItemSpot {
        ScriptPos item;
        ScriptPos count;             // zone leer = keine Anzahl
        bool hidden = false;
        QString label;
    };
    QList<ItemSpot> itemSpots;

    // Kampfturm: welche Zeilen zusammen in einem Team stehen
    QVector<QSet<int>> towerNeighbours;

    // Ergebnisse des Randomizers (fuer Info und Spoiler-Log)
    TrainerRandomizerResult trainerResult;
    EncounterResult encounters;
    WildResult wildResult;
    bool wildRandomized = false, undergroundRandomized = false;
    ItemResult items;
    PokemonDataResult dataResult;
    TowerResult tower;
};

// Liest alle bearbeitbaren Tabellen. Vorher setzen: romfs, exefs, version, original, work,
// scripts/field/ug (falls randomisiert, sonst leer) und die Ergebnisse.
bool openSession(EditSession& session, QString* error);

// Schon exportierte Mod (Ordner mit romfs, ggf. exefs) als Arbeitsstand laden
bool loadModInto(EditSession& session, const QString& modFolder, QString* error);

QList<edit::Section> buildSections(EditSession& session);
edit::Catalog buildCatalog(const EditSession& session);

bool sessionHasChanges(EditSession& session);
int changedTrainerCount(const EditSession& session);

// Schreibt romfs (+ exefs) nach outFolder und das Spoiler-Log (wenn spoilerPath nicht leer)
bool exportSession(EditSession& session, const QString& outFolder, const QString& spoilerPath,
                   const TrainerSettings& trainerSettings, const QString& seedText, bool* hasExefs, QString* error);

} // namespace bdsp

#endif // BDSP_SESSION_H
