#ifndef SWSH_WILD_H
#define SWSH_WILD_H

// ---------------------------------------------------------------------------
// Pokemon Schwert/Schild: wilde Pokemon.
//
// Die Tabellen liegen in bin/archive/field/resident/data_table.gfpak:
//   encount_k.bin / encount_t.bin               versteckte Begegnungen (Gras, Angeln ...)
//   encount_symbol_k.bin / encount_symbol_t.bin sichtbare Pokemon in der Spielwelt
// k = Schwert, t = Schild. Beide Versionen werden geschrieben, damit der Mod
// fuer beide Spiele passt.
// ---------------------------------------------------------------------------

#include <QJsonObject>
#include <QList>
#include <QString>

#include "swsh_files.h"
#include "swsh_encounters.h"

namespace swsh {

struct GameTexts;

// ---- Datenformat (EncounterArchive) ----
struct WildSlot {
    int probability = 0;
    int species = 0;
    int form = 0;
};
struct WildSubTable {
    int levelMin = 0;
    int levelMax = 0;
    QList<WildSlot> entries; // "slots" ist in Qt ein Makro
};
struct WildZone {
    quint64 zoneId = 0;
    QList<WildSubTable> subTables; // 0-8 Wetter, 9 Baeume schuetteln, 10 Angeln
};
struct WildArchive {
    quint32 field0 = 0;
    QList<WildZone> zones;
    bool load(const QByteArray& data);
    QByteArray save() const;
};

// Ortsname einer Zone (aus den Spieltexten, ohne "auf"/"in der" ...)
QString zoneName(quint64 zoneId, const QStringList& placeNames);

// ---- Einstellungen ----
struct WildSettings {
    bool enabled = false;
    int mode = 0;                 // 0 pro Gebiet, 1 global 1:1, 2 jeder Platz zufaellig
    bool levelAppropriate = true; // keine Entwicklungen vor ihrem Level
    bool sameType = false;        // neues Pokemon teilt einen Typ mit dem alten
    bool similarStrength = false; // Basiswerte-Summe aehnlich (optional)
    bool legendaries = false;
};

QJsonObject wildSettingsToJson(const WildSettings& s);
void wildSettingsFromJson(const QJsonObject& json, WildSettings& s);

struct WildResult {
    QByteArray dataTable;         // neue data_table.gfpak (leer = unveraendert)
    // Fuer das Spoiler-Log: pro Datei und Zone die neuen Arten
    struct ZoneLog {
        bool symbol = false;      // sichtbar in der Spielwelt
        bool shield = false;      // Datei fuer Schild
        quint64 zoneId = 0;
        int levelMin = 0, levelMax = 0;
        QList<QPair<StarterChoice, StarterChoice>> pairs; // (alt, neu), ohne Doppelte
    };
    QList<ZoneLog> zones;
};

bool randomizeWild(const QString& romfs, const GameFiles& files, const WildSettings& settings, quint64 seed,
                   WildResult& result, QString* error);
bool writeWild(const QString& outRomfs, const WildResult& result);
QList<SpoilerSection> wildSpoiler(const WildResult& result, const GameFiles& files, const GameTexts& texts,
                                  const QStringList& placeNames, Version version);

} // namespace swsh

#endif // SWSH_WILD_H
