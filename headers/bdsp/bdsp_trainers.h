#ifndef BDSP_TRAINERS_H
#define BDSP_TRAINERS_H

// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Trainer randomisieren.
//
//  - Arenen (Arenaleiter und Arenatrainer) und die Top Vier behalten ihren Typ
//  - Vollentwickelte Pokemon ab einstellbarem Level, level-passende Entwicklungen
//  - Starke Movesets (gemeinsame Logik mit Karmesin/Purpur und Schwert/Schild)
//  - Keine doppelten Arten in einem Team
// Die Optionen sind dieselben wie bei Schwert/Schild (ohne Gigadynamax).
// ---------------------------------------------------------------------------

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "bdsp_files.h"
#include "../swsh/swsh_trainers.h"

namespace bdsp {

using TrainerOptions = swsh::TrainerOptions;

enum TrainerGroup {
    GroupRivals = 0,
    GroupGymLeaders,
    GroupGymTrainers,
    GroupEliteFour,
    GroupGalactic,
    GroupRoutes,
    GroupRematches,
    GroupCount
};

struct GroupInfo {
    QString id;
    QString title;
    QString info;
};
GroupInfo groupInfo(int group);

enum GroupMode { SameAsBase = 0, OwnSettings = 1, KeepOriginal = 2 };

struct TrainerSettings {
    bool enabled = false;
    bool keepTypeTheme = true;
    TrainerOptions base;
    int modes[GroupCount] = {};
    TrainerOptions own[GroupCount];
    const TrainerOptions* optionsFor(int group) const; // nullptr = Gruppe bleibt original
};

QJsonObject settingsToJson(const TrainerSettings& s);
void settingsFromJson(const QJsonObject& json, TrainerSettings& s);

// Regeln fuer zufaellige Pokemon (gelten auch fuer spaetere Bereiche)
bool isLegendary(int species);
bool isExcludedForm(int species, int form); // nur im Kampf oder nur mit Item

int trainerGroup(const GameFiles& files, const Trainer& trainer);
int trainerTheme(const GameFiles& files, const Trainer& trainer); // Typ-ID oder -1

struct TrainerRandomizerResult {
    int randomized = 0;
};

// Randomisiert files.trainers (in place)
TrainerRandomizerResult randomizeTrainers(GameFiles& files, const TrainerSettings& settings, quint64 seed);

// Zusaetzlicher Abschnitt im Spoiler-Log (Starter, Geschenke ...)
struct SpoilerSection {
    QString title;
    QString html;
    int count = 0;
};

bool writeSpoiler(const QString& path, const GameFiles& files, const GameTexts& texts,
                  const TrainerSettings& settings, const QList<SpoilerSection>& extra,
                  const QString& seedText, Version version);

} // namespace bdsp

#endif // BDSP_TRAINERS_H
