#ifndef SV_SPOILER_EXTRAS_H
#define SV_SPOILER_EXTRAS_H

// ---------------------------------------------------------------------------
// Karmesin/Purpur: zusaetzliche Abschnitte fuer das Spoiler-Log (Tera-Raids).
// Die Raid-Randomisierung fuellt die Tabellen, svTrainers::writeSpoilerLog
// zeigt sie im selben Stil wie die Trainer an.
// ---------------------------------------------------------------------------

#include <QList>
#include <QString>
#include <QStringList>

struct SvRaidSpoilerMon {
    int natdex = 0;
    int form = 0;
    int oldNatdex = 0;
    int oldForm = 0;
    bool shiny = false;
    QString tera;          // gemType ("RANDOM", "DEFAULT" oder ein Typ)
    QList<int> moves;      // Attacken-IDs (leer = Spiel-Standard)
    QString edition;       // leer = beide Editionen, sonst "Karmesin" oder "Purpur"
};

struct SvRaidSpoilerTable {
    QString region;        // "Paldea", "Kitakami", "Blaubeer-Akademie"
    int stars = 0;
    int level = 0;
    QList<SvRaidSpoilerMon> mons;
};

#endif // SV_SPOILER_EXTRAS_H
