#ifndef SV_RAIDS_H
#define SV_RAIDS_H

// ---------------------------------------------------------------------------
// Karmesin/Purpur: Tera-Raids (1★ bis 6★) in Paldea, Kitakami (DLC 1) und
// der Blaubeer-Akademie (DLC 2).
//
// Pro Raid-Tabelle (Region + Sternzahl) bekommt jeder Eintrag ein zufaelliges
// Pokemon, moeglichst ohne Doppelte. Level, Tera-Typ-Regel, Faehigkeits-Regel,
// Seltenheit und Edition bleiben erhalten. Die Boss-Attacken werden passend zum
// neuen Pokemon gewaehlt, Wetter/Felder der Zusatzaktionen passen zum Typ.
// ---------------------------------------------------------------------------

#include "sv_shared_class.h"
#include "sv_spoiler_extras.h"

class svRaids: public QObject, public SVShared{
    Q_OBJECT
    public:
        svRaids();
        ~svRaids();

        // --- Neue Oberflaeche ---
        bool levelAppropriate = true;     // keine Entwicklungen vor ihrem Level; ab 5★ nur Endstufen
        bool sameType = false;            // optional: teilt einen Typ mit dem Original
        bool similarStrength = false;     // optional: aehnliche Basiswerte-Summe
        bool forceShiny = false;          // alle Raid-Pokemon schillernd
        allowedPokemonLimiter raidPokemon; // erlaubte Pokemon (fuer alle Regionen)
        json* personalOverride = nullptr; // randomisierte Pokemon-Daten (Typen, Werte, Attacken)
        QList<SvRaidSpoilerTable> spoilerTables; // fuers Spoiler-Log

        // Regionen (auch von der klassischen Ansicht genutzt)
        bool randomizePaldea = false;
        bool randomizeKitakami = false;
        bool randomizeBlueberry = false;

        // --- Klassische Ansicht (aeltere Felder, bleiben erhalten) ---
        bool paldeaForAll = false;
        int shinyRatePaldea = 10;
        bool forceShinyRaidsPaldea = false;
        allowedPokemonLimiter paldeaPokemon;
        int shinyRateKitakami = 10;
        bool forceShinyRaidsKitakami = false;
        allowedPokemonLimiter kitakamiPokemon;
        int shinyRateBlueberry = 10;
        bool forceShinyRaidsBlueberry = false;
        allowedPokemonLimiter blueberryPokemon;
        bool forNoWarning = false;

        QMap<int, QList<int>> allowedPokemon;

        bool anyEnabled() const { return randomizePaldea || randomizeKitakami || randomizeBlueberry; }
        // Randomisiert alle eingeschalteten Regionen. Fehler (z. B. zu wenige erlaubte Pokemon) -> false
        bool randomize(QString* error);
};

#endif // SV_RAIDS_H
