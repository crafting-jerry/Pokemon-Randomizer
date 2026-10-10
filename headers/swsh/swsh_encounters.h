#ifndef SWSH_ENCOUNTERS_H
#define SWSH_ENCOUNTERS_H

// ---------------------------------------------------------------------------
// Pokemon Schwert/Schild: Starter, Geschenke, statische Begegnungen und Tausch.
//
//  - Starter: Original, zufaellig oder Wunsch-Starter. Die Modelle auf dem
//    Tisch bei Delion zeigen die neuen Starter (placement.gfpak).
//  - Geschenke (add_poke.bin), statische Begegnungen (event_encount_data.bin)
//    und Tauschpartner (field_trade.bin)
//  - Story-kritische Pokemon bleiben unveraendert (Zacian, Zamazenta,
//    Endynalos, Dakuma, Coronospa mit den Roessern, Flegmon-Verfolgung)
// ---------------------------------------------------------------------------

#include <QJsonObject>
#include <QList>
#include <QString>

#include "swsh_files.h"

namespace swsh {

struct GameTexts;

struct StarterChoice {
    int species = 0;
    int form = 0;
};

struct EncounterSettings {
    int starterMode = 0;          // 0 Original, 1 Zufaellig, 2 Wunsch
    int starterTypes = 1;         // 0 beliebig, 1 drei verschiedene, 2 Pflanze/Feuer/Wasser
    bool starterThreeStages = true; // nur Basis-Pokemon mit zwei Entwicklungen
    bool starterSimilarStrength = false; // Basiswerte-Summe wie die Original-Starter (optional)
    StarterChoice wished[3];      // statt Chimpep, Hopplo, Memmeon

    bool gifts = false;
    bool statics = false;         // Legendaere und Story-Begegnungen
    bool overworld = false;       // feste Pokemon in der Spielwelt
    bool trades = false;
    bool similarStrength = false; // Basiswerte-Summe aehnlich wie im Original (optional)
    bool legendaries = false;     // Legendaere auch dort, wo vorher keine waren

    bool anyEnabled() const { return starterMode != 0 || gifts || statics || overworld || trades; }
};

QJsonObject encounterSettingsToJson(const EncounterSettings& s);
void encounterSettingsFromJson(const QJsonObject& json, EncounterSettings& s);

struct EncounterChange {
    int category = 0;             // 0 Starter, 1 Geschenk, 2 Story/Legendaer, 3 Spielwelt, 4 Tausch
    int index = 0;
    int oldSpecies = 0, oldForm = 0;
    int newSpecies = 0, newForm = 0;
    int level = 0;
    QString note;
    int requiredSpecies = 0;      // Tausch: gewuenschtes Pokemon
};

struct EncounterResult {
    QList<EncounterChange> changes;
    QByteArray gifts, statics, trades, placement; // leer = Datei unveraendert
};

// Randomisiert alles laut Einstellungen. Liest die Dateien aus romfs.
bool randomizeEncounters(const QString& romfs, const GameFiles& files, const EncounterSettings& settings,
                         quint64 seed, EncounterResult& result, QString* error);
bool writeEncounters(const QString& outRomfs, const EncounterResult& result);

// Alle (Art, Form), die als Starter waehlbar sind (im Spiel vorhanden)
QList<StarterChoice> availablePokemon(const GameFiles& files);

// HTML-Abschnitte fuer das Spoiler-Log
struct SpoilerSection {
    QString title;
    QString html;
    int count = 0;
};
QList<SpoilerSection> encounterSpoiler(const EncounterResult& result, const GameFiles& files, const GameTexts& texts);

} // namespace swsh

#endif // SWSH_ENCOUNTERS_H
