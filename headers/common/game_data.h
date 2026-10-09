#ifndef COMMON_GAME_DATA_H
#define COMMON_GAME_DATA_H

// ---------------------------------------------------------------------------
// GameData: spielneutrales Datenmodell fuer die Team-Logik (TrainerSmartData).
// Jedes Spiel (Karmesin/Purpur, Schwert/Schild, ...) fuellt dieses Modell aus
// seinen eigenen Dateien. Alle Pokemon werden ueber (Nationaldex, Form)
// angesprochen, alle Attacken ueber ihre nationale Attacken-ID.
// ---------------------------------------------------------------------------

#include <QList>
#include <QPair>
#include <string>

struct GameData {
    // Jede bekannte Form (auch solche, die nicht im Spiel sind)
    struct Form {
        int natdex = 0;
        int form = 0;
        int stage = 0;              // Entwicklungsstufe laut Spiel (fuer Sonderfaelle bei Regionalformen)
        bool fullyEvolved = false;  // Endstufe oder keine Entwicklung
    };

    // Werte, Typen und Attacken einer Form
    struct Stats {
        int natdex = 0;
        int form = 0;
        int type1 = 0;
        int type2 = 0;
        int atk = 0;
        int spa = 0;
        QList<QPair<int,int>> levelMoves; // (Attacke, Level); Level 0 oder >= 250 = beim Entwickeln
        QList<int> tmMoves;
    };

    // Eine Entwicklung von -> nach (Reihenfolge wie im Spiel)
    struct Evolution {
        int fromNatdex = 0;
        int fromForm = 0;
        int toNatdex = 0;
        int toForm = 0;
        int level = 0;      // 0 = nicht per Level
        int condition = 0;  // Entwicklungsmethode (gleiche Nummerierung in Gen 8 und 9)
    };

    struct Move {
        int id = 0;
        int type = 0;
        int category = 0;   // 0 Status, 1 Physisch, 2 Speziell
        int power = 0;
        int accuracy = 100; // > 100 = trifft immer
        int hitMin = 0;
        int hitMax = 0;
        int priority = 0;
        bool canUse = true;
        bool charge = false;
        bool recharge = false;
        bool futureAttack = false;
        std::string devName; // nur Karmesin/Purpur (WAZA_...)
    };

    QList<Form> forms;
    QList<Stats> stats;
    QList<Evolution> evolutions;
    QList<Move> moves;
};

#endif // COMMON_GAME_DATA_H
