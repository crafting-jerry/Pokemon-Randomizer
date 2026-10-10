// ---------------------------------------------------------------------------
// Pokemon Schwert/Schild: Dynamax-Abenteuer (Dyna-Hoehle) und Kampfturm
// ---------------------------------------------------------------------------

#include "headers/swsh/swsh_extras.h"
#include "headers/swsh/swsh_trainers.h"
#include "headers/common/trainer_smart.h"

#include <QHash>
#include <QMap>
#include <functional>
#include <QRandomGenerator>
#include <QSet>
#include <algorithm>

namespace swsh {

namespace {

const QString kLairFile = "bin/appli/chika/data_table/underground_exploration_poke.bin";
const QString kTowerFile = "bin/field/param/battle_tower/battle_tower_poke_table.bin";

// EncounterUnderground
namespace lair {
enum Field { IsSingleCapture, CaptureFlag, Field02, Form, GigantamaxState, Ball, IndexNum, Level, Species, UiMessage,
             OtGender, Version, Shiny, IvSpe, IvAtk, IvDef, IvHp, IvSpa, IvSpd, Ability, StoryGated,
             Move0, Move1, Move2, Move3 };
const QVector<int> kSizes = {1, 8, 1, 1, 4, 4, 4, 4, 4, 8, 4, 1, 4, 1, 1, 1, 1, 1, 1, 4, 1, 4, 4, 4, 4};
}
// BattleTowerPoke
namespace tower {
enum Field { F00, F01, F02, F03, F04, F05, F06, Form, F08, HeldItem, Species, EntryId, F0C, Nature, F0E,
             IvHp, IvAtk, IvDef, IvSpa, IvSpd, IvSpe, F15, Move0, Move1, Move2, Move3 };
const QVector<int> kSizes = {1, 1, 1, 4, 1, 1, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
}

const QSet<int> kSpeciesItems = {236, 259, 258, 256, 257, 274, 225, 226, 227, 1103, 1104, 135, 136, 112};

struct Mon {
    int species = 0;
    int form = 0;
};

QList<Mon> candidates(const GameFiles& files, const std::function<bool(int species, int form, int index)>& filter) {
    QList<Mon> list;
    for (int species = 1; species <= kMaxSpecies; species++) {
        const int forms = qMax(1, files.personal.formCount(files.personal.indexOf(species, 0)));
        for (int form = 0; form < forms; form++) {
            const int index = files.personal.indexOf(species, form);
            if (index <= 0 || !files.personal.isPresent(index) || isExcludedForm(species, form)) continue;
            if (filter(species, form, index)) list.append({species, form});
        }
    }
    return list;
}

// Gleichmaessig ueber Arten; vermeidet bereits benutzte Arten, solange es geht
bool pickMon(QRandomGenerator& rng, const QList<Mon>& pool, const QSet<int>& avoid, Mon& out) {
    for (int unique = 1; unique >= 0; unique--) {
        QHash<int, QList<int>> bySpecies;
        QList<int> order;
        for (const Mon& m : pool) {
            if (unique && avoid.contains(m.species)) continue;
            if (!bySpecies.contains(m.species)) order.append(m.species);
            bySpecies[m.species].append(m.form);
        }
        if (!order.isEmpty()) {
            const int species = order[static_cast<int>(rng.bounded(static_cast<int>(order.size())))];
            const QList<int>& forms = bySpecies[species];
            out = {species, forms[static_cast<int>(rng.bounded(static_cast<int>(forms.size())))]};
            return true;
        }
    }
    return false;
}

} // namespace

QJsonObject facilitySettingsToJson(const FacilitySettings& s) {
    QJsonObject j;
    j["maxLair"] = s.maxLair;
    j["maxLairLegends"] = s.maxLairLegends;
    j["tower"] = s.tower;
    j["towerFullyEvolved"] = s.towerFullyEvolved;
    j["towerLegendaries"] = s.towerLegendaries;
    return j;
}

void facilitySettingsFromJson(const QJsonObject& j, FacilitySettings& s) {
    s.maxLair = j["maxLair"].toBool(s.maxLair);
    s.maxLairLegends = j["maxLairLegends"].toBool(s.maxLairLegends);
    s.tower = j["tower"].toBool(s.tower);
    s.towerFullyEvolved = j["towerFullyEvolved"].toBool(s.towerFullyEvolved);
    s.towerLegendaries = j["towerLegendaries"].toBool(s.towerLegendaries);
}

bool randomizeFacilities(const QString& romfs, const GameFiles& files, const FacilitySettings& s, quint64 seed,
                         FacilityResult& result, QString* error) {
    result = FacilityResult();
    if (!s.anyEnabled()) return true;

    TrainerSmartData smart;
    smart.build(files.toGameData());
    QRandomGenerator rng(seed ^ 0xFAC11177ULL);
    auto moveset = [&](const Mon& m, int level) {
        QList<int> moves = smart.buildMovesetIds(m.species, m.form, level, true, rng);
        if (moves.isEmpty()) moves = smart.defaultMoveset(m.species, m.form, level);
        while (moves.size() < 4) moves.append(0);
        return moves;
    };

    // --------------------------------------------------------- Dyna-Hoehle
    if (s.maxLair || s.maxLairLegends) {
        bool ok = false;
        QList<FlatRecord> rows = readFlatArchive(readFile(romfs + "/" + kLairFile), lair::kSizes, &ok);
        if (!ok || rows.isEmpty()) {
            if (error) *error = "Die Daten der Dyna-Höhle (underground_exploration_poke.bin) konnten nicht gelesen werden.";
            return false;
        }
        const QList<Mon> normal = candidates(files, [&](int species, int, int index) {
            return !isLegendary(species) && files.personal.canDynamax(index);
        });
        const QList<Mon> legends = candidates(files, [&](int species, int, int index) {
            return isLegendary(species) && files.personal.canDynamax(index);
        });
        QSet<int> usedNormal, usedLegends;
        for (FlatRecord& r : rows) {
            const bool legendary = r.get(lair::IsSingleCapture) != 0;
            if ((legendary && !s.maxLairLegends) || (!legendary && !s.maxLair)) continue;
            const bool gmax = r.get(lair::GigantamaxState) == 2;
            QList<Mon> pool = legendary ? legends : normal;
            if (gmax) {
                QList<Mon> g;
                for (const Mon& m : pool) if (canGigantamax(m.species, m.form)) g.append(m);
                if (!g.isEmpty()) pool = g;
            }
            Mon m;
            if (!pickMon(rng, pool, legendary ? usedLegends : usedNormal, m)) continue;
            (legendary ? usedLegends : usedNormal).insert(m.species);
            r.set(lair::Species, m.species);
            r.set(lair::Form, m.form);
            const bool newGmax = gmax && canGigantamax(m.species, m.form);
            r.set(lair::GigantamaxState, newGmax ? 2 : 1);
            const QList<int> moves = moveset(m, static_cast<int>(r.get(lair::Level)));
            r.set(lair::Move0, moves[0]);
            r.set(lair::Move1, moves[1]);
            r.set(lair::Move2, moves[2]);
            r.set(lair::Move3, moves[3]);
            (legendary ? result.lairLegends : result.lairPokemon).append({m.species, m.form + (newGmax ? 1000 : 0)});
        }
        result.lair = writeFlatArchive(rows, lair::kSizes);
    }

    // ----------------------------------------------------------- Kampfturm
    if (s.tower) {
        bool ok = false;
        QList<FlatRecord> rows = readFlatArchive(readFile(romfs + "/" + kTowerFile), tower::kSizes, &ok);
        if (!ok || rows.isEmpty()) {
            if (error) *error = "Die Kampfturm-Daten (battle_tower_poke_table.bin) konnten nicht gelesen werden.";
            return false;
        }
        const QList<Mon> pool = candidates(files, [&](int species, int form, int) {
            if (!s.towerLegendaries && isLegendary(species)) return false;
            return !s.towerFullyEvolved || smart.isFullyEvolved(species, form);
        });
        // Viele Eintraege: jede Art kommt erst wieder, wenn alle anderen einmal dran waren
        QSet<int> used;
        for (FlatRecord& r : rows) {
            if (r.get(tower::Species) <= 0) continue;
            Mon m;
            if (!pickMon(rng, pool, used, m)) continue;
            used.insert(m.species);
            if (used.size() >= pool.size()) used.clear();
            r.set(tower::Species, m.species);
            r.set(tower::Form, m.form);
            const QList<int> moves = moveset(m, 50);
            r.set(tower::Move0, moves[0]);
            r.set(tower::Move1, moves[1]);
            r.set(tower::Move2, moves[2]);
            r.set(tower::Move3, moves[3]);
            if (r.get(tower::HeldItem) <= 0 || kSpeciesItems.contains(static_cast<int>(r.get(tower::HeldItem)))) {
                r.set(tower::HeldItem, randomBattleItem(rng));
            }
            result.towerPokemon.append({m.species, m.form});
        }
        result.tower = writeFlatArchive(rows, tower::kSizes);
    }
    return true;
}

bool writeFacilities(const QString& outRomfs, const FacilityResult& result) {
    bool ok = true;
    if (!result.lair.isEmpty()) ok &= writeFile(outRomfs + "/" + kLairFile, result.lair);
    if (!result.tower.isEmpty()) ok &= writeFile(outRomfs + "/" + kTowerFile, result.tower);
    return ok;
}

// ------------------------------------------------------------ Spoiler-Log

QList<SpoilerSection> dataSpoiler(const PokemonDataResult& data, const FacilityResult& facilities, const GameFiles& files,
                                  const GameTexts& texts) {
    auto esc = [](const QString& t) { return t.toHtmlEscaped(); };
    QList<SpoilerSection> sections;

    // Pokemon-Daten: Uebersicht aller Pokemon im Spiel mit Typen, Werten und Faehigkeiten
    if (data.personalChanged) {
        SpoilerSection s;
        s.title = "Pokémon-Daten";
        const QStringList abilityNames = texts.abilities;
        QString rows;
        for (int species = 1; species <= kMaxSpecies; species++) {
            const int forms = qMax(1, files.personal.formCount(files.personal.indexOf(species, 0)));
            for (int form = 0; form < forms; form++) {
                const int index = files.personal.indexOf(species, form);
                if (index <= 0 || !files.personal.isPresent(index) || isExcludedForm(species, form)) continue;
                const QString name = texts.pokemonName(species, form);
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
                    const QString n = (a >= 0 && a < abilityNames.size()) ? abilityNames[a] : QString::number(a);
                    if (!abilities.contains(n)) abilities << (k == 2 ? n + " (V)" : n);
                }
                rows += "<tr data-search=\"" + esc((name + " " + abilities.join(" ")).toLower()) + "\"><td><b>" + esc(name) +
                        "</b></td><td>" + types + "</td><td class=\"num\">" + stats.join(" / ") + " = " +
                        QString::number(total) + "</td><td>" + esc(abilities.join(", ")) + "</td></tr>";
                s.count++;
            }
        }
        s.html = "<p class=\"hint\">Werte: KP / Angriff / Verteidigung / Initiative / Sp.-Angriff / Sp.-Verteidigung. "
                 "(V) = versteckte Fähigkeit.</p><table class=\"enc\"><thead><tr><th>Pokémon</th><th>Typ</th><th>Basiswerte</th>"
                 "<th>Fähigkeiten</th></tr></thead><tbody>" + rows + "</tbody></table>";
        sections.append(s);
    }

    auto chipSection = [&](const QString& title, const QString& hint, const QList<StarterChoice>& list) {
        SpoilerSection s;
        s.title = title;
        QMap<QString, int> counts;
        for (const StarterChoice& c : list) {
            const QString name = texts.pokemonName(c.species, c.form % 1000) + (c.form >= 1000 ? " (G)" : "");
            counts[name]++;
        }
        QStringList chips;
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
            chips << "<span class=\"wild\" data-search=\"" + esc(it.key().toLower()) + "\">" + esc(it.key()) +
                         (it.value() > 1 ? " ×" + QString::number(it.value()) : QString()) + "</span>";
        }
        s.count = list.size();
        s.html = "<p class=\"hint\">" + esc(hint) + "</p><div>" + chips.join(" ") + "</div>";
        sections.append(s);
    };
    if (!facilities.lairPokemon.isEmpty()) {
        chipSection("Dyna-Höhle: Leih- und Gegner-Pokémon",
                    "Alle Pokémon, die in Dynamax-Abenteuern als Leih-Pokémon oder Gegner vorkommen. (G) = Gigadynamax.",
                    facilities.lairPokemon);
    }
    if (!facilities.lairLegends.isEmpty()) {
        chipSection("Dyna-Höhle: Legendäre", "Die Legendären am Ende der Dyna-Höhle.", facilities.lairLegends);
    }
    if (!facilities.towerPokemon.isEmpty()) {
        chipSection("Kampfturm", "Alle Pokémon, aus denen die Teams im Kampfturm zusammengestellt werden.",
                    facilities.towerPokemon);
    }
    return sections;
}

} // namespace swsh
