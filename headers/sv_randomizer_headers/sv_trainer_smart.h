#ifndef SV_TRAINER_SMART_H
#define SV_TRAINER_SMART_H

// ---------------------------------------------------------------------------
// TrainerSmartData
//
// Hilfsklasse fuer intelligentere Trainer-Teams:
//  - "Vollentwickelt ab Level X"
//  - Level-passende Entwicklungen (kein Garados auf Level 12)
//  - Starke Movesets (Level-Up-Attacken inkl. Vorentwicklungen, optional TMs)
//
// Die Klasse wird einmal pro Randomisierung aufgebaut und danach in den
// Threads nur noch gelesen (alle Abfragen sind const).
// ---------------------------------------------------------------------------

#include <nlohmann/json.hpp>
#include <QHash>
#include <QList>
#include <QSet>
#include <QPair>
#include <QStringList>
#include <QRandomGenerator>

using json = nlohmann::json;

class TrainerSmartData {
public:
    // mapping:   pokemon_mapping.json
    // personal:  personal_array (clean oder bereits randomisiert)
    // wazaTable: waza_array_clean.json
    // moveNames: sorted_move_list.json (fuer die WAZA_-Namen)
    void build(const json& mapping, const json& personal, const json& wazaTable, const json& moveNames);

    bool isFullyEvolved(int natdex, int form) const;
    int  minimumLevel(int natdex, int form) const;

    // Prueft alle Level-Regeln fuer einen Kandidaten.
    // fullyEvolvedFrom <= 0 bedeutet: Regel aus.
    bool isAllowedAtLevel(int natdex, int form, int level, int fullyEvolvedFrom, bool levelAppropriate) const;

    // Liefert bis zu 4 Attacken als WAZA_-Namen. Leer = keine Daten, Spiel-Standard verwenden.
    QStringList buildMoveset(int natdex, int form, int level, bool includeTMs, QRandomGenerator& rng) const;

    bool isReady() const { return ready; }

    // Typen einer Form (fuer das Spoiler-Log). first = Typ 1, second = Typ 2
    QPair<int,int> types(int natdex, int form) const;

    // Attacken, die das Spiel bei "DEFAULT" waehlt: die letzten 4 per Level gelernten
    QList<int> defaultMoveset(int natdex, int form, int level) const;

private:
    struct MoveInfo {
        int id = 0;
        int type = 0;
        int category = 0;   // 0 Status, 1 Physisch, 2 Speziell
        int power = 0;
        int accuracy = 100;
        double avgHits = 1.0;
        bool damaging = false;   // brauchbare Schadensattacke
        int statusRole = 0;      // 0 = keine gute Statusattacke, 1 = physisches Setup, 2 = spezielles Setup, 3 = allgemein nuetzlich
        std::string devName;
    };

    struct PokeInfo {
        int type1 = 0;
        int type2 = 0;
        int atk = 0;
        int spa = 0;
        QList<QPair<int,int>> levelMoves; // (moveId, level) ; level 0 oder >= 250 = beim Entwickeln
        QList<int> tmMoves;
    };

    static qint64 key(int natdex, int form) { return static_cast<qint64>(natdex) * 1000 + form; }
    int computeMinLevel(qint64 k, QSet<qint64>& visiting);
    void collectMovePool(qint64 k, int level, bool includeTMs, QSet<int>& pool, QSet<qint64>& visited) const;

    bool ready = false;
    QHash<qint64, int> stage;                 // 0 = keine Entwicklung, 1 = Basis, 2 = Mitte, 3 = Endstufe
    QHash<qint64, int> minLevel;
    QHash<qint64, QList<QPair<qint64,int>>> parents; // Ziel -> (Vorentwicklung, Bedingung/Level-Info)
    QHash<qint64, int> parentEvoLevel;        // Level der Entwicklung (0 = nicht per Level)
    QHash<qint64, int> parentEvoCondition;
    QHash<qint64, PokeInfo> pokeInfo;
    QHash<int, MoveInfo> moves;
};

#endif // SV_TRAINER_SMART_H
