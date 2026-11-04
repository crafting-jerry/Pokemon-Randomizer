#ifndef SV_RUNNER_H
#define SV_RUNNER_H

// ---------------------------------------------------------------------------
// Karmesin/Purpur: fuehrt den Randomizer mit den Einstellungen in code() aus.
// Die Einstellungen setzt die Oberflaeche (ModernRandomizerWindow).
// Ergebnis: Randomizers-Output/Karmesin-Purpur/Randomizer-1 (romfs, ggf. Spoiler-Log)
// ---------------------------------------------------------------------------

#include <QString>
#include "svrandomizercode.h"

class SVRandomizerRunner {
public:
    SVRandomizerCode& code() { return randomizer; }

    // false + Fehlermeldung, wenn etwas schiefgeht
    bool run(QString* error);
    QString lastOutputFolder; // Ordner des letzten erfolgreichen Durchlaufs

private:
    bool runSteps(QString* error);
    SVRandomizerCode randomizer;
};

#endif // SV_RUNNER_H
