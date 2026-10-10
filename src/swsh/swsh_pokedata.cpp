// ---------------------------------------------------------------------------
// Pokemon Schwert/Schild: Pokemon-Daten (Faehigkeiten, Typen, Basiswerte,
// Level-Attacken, TM/TP-Kompatibilitaet)
// ---------------------------------------------------------------------------

#include "headers/swsh/swsh_extras.h"

#include <QHash>
#include <QRandomGenerator>
#include <QSet>
#include <algorithm>

namespace swsh {

namespace {

// Faehigkeiten, die nur bei einem bestimmten Pokemon funktionieren oder das Spiel kaputt machen
const QSet<int> kBannedAbilities = {25, 59, 121, 122, 161, 176, 197, 208, 209, 210, 211, 213, 225, 241, 248, 258, 266, 267};
constexpr int kMaxAbility = 267;

// Attacken, die nie in Lernsets landen sollen (Verzweifler, Dyna-Attacken)
bool isBannedMove(int id) {
    return id == 165 || id == 743 || (id >= 757 && id <= 774);
}

// Union-Find ueber Entwicklungen: jede Entwicklungsreihe bekommt eine Wurzel
class Families {
public:
    explicit Families(const GameFiles& files) {
        const int n = files.personal.count();
        parent.resize(n);
        for (int i = 0; i < n; i++) parent[i] = i;
        for (int i = 1; i < n; i++) {
            for (const Evolution& e : files.evolutions.get(i)) {
                const int target = files.personal.indexOf(e.species, e.form);
                if (target > 0) unite(i, target);
            }
        }
    }
    int root(int i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    }

private:
    void unite(int a, int b) {
        a = root(a);
        b = root(b);
        if (a != b) parent[qMax(a, b)] = qMin(a, b); // kleinster Index (meist die Grundform) ist die Wurzel
    }
    QVector<int> parent;
};

} // namespace

PokemonDataResult randomizePokemonData(GameFiles& files, const PokemonDataSettings& s, quint64 seed) {
    PokemonDataResult result;
    QRandomGenerator rng(seed ^ 0xDA7ADA7AULL);
    Families families(files);
    PersonalTable& p = files.personal;
    const int count = p.count();

    auto familyKey = [&](int index) { return s.keepFamilies ? families.root(index) : index; };

    // ------------------------------------------------------------ Typen
    if (s.types) {
        QHash<int, QVector<int>> mapping; // Familie -> Typ-Zuordnung (alt -> neu)
        for (int i = 1; i < count; i++) {
            const int key = familyKey(i);
            if (!mapping.contains(key)) {
                QVector<int> perm(18);
                for (int t = 0; t < 18; t++) perm[t] = t;
                for (int t = 17; t > 0; t--) std::swap(perm[t], perm[static_cast<int>(rng.bounded(t + 1))]);
                mapping[key] = perm;
            }
            const QVector<int>& perm = mapping[key];
            const int t1 = p.type1(i), t2 = p.type2(i);
            if (t1 > 17 || t2 > 17) continue;
            p.setTypes(i, perm[t1], perm[t2]);
            result.types++;
        }
        result.personalChanged = true;
    }

    // ---------------------------------------------------------- Basiswerte
    if (s.stats) {
        QHash<int, QVector<int>> mapping; // Familie -> Reihenfolge der 6 Werte
        for (int i = 1; i < count; i++) {
            const QPair<int,int> sf = p.speciesForm(i);
            if (sf.first == 292) continue; // Ninjatom (1 KP) bleibt
            const int key = familyKey(i);
            if (!mapping.contains(key)) {
                QVector<int> perm = {0, 1, 2, 3, 4, 5};
                for (int k = 5; k > 0; k--) std::swap(perm[k], perm[static_cast<int>(rng.bounded(k + 1))]);
                mapping[key] = perm;
            }
            const QVector<int>& perm = mapping[key];
            int old[6];
            for (int k = 0; k < 6; k++) old[k] = p.stat(i, k);
            for (int k = 0; k < 6; k++) p.setStat(i, k, old[perm[k]]);
            result.stats++;
        }
        result.personalChanged = true;
    }

    // -------------------------------------------------------- Faehigkeiten
    if (s.abilities) {
        QList<int> pool;
        for (int a = 1; a <= kMaxAbility; a++) {
            if (!kBannedAbilities.contains(a)) pool.append(a);
        }
        auto random = [&]() { return pool[static_cast<int>(rng.bounded(static_cast<int>(pool.size())))]; };
        QHash<int, QVector<int>> chosen; // Familie -> 3 Faehigkeiten
        for (int i = 1; i < count; i++) {
            const int a1 = p.ability(i, 0), a2 = p.ability(i, 1), ah = p.ability(i, 2);
            if (a1 <= 0) continue;
            // Pokemon mit Spezial-Faehigkeit (z. B. Wunderwache, Kostuem) behalten sie
            if (kBannedAbilities.contains(a1) || kBannedAbilities.contains(a2) || kBannedAbilities.contains(ah)) continue;
            const int key = familyKey(i);
            if (!chosen.contains(key)) {
                QVector<int> abilities(3);
                abilities[0] = random();
                do { abilities[1] = random(); } while (abilities[1] == abilities[0] && pool.size() > 2);
                do { abilities[2] = random(); } while ((abilities[2] == abilities[0] || abilities[2] == abilities[1]) && pool.size() > 3);
                chosen[key] = abilities;
            }
            const QVector<int>& a = chosen[key];
            p.setAbility(i, 0, a[0]);
            p.setAbility(i, 1, a1 == a2 ? a[0] : a[1]); // nur eine Faehigkeit im Original -> bleibt eine
            p.setAbility(i, 2, a[2]);
            result.abilities++;
        }
        result.personalChanged = true;
    }

    // ------------------------------------------------------------ TM / TP
    if (s.tmMode != 0) {
        for (int i = 1; i < count; i++) {
            if (!p.isPresent(i)) continue;
            int learned = 0;
            for (int k = 0; k < 100; k++) learned += (p.canLearnTM(i, k) ? 1 : 0) + (p.canLearnTR(i, k) ? 1 : 0);
            if (s.tmMode == 2) {
                for (int k = 0; k < 100; k++) {
                    p.setTM(i, k, true);
                    p.setTR(i, k, true);
                }
            } else {
                // Zufaellig, aber gleich viele wie im Original
                QVector<int> bitOrder(200);
                for (int k = 0; k < 200; k++) bitOrder[k] = k;
                for (int k = 199; k > 0; k--) std::swap(bitOrder[k], bitOrder[static_cast<int>(rng.bounded(k + 1))]);
                for (int k = 0; k < 100; k++) {
                    p.setTM(i, k, false);
                    p.setTR(i, k, false);
                }
                for (int k = 0; k < learned; k++) {
                    if (bitOrder[k] < 100) p.setTM(i, bitOrder[k], true);
                    else p.setTR(i, bitOrder[k] - 100, true);
                }
            }
            result.tms++;
        }
        result.personalChanged = true;
    }

    // ---------------------------------------------------- Level-Attacken
    if (s.levelMoves) {
        QList<int> damaging, status;
        QHash<int, QList<int>> damagingByType;
        QList<int> weakDamaging;
        for (const Move& m : files.moves.all()) {
            if (!m.canUse || m.id <= 0 || isBannedMove(m.id)) continue;
            if (m.category != 0 && m.power > 1) {
                damaging.append(m.id);
                damagingByType[m.type].append(m.id);
                if (m.power <= 50) weakDamaging.append(m.id);
            } else {
                status.append(m.id);
            }
        }
        auto pickFrom = [&](const QList<int>& list) { return list[static_cast<int>(rng.bounded(static_cast<int>(list.size())))]; };

        for (int i = 1; i < count && i < files.learnsets.count(); i++) {
            if (!p.isPresent(i)) continue;
            QList<QPair<int,int>> moves = files.learnsets.get(i);
            if (moves.isEmpty()) continue;
            const QList<int> ownTypes = {p.type1(i), p.type2(i)};
            QSet<int> used;
            for (auto& entry : moves) {
                int id = 0;
                for (int tries = 0; tries < 30; tries++) {
                    // etwa 2/3 Schadensattacken, davon die Haelfte vom eigenen Typ
                    if (rng.bounded(100) < 65) {
                        const QList<int> stab = damagingByType.value(ownTypes[static_cast<int>(rng.bounded(2))]);
                        id = (rng.bounded(100) < 50 && !stab.isEmpty()) ? pickFrom(stab) : pickFrom(damaging);
                    } else {
                        id = pickFrom(status);
                    }
                    if (!used.contains(id)) break;
                }
                used.insert(id);
                entry.first = id;
            }
            // Schadensattacken nach Staerke sortieren: frueh schwach, spaet stark
            QList<int> positions;
            QList<int> damagingMoves;
            for (int k = 0; k < moves.size(); k++) {
                const Move m = files.moves.get(moves[k].first);
                if (m.category != 0 && m.power > 0 && moves[k].second > 0) {
                    positions.append(k);
                    damagingMoves.append(moves[k].first);
                }
            }
            std::sort(damagingMoves.begin(), damagingMoves.end(), [&](int a, int b) {
                return files.moves.get(a).power < files.moves.get(b).power;
            });
            for (int k = 0; k < positions.size(); k++) moves[positions[k]].first = damagingMoves[k];
            // Die erste Attacke (niedrigstes Level) ist immer eine schwache Schadensattacke
            int first = -1;
            for (int k = 0; k < moves.size(); k++) {
                if (moves[k].second > 0 && (first < 0 || moves[k].second < moves[first].second)) first = k;
            }
            if (first >= 0) {
                const Move m = files.moves.get(moves[first].first);
                if ((m.category == 0 || m.power <= 0) && !weakDamaging.isEmpty()) moves[first].first = pickFrom(weakDamaging);
            }
            files.learnsets.set(i, moves);
            result.learnsets++;
        }
        result.learnsetsChanged = true;
    }
    return result;
}

} // namespace swsh
