// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Pokemon-Daten und Kampfturm
// ---------------------------------------------------------------------------

#include "headers/bdsp/bdsp_pokedata.h"
#include "headers/common/trainer_smart.h"

#include <QHash>
#include <QMap>
#include <QRandomGenerator>
#include <QSet>
#include <QVector>
#include <algorithm>
#include <climits>

namespace bdsp {

namespace {

// Faehigkeiten, die nur bei einem bestimmten Pokemon funktionieren (Wunderwache, Prognose, Variabilitaet,
// Pflanzengabe und weitere aus spaeteren Generationen)
const QSet<int> kBannedAbilities = {25, 59, 121, 122, 161, 176, 197, 208, 209, 210, 211, 213, 225, 241, 248, 258, 266, 267};

// Attacken, die nie in Lernsets landen sollen (Verzweifler)
bool isBannedMove(int id) { return id == 165; }

// Entwicklungsarten (wie Schwert/Schild)
enum EvoMethod { LevelUp = 4, Trade = 5, TradeHeldItem = 6, TradeWithSpecies = 7, HeldItemDay = 19, HeldItemNight = 20 };
constexpr int kTradeLevel = 37;

// Items, die nur einem bestimmten Pokemon nuetzen (Kampfturm: werden durch Kampf-Items ersetzt)
const QSet<int> kSpeciesItems = {112, 135, 136, 225, 226, 227, 236, 256, 257, 258, 259, 274, 298, 299, 300, 301, 302,
                                 303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313};
// Kampf-Items (wie bei den Trainer-Items, nur 4. Generation)
const QList<int> kBattleItems = {234, 270, 220, 297, 287, 275, 158, 157, 268, 269, 217, 253, 266, 267, 232, 213, 221,
                                 214, 219, 155, 230, 276, 265, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 248,
                                 249, 250, 251};

// Union-Find ueber Entwicklungen: jede Entwicklungsreihe bekommt eine Wurzel
class Families {
public:
    explicit Families(const GameFiles& files) {
        const int n = files.personal.count();
        parent.resize(n);
        for (int i = 0; i < n; i++) parent[i] = i;
        for (int i = 1; i < n && i < files.evolutions.count(); i++) {
            for (const Evolution& e : files.evolutions.get(i)) {
                const int target = files.personal.indexOf(e.species, e.form);
                if (target > 0 && target < n) unite(i, target);
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
        if (a != b) parent[qMax(a, b)] = qMin(a, b);
    }
    QVector<int> parent;
};

QList<EvolutionChange> removeTradeEvolutions(GameFiles& files, const GameTexts& texts) {
    QList<EvolutionChange> changes;
    for (int index = 1; index < files.personal.count() && index < files.evolutions.count(); index++) {
        const QList<Evolution> original = files.evolutions.get(index);
        bool hasTrade = false;
        for (const Evolution& e : original) {
            hasTrade |= e.method == Trade || e.method == TradeHeldItem || e.method == TradeWithSpecies;
        }
        if (!hasTrade) continue;

        const QPair<int, int> from = files.personal.speciesForm(index);
        QList<Evolution> first; // Item-Entwicklungen zuerst (z. B. Flegmon: King-Stein vor Level 37)
        QList<Evolution> rest;
        for (const Evolution& e : original) {
            EvolutionChange c{from.first, from.second, e.species, e.form, QString(), QString()};
            if (e.method == Trade || e.method == TradeWithSpecies) {
                Evolution n = e;
                n.method = LevelUp;
                n.argument = 0;
                n.level = kTradeLevel;
                rest.append(n);
                c.before = "Tausch";
                c.after = QString("ab Level %1").arg(kTradeLevel);
            } else if (e.method == TradeHeldItem) {
                Evolution day = e;
                day.method = HeldItemDay;
                day.level = 0;
                Evolution night = day;
                night.method = HeldItemNight;
                first.append(day);
                first.append(night);
                c.before = "Tausch mit " + texts.itemName(e.argument);
                c.after = "Level-Aufstieg, während es " + texts.itemName(e.argument) + " trägt";
            } else {
                rest.append(e);
                continue;
            }
            changes.append(c);
        }
        files.evolutions.set(index, first + rest);
        files.evolutionsChanged = true;
    }
    return changes;
}

} // namespace

QJsonObject pokemonDataSettingsToJson(const PokemonDataSettings& s) {
    QJsonObject j;
    j["tradeEvolutions"] = s.tradeEvolutions;
    j["abilities"] = s.abilities;
    j["types"] = s.types;
    j["stats"] = s.stats;
    j["levelMoves"] = s.levelMoves;
    j["tmMode"] = s.tmMode;
    j["keepFamilies"] = s.keepFamilies;
    return j;
}

void pokemonDataSettingsFromJson(const QJsonObject& j, PokemonDataSettings& s) {
    s.tradeEvolutions = j["tradeEvolutions"].toBool(s.tradeEvolutions);
    s.abilities = j["abilities"].toBool(s.abilities);
    s.types = j["types"].toBool(s.types);
    s.stats = j["stats"].toBool(s.stats);
    s.levelMoves = j["levelMoves"].toBool(s.levelMoves);
    s.tmMode = qBound(0, j["tmMode"].toInt(s.tmMode), 2);
    s.keepFamilies = j["keepFamilies"].toBool(s.keepFamilies);
}

PokemonDataResult randomizePokemonData(GameFiles& files, const GameTexts& texts, const PokemonDataSettings& s,
                                       quint64 seed) {
    PokemonDataResult result;
    if (!s.anyEnabled()) return result;
    if (s.tradeEvolutions) result.evolutions = removeTradeEvolutions(files, texts);

    QRandomGenerator rng(seed ^ 0xDA7AB05DULL);
    Families families(files);
    PersonalTable& p = files.personal;
    const int count = p.count();
    auto familyKey = [&](int index) { return s.keepFamilies ? families.root(index) : index; };

    // ------------------------------------------------------------ Typen
    if (s.types) {
        QHash<int, QVector<int>> mapping; // Familie -> Typ-Zuordnung (alt -> neu)
        for (int i = 1; i < count; i++) {
            if (!p.isPresent(i)) continue;
            const int key = familyKey(i);
            if (!mapping.contains(key)) {
                QVector<int> perm(18);
                for (int t = 0; t < 18; t++) perm[t] = t;
                for (int t = 17; t > 0; t--) std::swap(perm[t], perm[static_cast<int>(rng.bounded(t + 1))]);
                mapping[key] = perm;
            }
            const QVector<int>& perm = mapping[key];
            const int t1 = p.type1(i), t2 = p.type2(i);
            if (t1 < 0 || t2 < 0 || t1 > 17 || t2 > 17) continue;
            p.set(i, "type1", perm[t1]);
            p.set(i, "type2", perm[t2]);
            result.types++;
        }
        files.personalChanged = true;
    }

    // ---------------------------------------------------------- Basiswerte
    if (s.stats) {
        QHash<int, QVector<int>> mapping; // Familie -> Reihenfolge der 6 Werte
        for (int i = 1; i < count; i++) {
            if (!p.isPresent(i)) continue;
            if (p.speciesForm(i).first == 292) continue; // Ninjatom (1 KP) bleibt
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
        files.personalChanged = true;
    }

    // -------------------------------------------------------- Faehigkeiten
    if (s.abilities) {
        // Nur Faehigkeiten, die im Spiel bei einem Pokemon vorkommen (alle anderen sind in BDSP nicht sicher)
        QSet<int> inGame;
        for (int i = 1; i < count; i++) {
            if (!p.isPresent(i)) continue;
            for (int k = 0; k < 3; k++) inGame.insert(p.ability(i, k));
        }
        QList<int> pool;
        for (int a : inGame) {
            if (a > 0 && !kBannedAbilities.contains(a)) pool.append(a);
        }
        std::sort(pool.begin(), pool.end());
        auto random = [&]() { return pool[static_cast<int>(rng.bounded(static_cast<int>(pool.size())))]; };
        QHash<int, QVector<int>> chosen; // Familie -> 3 Faehigkeiten
        for (int i = 1; i < count && pool.size() > 3; i++) {
            if (!p.isPresent(i)) continue;
            const int a1 = p.ability(i, 0), a2 = p.ability(i, 1), ah = p.ability(i, 2);
            if (a1 <= 0) continue;
            if (kBannedAbilities.contains(a1) || kBannedAbilities.contains(a2) || kBannedAbilities.contains(ah)) continue;
            const int key = familyKey(i);
            if (!chosen.contains(key)) {
                QVector<int> a(3);
                a[0] = random();
                do { a[1] = random(); } while (a[1] == a[0]);
                do { a[2] = random(); } while (a[2] == a[0] || a[2] == a[1]);
                chosen[key] = a;
            }
            const QVector<int>& a = chosen[key];
            p.setAbility(i, 0, a[0]);
            p.setAbility(i, 1, a1 == a2 ? a[0] : a[1]); // nur eine Faehigkeit im Original -> bleibt eine
            p.setAbility(i, 2, a[2]);
            result.abilities++;
        }
        files.personalChanged = true;
    }

    // -------------------------------------------------------------- TMs
    if (s.tmMode != 0) {
        QList<int> machines = files.tmMoves.keys();
        std::sort(machines.begin(), machines.end());
        for (int i = 1; i < count; i++) {
            if (!p.isPresent(i)) continue;
            int learned = 0;
            for (int m : machines) learned += p.canLearnTM(i, m) ? 1 : 0;
            if (s.tmMode == 2) {
                for (int m : machines) p.setTM(i, m, true);
            } else {
                // Zufaellig, aber gleich viele wie im Original
                QList<int> order = machines;
                for (int k = static_cast<int>(order.size()) - 1; k > 0; k--) {
                    std::swap(order[k], order[static_cast<int>(rng.bounded(k + 1))]);
                }
                for (int m : machines) p.setTM(i, m, false);
                for (int k = 0; k < learned && k < order.size(); k++) p.setTM(i, order[k], true);
            }
            result.tms++;
        }
        files.personalChanged = true;
    }

    // ---------------------------------------------------- Level-Attacken
    if (s.levelMoves) {
        QList<int> damaging, status, weakDamaging;
        QHash<int, QList<int>> damagingByType;
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
        auto pickFrom = [&](const QList<int>& list) {
            return list[static_cast<int>(rng.bounded(static_cast<int>(list.size())))];
        };
        for (int i = 1; i < count && i < files.learnsets.count(); i++) {
            if (!p.isPresent(i)) continue;
            QList<QPair<int, int>> moves = files.learnsets.get(i);
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
            QList<int> positions, damagingMoves;
            for (int k = 0; k < moves.size(); k++) {
                const Move m = files.moves.get(moves[k].first);
                if (m.category != 0 && m.power > 0 && moves[k].second > 0) {
                    positions.append(k);
                    damagingMoves.append(moves[k].first);
                }
            }
            std::stable_sort(damagingMoves.begin(), damagingMoves.end(), [&](int a, int b) {
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
        files.learnsetsChanged = true;
    }
    return result;
}

// ================================================================ Kampfturm

QJsonObject towerSettingsToJson(const TowerSettings& s) {
    QJsonObject j;
    j["enabled"] = s.enabled;
    j["fullyEvolved"] = s.fullyEvolved;
    j["legendaries"] = s.legendaries;
    return j;
}

void towerSettingsFromJson(const QJsonObject& j, TowerSettings& s) {
    s.enabled = j["enabled"].toBool(s.enabled);
    s.fullyEvolved = j["fullyEvolved"].toBool(s.fullyEvolved);
    s.legendaries = j["legendaries"].toBool(s.legendaries);
}

bool randomizeTower(GameFiles& files, const TowerSettings& s, quint64 seed, TowerResult& result, QString* error) {
    result = TowerResult();
    if (!s.enabled) return true;
    if (!files.masterBundle.has("TowerTrainerTable")) {
        if (error) *error = "Die Kampfturm-Daten (TowerTrainerTable) konnten nicht gelesen werden.";
        return false;
    }
    TrainerSmartData smart;
    smart.build(files.toGameData());
    QRandomGenerator rng(seed ^ 0x70E570E5ULL);

    QList<int> pool;
    for (int species = 1; species <= kMaxSpecies; species++) {
        const int index = files.personal.indexOf(species, 0);
        if (index <= 0 || !files.personal.isPresent(index)) continue;
        if (!s.legendaries && isLegendary(species)) continue;
        if (s.fullyEvolved && !smart.isFullyEvolved(species, 0)) continue;
        pool.append(species);
    }
    if (pool.size() < 8) {
        if (error) *error = "Für den Kampfturm wurden zu wenige passende Pokémon gefunden.";
        return false;
    }

    std::vector<UValue>& pokes = files.masterBundle.table("TowerTrainerTable")["TrainerPoke"].list();
    QHash<qint64, int> byId; // Pokemon-ID -> Zeile
    for (int k = 0; k < static_cast<int>(pokes.size()); k++) byId.insert(pokes[static_cast<size_t>(k)]["ID"].i, k);

    // Welche Pokemon zusammen in einem Team stehen (Einzel- und Doppel-Teams): dort keine doppelten Arten
    QVector<QSet<int>> neighbours(static_cast<int>(pokes.size()));
    for (const auto& table : {qMakePair(QString("TowerSingleStockTable"), QString("TowerSingleStock")),
                              qMakePair(QString("TowerDoubleStockTable"), QString("TowerDoubleStock"))}) {
        if (!files.masterBundle.has(table.first)) continue;
        for (const UValue& team : files.masterBundle.table(table.first)[table.second.toUtf8().constData()].list()) {
            QList<int> rows;
            for (const UValue& id : team["PokeID"].list()) {
                if (byId.contains(id.i)) rows.append(byId.value(id.i));
            }
            for (int a : rows)
                for (int b : rows)
                    if (a != b) neighbours[a].insert(b);
            result.teams++;
        }
    }

    // Reihenfolge zufaellig; jede Art moeglichst gleich oft (die seltensten erlaubten zuerst)
    QVector<int> order(static_cast<int>(pokes.size()));
    for (int k = 0; k < order.size(); k++) order[k] = k;
    for (int k = order.size() - 1; k > 0; k--) std::swap(order[k], order[static_cast<int>(rng.bounded(k + 1))]);
    QVector<int> assigned(static_cast<int>(pokes.size()), 0);
    QHash<int, int> uses;
    for (int row : order) {
        UValue& poke = pokes[static_cast<size_t>(row)];
        if (poke["MonsNo"].toInt() <= 0) continue;
        QSet<int> blocked;
        for (int other : neighbours[row]) blocked.insert(assigned[other]);
        int fewest = INT_MAX;
        QList<int> candidates;
        for (int species : pool) {
            if (blocked.contains(species)) continue;
            const int u = uses.value(species);
            if (u < fewest) {
                fewest = u;
                candidates.clear();
            }
            if (u == fewest) candidates.append(species);
        }
        if (candidates.isEmpty()) continue; // kann bei 3-4 Pokemon pro Team praktisch nicht passieren
        const int species = candidates[static_cast<int>(rng.bounded(static_cast<int>(candidates.size())))];
        assigned[row] = species;
        uses[species]++;

        const int index = files.personal.indexOf(species, 0);
        poke["MonsNo"].i = species;
        poke["FormNo"].i = 0;
        poke["Sex"].i = 3;
        poke["Tokusei"].i = files.personal.ability(index, static_cast<int>(rng.bounded(2)));
        const int level = qMax(1, poke["Level"].toInt());
        QList<int> moves = smart.buildMovesetIds(species, 0, level, true, rng);
        if (moves.isEmpty()) moves = smart.defaultMoveset(species, 0, level);
        while (moves.size() < 4) moves.append(0);
        poke["Waza1"].i = moves[0];
        poke["Waza2"].i = moves[1];
        poke["Waza3"].i = moves[2];
        poke["Waza4"].i = moves[3];
        const int item = poke["Item"].toInt();
        if (item <= 0 || kSpeciesItems.contains(item)) {
            poke["Item"].i = kBattleItems[static_cast<int>(rng.bounded(static_cast<int>(kBattleItems.size())))];
        }
    }
    for (int row = 0; row < assigned.size(); row++) {
        if (assigned[row] > 0) result.pokemon.append(assigned[row]);
    }
    files.towerChanged = !result.pokemon.isEmpty();
    return true;
}

// ============================================================== Spoiler-Log

QList<SpoilerSection> pokemonDataSpoiler(const PokemonDataResult& data, const TowerResult& tower, const GameFiles& files,
                                         const GameTexts& texts) {
    auto esc = [](const QString& t) { return t.toHtmlEscaped(); };
    QList<SpoilerSection> sections;

    if (!data.evolutions.isEmpty()) {
        SpoilerSection s;
        s.title = "Tausch-Entwicklungen";
        QString rows;
        for (const EvolutionChange& c : data.evolutions) {
            const QString from = texts.pokemonName(c.fromSpecies, c.fromForm);
            const QString to = texts.pokemonName(c.toSpecies, c.toForm);
            rows += "<tr data-search=\"" + esc((from + " " + to).toLower()) + "\"><td><b>" + esc(from) + "</b></td>"
                    "<td class=\"arrow\">→</td><td><b>" + esc(to) + "</b></td><td class=\"hint\">" + esc(c.before) +
                    "</td><td>" + esc(c.after) + "</td></tr>";
            s.count++;
        }
        s.html = "<table class=\"enc\"><thead><tr><th>Pokémon</th><th></th><th>Entwicklung</th><th>Vorher</th>"
                 "<th>Jetzt</th></tr></thead><tbody>" + rows + "</tbody></table>";
        sections.append(s);
    }

    // Uebersicht aller Pokemon mit Typen, Werten und Faehigkeiten
    if (data.types + data.stats + data.abilities > 0) {
        SpoilerSection s;
        s.title = "Pokémon-Daten";
        QString rows;
        for (int species = 1; species <= kMaxSpecies; species++) {
            const int index = files.personal.indexOf(species, 0);
            if (index <= 0 || !files.personal.isPresent(index)) continue;
            const QString name = texts.pokemonName(species, 0);
            const int t1 = files.personal.type1(index), t2 = files.personal.type2(index);
            QString types = "<span class=\"type t" + QString::number(t1) + "\">" + esc(texts.typeName(t1)) + "</span>";
            if (t2 != t1) types += "<span class=\"type t" + QString::number(t2) + "\">" + esc(texts.typeName(t2)) + "</span>";
            QStringList stats;
            int total = 0;
            for (int k = 0; k < 6; k++) {
                stats << QString::number(files.personal.stat(index, k));
                total += files.personal.stat(index, k);
            }
            QStringList abilities;
            for (int k = 0; k < 3; k++) {
                const int a = files.personal.ability(index, k);
                const QString n = (a >= 0 && a < texts.abilities.size()) ? texts.abilities[a] : QString::number(a);
                if (!abilities.contains(n) && !abilities.contains(n + " (V)")) abilities << (k == 2 ? n + " (V)" : n);
            }
            rows += "<tr data-search=\"" + esc((name + " " + abilities.join(" ")).toLower()) + "\"><td><b>" + esc(name) +
                    "</b></td><td>" + types + "</td><td class=\"num\">" + stats.join(" / ") + " = " +
                    QString::number(total) + "</td><td>" + esc(abilities.join(", ")) + "</td></tr>";
            s.count++;
        }
        s.html = "<p class=\"hint\">Werte: KP / Angriff / Verteidigung / Initiative / Sp.-Angriff / Sp.-Verteidigung. "
                 "(V) = versteckte Fähigkeit.</p><table class=\"enc\"><thead><tr><th>Pokémon</th><th>Typ</th>"
                 "<th>Basiswerte</th><th>Fähigkeiten</th></tr></thead><tbody>" + rows + "</tbody></table>";
        sections.append(s);
    }

    if (!tower.pokemon.isEmpty()) {
        SpoilerSection s;
        s.title = "Kampfturm";
        QMap<QString, int> counts;
        for (int species : tower.pokemon) counts[texts.pokemonName(species, 0)]++;
        QStringList chips;
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
            chips << "<span class=\"wild\" data-search=\"" + esc(it.key().toLower()) + "\">" + esc(it.key()) +
                         (it.value() > 1 ? " ×" + QString::number(it.value()) : QString()) + "</span>";
        }
        s.count = tower.pokemon.size();
        s.html = "<p class=\"hint\">Alle Pokémon, aus denen die Gegner im Kampfturm ihre Teams zusammenstellen. Kein Team "
                 "enthält eine Art doppelt.</p><div>" + chips.join(" ") + "</div>";
        sections.append(s);
    }
    return sections;
}

} // namespace bdsp
