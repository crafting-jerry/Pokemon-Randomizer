#include "headers/common/trainer_smart.h"

#include <algorithm>
#include <cmath>

namespace {

// Schadensattacken, die fuer Trainer-KI unbrauchbar oder schaedlich sind
const QSet<int> kBannedDamageMoves = {
    120, 153, 802,       // Finale, Explosion, Nebelexplosion
    138,                 // Traumfresser (braucht schlafendes Ziel)
    264,                 // Power-Punch
    387,                 // Zuflucht
    562,                 // Ruelpser
    485,                 // Synchronoise
    363, 374, 255,       // Beerenkraefte, Schleuder, Entfessler
    165,                 // Verzweifler
    515,                 // Wagemut
    720, 796,            // Knallkopf, Stahlstrahl
    173,                 // Schnarcher
    248, 353,            // Seher, Kismetwunsch
    68, 243, 368         // Konter, Spiegelcape, Metallsound
};

// Gute Statusattacken (Rolle: 1 = physisches Setup, 2 = spezielles Setup, 3 = allgemein nuetzlich)
const QHash<int,int> kGoodStatusMoves = {
    {14, 1},  // Schwerttanz
    {349, 1}, // Drachentanz
    {339, 1}, // Protzer
    {468, 1}, // Klauenwetzer
    {489, 1}, // Einrollen
    {417, 2}, // Raenkeschmied
    {347, 2}, // Gedankengut
    {483, 2}, // Falterreigen
    {504, 3}, // Hausbruch
    {74, 3},  // Wachstum
    {97, 3},  // Agilitaet
    {105, 3}, // Genesung
    {355, 3}, // Ruheort
    {208, 3}, // Milchgetraenk
    {234, 3}, // Morgengrauen
    {235, 3}, // Synthese
    {236, 3}, // Mondschein
    {303, 3}, // Tagedieb
    {456, 3}, // Heilbefehl
    {92, 3},  // Toxin
    {261, 3}, // Irrlicht
    {86, 3},  // Donnerwelle
    {79, 3},  // Schlafpuder
    {147, 3}, // Pilzspore
    {446, 3}, // Tarnsteine
    {191, 3}, // Stachler
    {390, 3}  // Giftspitzen
};

bool hasType(int type1, int type2, int type) {
    return type1 == type || type2 == type;
}

} // namespace

void TrainerSmartData::build(const GameData& data) {
    stage.clear();
    fullyEvolved.clear();
    minLevel.clear();
    parents.clear();
    parentEvoLevel.clear();
    parentEvoCondition.clear();
    pokeInfo.clear();
    moves.clear();

    // --- Alle bekannten Formen ---
    for (const GameData::Form& f : data.forms) {
        stage[key(f.natdex, f.form)] = f.stage;
        fullyEvolved[key(f.natdex, f.form)] = f.fullyEvolved;
    }

    // --- Typen, Werte, Attacken ---
    for (const GameData::Stats& s : data.stats) {
        if (s.natdex <= 0) {
            continue;
        }
        PokeInfo info;
        info.type1 = s.type1;
        info.type2 = s.type2;
        info.atk = s.atk;
        info.spa = s.spa;
        info.levelMoves = s.levelMoves;
        info.tmMoves = s.tmMoves;
        pokeInfo[key(s.natdex, s.form)] = info;
    }

    // --- Entwicklungen ---
    for (const GameData::Evolution& e : data.evolutions) {
        qint64 target = key(e.toNatdex, e.toForm);
        parents[target].append(qMakePair(key(e.fromNatdex, e.fromForm), 0));
        // Bei mehreren Vorentwicklungen zaehlt die frueheste Moeglichkeit (siehe computeMinLevel)
        if (!parentEvoLevel.contains(target)) {
            parentEvoLevel[target] = e.level;
            parentEvoCondition[target] = e.condition;
        }
    }

    // --- Attacken-Daten ---
    for (const GameData::Move& m : data.moves) {
        MoveInfo info;
        info.id = m.id;
        info.type = m.type;
        info.category = m.category;
        info.power = m.power;
        info.accuracy = std::min(100, std::max(1, m.accuracy));
        info.devName = m.devName;

        if (m.hitMax > 1) {
            info.avgHits = std::min(3.0, (std::max(m.hitMin, 1) + m.hitMax) / 2.0);
        }

        bool usable = m.canUse && info.id > 0;
        bool clumsy = m.recharge || m.charge || m.futureAttack || m.priority < 0;

        info.damaging = usable && info.category != 0 && info.power > 1 && !clumsy &&
                        !kBannedDamageMoves.contains(info.id);
        info.statusRole = (usable && kGoodStatusMoves.contains(info.id)) ? kGoodStatusMoves.value(info.id) : 0;

        moves[info.id] = info;
    }

    // --- Mindestlevel fuer jede Form berechnen ---
    QSet<qint64> visiting;
    for (auto it = stage.constBegin(); it != stage.constEnd(); ++it) {
        computeMinLevel(it.key(), visiting);
    }

    ready = true;
}

int TrainerSmartData::computeMinLevel(qint64 k, QSet<qint64>& visiting) {
    if (minLevel.contains(k)) {
        return minLevel.value(k);
    }
    if (visiting.contains(k)) {
        return 1; // Schutz vor Endlosschleifen bei randomisierten Entwicklungen
    }
    visiting.insert(k);

    int natdex = static_cast<int>(k / 1000);
    int form = static_cast<int>(k % 1000);
    int result = 1;

    if (!parents.contains(k)) {
        // Regionalformen ohne eigene Vorentwicklung (z. B. Alola-Raichu): Wert der Grundform uebernehmen
        if (form != 0 && stage.value(k) >= 2 && parents.contains(key(natdex, 0))) {
            result = computeMinLevel(key(natdex, 0), visiting);
        }
    } else {
        result = 100;
        int evoLevel = parentEvoLevel.value(k, 0);
        int condition = parentEvoCondition.value(k, 0);

        for (const auto& parent : parents.value(k)) {
            int parentMin = computeMinLevel(parent.first, visiting);
            int candidate;
            if (evoLevel > 0) {
                candidate = std::max(evoLevel, parentMin);                 // Entwicklung per Level
            } else if (condition == 1 || condition == 2 || condition == 3) {
                candidate = std::max(parentMin + 10, 16);                  // Freundschaft
            } else if (parents.contains(parent.first)) {
                candidate = std::max(parentMin + 10, 40);                  // Item/Tausch, dritte Stufe
            } else {
                candidate = std::max(parentMin + 10, 30);                  // Item/Tausch, zweite Stufe
            }
            result = std::min(result, candidate);
        }
    }

    visiting.remove(k);
    minLevel[k] = result;
    return result;
}

bool TrainerSmartData::isFullyEvolved(int natdex, int form) const {
    // Unbekannte Formen gelten als vollentwickelt (wie bisher Stufe 0)
    return fullyEvolved.value(key(natdex, form), true);
}

int TrainerSmartData::minimumLevel(int natdex, int form) const {
    return minLevel.value(key(natdex, form), 1);
}

bool TrainerSmartData::isAllowedAtLevel(int natdex, int form, int level, int fullyEvolvedFrom, bool levelAppropriate) const {
    if (fullyEvolvedFrom > 0 && level >= fullyEvolvedFrom && !isFullyEvolved(natdex, form)) {
        return false;
    }
    if (levelAppropriate && minimumLevel(natdex, form) > level) {
        return false;
    }
    return true;
}

void TrainerSmartData::collectMovePool(qint64 k, int level, bool includeTMs, QSet<int>& pool, QSet<qint64>& visited) const {
    if (visited.contains(k)) {
        return;
    }
    visited.insert(k);

    qint64 lookup = k;
    if (!pokeInfo.contains(lookup)) {
        lookup = key(static_cast<int>(k / 1000), 0); // Fallback auf Grundform
    }
    if (pokeInfo.contains(lookup)) {
        const PokeInfo& info = pokeInfo[lookup];
        for (const auto& lm : info.levelMoves) {
            // Level 0 bzw. >= 250 sind Entwicklungsattacken
            if (lm.second <= level || lm.second == 0 || lm.second >= 250) {
                pool.insert(lm.first);
            }
        }
        if (includeTMs) {
            for (int tm : info.tmMoves) {
                pool.insert(tm);
            }
        }
    }

    // Attacken der Vorentwicklungen bis zum aktuellen Level
    for (const auto& parent : parents.value(k)) {
        collectMovePool(parent.first, level, false, pool, visited);
    }
}

QList<int> TrainerSmartData::buildMovesetIds(int natdex, int form, int level, bool includeTMs, QRandomGenerator& rng) const {
    QList<int> result;
    if (!ready) {
        return result;
    }

    qint64 k = key(natdex, form);
    QSet<int> pool;
    QSet<qint64> visited;
    collectMovePool(k, level, includeTMs, pool, visited);
    if (pool.isEmpty()) {
        return result;
    }

    PokeInfo self = pokeInfo.value(k, pokeInfo.value(key(natdex, 0)));
    bool physicalMon = self.atk >= self.spa;

    // Bewertung der Schadensattacken
    struct Scored { int id; int type; double score; };
    QList<Scored> damaging;
    QList<int> setupMoves;
    QList<int> utilityMoves;

    for (int id : pool) {
        if (!moves.contains(id)) {
            continue;
        }
        const MoveInfo& m = moves[id];
        if (m.damaging) {
            double score = m.power * m.avgHits * (m.accuracy / 100.0);
            if (hasType(self.type1, self.type2, m.type)) {
                score *= 1.5; // Typ-Bonus
            }
            if (m.category == 1 && self.atk < self.spa * 0.8) {
                score *= 0.6; // physische Attacke auf speziellem Pokemon
            } else if (m.category == 2 && self.spa < self.atk * 0.8) {
                score *= 0.6; // spezielle Attacke auf physischem Pokemon
            }
            score *= 0.9 + rng.generateDouble() * 0.2; // etwas Abwechslung
            damaging.append({id, m.type, score});
        } else if (m.statusRole == 1 && physicalMon) {
            setupMoves.append(id);
        } else if (m.statusRole == 2 && !physicalMon) {
            setupMoves.append(id);
        } else if (m.statusRole == 3) {
            utilityMoves.append(id);
        }
    }

    // Bis zu 3 Schadensattacken, gleiche Typen werden abgewertet (Abdeckung)
    QSet<int> usedTypes;
    QList<int> chosen;
    int damageSlots = (setupMoves.isEmpty() && utilityMoves.isEmpty()) ? 4 : 3;
    while (chosen.size() < damageSlots && !damaging.isEmpty()) {
        int bestIndex = -1;
        double bestScore = -1;
        for (int i = 0; i < damaging.size(); i++) {
            double s = damaging[i].score * (usedTypes.contains(damaging[i].type) ? 0.5 : 1.0);
            if (s > bestScore) {
                bestScore = s;
                bestIndex = i;
            }
        }
        chosen.append(damaging[bestIndex].id);
        usedTypes.insert(damaging[bestIndex].type);
        damaging.removeAt(bestIndex);
    }

    // Eine Statusattacke: Setup bevorzugt, sonst allgemein nuetzlich
    if (chosen.size() < 4) {
        if (!setupMoves.isEmpty()) {
            chosen.append(setupMoves[rng.bounded(static_cast<int>(setupMoves.size()))]);
        } else if (!utilityMoves.isEmpty()) {
            chosen.append(utilityMoves[rng.bounded(static_cast<int>(utilityMoves.size()))]);
        }
    }

    // Restliche Plaetze mit weiteren Schadensattacken auffuellen
    while (chosen.size() < 4 && !damaging.isEmpty()) {
        int bestIndex = 0;
        for (int i = 1; i < damaging.size(); i++) {
            if (damaging[i].score > damaging[bestIndex].score) {
                bestIndex = i;
            }
        }
        chosen.append(damaging[bestIndex].id);
        damaging.removeAt(bestIndex);
    }

    result = chosen;
    return result;
}

QStringList TrainerSmartData::buildMoveset(int natdex, int form, int level, bool includeTMs, QRandomGenerator& rng) const {
    QStringList result;
    for (int id : buildMovesetIds(natdex, form, level, includeTMs, rng)) {
        result.append(QString::fromStdString(moves.value(id).devName));
    }
    return result;
}

QPair<int,int> TrainerSmartData::types(int natdex, int form) const {
    qint64 k = key(natdex, form);
    if (!pokeInfo.contains(k)) {
        k = key(natdex, 0);
    }
    if (!pokeInfo.contains(k)) {
        return qMakePair(0, 0);
    }
    const PokeInfo info = pokeInfo.value(k);
    return qMakePair(info.type1, info.type2);
}

QList<int> TrainerSmartData::defaultMoveset(int natdex, int form, int level) const {
    QList<int> result;
    qint64 k = key(natdex, form);
    if (!pokeInfo.contains(k)) {
        k = key(natdex, 0);
    }
    if (!pokeInfo.contains(k)) {
        return result;
    }

    QList<QPair<int,int>> learnset = pokeInfo.value(k).levelMoves;
    std::stable_sort(learnset.begin(), learnset.end(), [](const QPair<int,int>& a, const QPair<int,int>& b){
        return a.second < b.second;
    });
    for (const auto& lm : learnset) {
        if (lm.second >= 250 || lm.second > level) {
            continue;
        }
        result.removeAll(lm.first);
        result.append(lm.first);
    }
    while (result.size() > 4) {
        result.removeFirst();
    }
    return result;
}
