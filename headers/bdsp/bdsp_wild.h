#ifndef BDSP_WILD_H
#define BDSP_WILD_H

// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: wilde Pokemon
//
//  - FieldEncountTable_d / _p (gamesettings): Gras, Schwaerme, Tag/Nacht,
//    Poke-Radar, GBA-Slots, Surfen, Angeln; dazu Honigbaeume, Grosses Moor
//    und Trophaeengarten
//  - Untergrund (ugdata): Hoehlen-Tabellen und seltene Pokemon
// Beide Editionen werden geaendert; das Spoiler-Log zeigt die eigene.
// ---------------------------------------------------------------------------

#include <QJsonObject>
#include <QList>
#include <QString>

#include "bdsp_files.h"
#include "bdsp_trainers.h"

namespace bdsp {

struct WildSettings {
    bool enabled = false;
    int mode = 0;                 // 0 pro Gebiet, 1 global 1:1, 2 jeder Platz zufaellig
    bool levelAppropriate = true; // keine Entwicklungen vor ihrem Level
    bool sameType = false;        // neues Pokemon teilt einen Typ mit dem alten
    bool similarStrength = false; // Basiswerte-Summe aehnlich (optional)
    bool legendaries = false;
    bool underground = true;      // Untergrund (Hoehlen) mit randomisieren
};

QJsonObject wildSettingsToJson(const WildSettings& s);
void wildSettingsFromJson(const QJsonObject& json, WildSettings& s);

struct WildResult {
    BundleData gameSettings;      // gamesettings mit FieldEncountTable_d/_p
    BundleData underground;       // ugdata
    bool fieldChanged = false;
    QStringList undergroundTables;
    struct AreaLog {
        QString place;
        int levelMin = 0, levelMax = 0;
        QList<QPair<int, int>> pairs; // alt -> neu
    };
    QList<AreaLog> areas;        // eigene Edition
    QList<AreaLog> undergroundLog;
};

bool randomizeWild(const QString& romfs, const GameFiles& files, const WildSettings& settings, Version version,
                   quint64 seed, WildResult& result, QString* error);
bool writeWild(const QString& outRomfs, const WildResult& result, QString* error);

QList<SpoilerSection> wildSpoiler(const WildResult& result, const GameFiles& files, const GameTexts& texts);

} // namespace bdsp

#endif // BDSP_WILD_H
