#ifndef BDSP_ENCOUNTERS_H
#define BDSP_ENCOUNTERS_H

// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Starter, Geschenke,
// statische Begegnungen und Tausch.
//
//  - Starter: Programmcode (IPS-Patch) + Starter-Liste in global-metadata.dat,
//    Skript-Vergleiche (Rivale, Lucius/Lucia, Labor) und die Teams von Rivale
//    und Lucius/Lucia (deren Starter-Linie wird mitgetauscht)
//  - Geschenke: Evoli (Programmcode), Eier von Wanderer und Urs (Skript)
//  - Statische Begegnungen: Legendaere, Driftlon, Rotom, Kryppuk (Skript)
//  - Tausch: LocalKoukanData (masterdatas)
// ---------------------------------------------------------------------------

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "bdsp_files.h"
#include "bdsp_trainers.h"

namespace bdsp {

struct StarterChoice {
    int species = 0;
    int form = 0;
};

struct EncounterSettings {
    int starterMode = 0;                 // 0 Original, 1 Zufaellig, 2 Wunsch
    int starterTypes = 1;                // 0 beliebig, 1 drei verschiedene, 2 Pflanze/Feuer/Wasser
    bool starterThreeStages = true;      // nur Basis-Pokemon mit zwei Entwicklungen
    bool starterSimilarStrength = false; // Basiswerte-Summe wie die Original-Starter (optional)
    StarterChoice wished[3];             // statt Chelast, Panflam, Plinfa

    bool gifts = false;
    bool statics = false;
    bool trades = false;
    bool similarStrength = false;        // Basiswerte-Summe aehnlich wie im Original (optional)
    bool legendaries = false;            // Legendaere auch dort, wo vorher keine waren

    bool anyEnabled() const { return starterMode != 0 || gifts || statics || trades; }
};

QJsonObject encounterSettingsToJson(const EncounterSettings& s);
void encounterSettingsFromJson(const QJsonObject& json, EncounterSettings& s);

struct EncounterChange {
    int category = 0; // 0 Starter, 1 Geschenk, 2 Legendaer/Begegnung, 4 Tausch
    int index = 0;
    int oldSpecies = 0;
    int newSpecies = 0;
    int level = 0;
    QString note;
    int requiredSpecies = 0; // Tausch: gewuenschtes Pokemon
};

struct EncounterResult {
    QList<EncounterChange> changes;
    BundleData scripts;          // ev_script mit den geaenderten Skripten
    QStringList changedScripts;
    QByteArray ips;              // exefs-Patch (leer = keiner)
    QString ipsName;             // <Build-ID>.ips
    QByteArray metadata;         // geaenderte global-metadata.dat (leer = unveraendert)
    int rivalPokemon = 0;        // ersetzte Starter in den Teams von Rivale und Lucius/Lucia
};

// Pokemon fuer die Wunsch-Starter-Auswahl (Grundformen)
QList<StarterChoice> availablePokemon(const GameFiles& files);

// Veraendert files (Tausch, Teams von Rivale/Lucius/Lucia). exefs wird fuer die Starter und Evoli gebraucht.
bool randomizeEncounters(const QString& romfs, const QString& exefs, GameFiles& files, const EncounterSettings& settings,
                         quint64 seed, EncounterResult& result, QString* error);

// Schreibt ev_script, global-metadata.dat (romfs) und den IPS-Patch (exefs) nach outFolder
bool writeEncounters(const QString& outFolder, const EncounterResult& result, QString* error);

QList<SpoilerSection> encounterSpoiler(const EncounterResult& result, const GameFiles& files, const GameTexts& texts);

} // namespace bdsp

#endif // BDSP_ENCOUNTERS_H
