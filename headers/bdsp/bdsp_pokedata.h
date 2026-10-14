#ifndef BDSP_POKEDATA_H
#define BDSP_POKEDATA_H

// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Pokemon-Daten und Kampfturm
//
//  - Tausch-Entwicklungen ohne Tausch (EvolveTable)
//  - Faehigkeiten, Typen, Basiswerte, TM-Kompatibilitaet (PersonalTable)
//  - Level-Attacken (WazaOboeTable)
//  - Kampfturm: Pokemon der Gegner (TowerTrainerTable in masterdatas)
// ---------------------------------------------------------------------------

#include <QJsonObject>
#include <QList>
#include <QString>

#include "bdsp_files.h"
#include "bdsp_trainers.h"

namespace bdsp {

struct PokemonDataSettings {
    bool tradeEvolutions = false;   // Tausch-Entwicklungen ohne Tausch
    bool abilities = false;         // Faehigkeiten zufaellig
    bool types = false;             // Typen zufaellig
    bool stats = false;             // Basiswerte mischen (Summe bleibt)
    bool levelMoves = false;        // Level-Attacken zufaellig
    int tmMode = 0;                 // 0 Original, 1 Zufaellig, 2 Alle lernen alle TMs
    bool keepFamilies = true;       // Entwicklungsreihen bleiben einheitlich
    bool anyEnabled() const { return tradeEvolutions || abilities || types || stats || levelMoves || tmMode != 0; }
};
QJsonObject pokemonDataSettingsToJson(const PokemonDataSettings& s);
void pokemonDataSettingsFromJson(const QJsonObject& json, PokemonDataSettings& s);

struct EvolutionChange {
    int fromSpecies = 0, fromForm = 0;
    int toSpecies = 0, toForm = 0;
    QString before;   // z. B. "Tausch"
    QString after;    // z. B. "ab Level 37"
};

struct PokemonDataResult {
    int abilities = 0, types = 0, stats = 0, learnsets = 0, tms = 0; // Anzahl geaenderter Eintraege
    QList<EvolutionChange> evolutions;
};

// Veraendert files.personal, files.learnsets und files.evolutions (und setzt die *Changed-Flags)
PokemonDataResult randomizePokemonData(GameFiles& files, const GameTexts& texts, const PokemonDataSettings& settings,
                                       quint64 seed);

// ------------------------------------------------------------- Kampfturm

struct TowerSettings {
    bool enabled = false;
    bool fullyEvolved = true;       // nur voll entwickelte Pokemon (wie im Original)
    bool legendaries = false;
};
QJsonObject towerSettingsToJson(const TowerSettings& s);
void towerSettingsFromJson(const QJsonObject& json, TowerSettings& s);

struct TowerResult {
    QList<int> pokemon;             // neue Arten (eine pro Kampfturm-Pokemon)
    int teams = 0;                  // gepruefte Teams (Einzel + Doppel)
};

// Veraendert files.masterBundle (TowerTrainerTable) und setzt files.towerChanged
bool randomizeTower(GameFiles& files, const TowerSettings& settings, quint64 seed, TowerResult& result, QString* error);

QList<SpoilerSection> pokemonDataSpoiler(const PokemonDataResult& data, const TowerResult& tower, const GameFiles& files,
                                         const GameTexts& texts);

} // namespace bdsp

#endif // BDSP_POKEDATA_H
