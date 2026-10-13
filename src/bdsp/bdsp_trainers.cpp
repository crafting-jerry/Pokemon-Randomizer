// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Trainer randomisieren
// ---------------------------------------------------------------------------

#include "headers/bdsp/bdsp_trainers.h"
#include "headers/common/trainer_smart.h"
#include "headers/common/spoiler_html.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QSet>

namespace bdsp {

namespace {

// Trainerklassen (LabelTrType ohne Praefix "DP_Trainers_Type_")
QString classKey(const Trainer& t) {
    QString c = t.classLabel;
    c.remove("DP_Trainers_Type_");
    return c;
}

// Arenatrainer, die nicht in PlaceData stehen (Ewigenau: Silvanas Arena mit Uhr-Raetsel)
const QSet<int> kExtraGymTrainers = {183, 184, 248, 269};
// Trainer der Revanchen (Kampfradar), Index-Bereich im Spiel
constexpr int kRematchFirst = 501;
constexpr int kRematchLast = 651;
// Multi-Kaempfe mit Partner (Gegner und Partner): hoechstens 3 Pokemon, ausser das Original hat mehr
const QSet<int> kMultiBattleTrainers = {401, 312, 317, 318, 674, 675, 481, 493, 494,
                                        482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492,
                                        495, 496, 497, 498, 499, 500};

// Typ-IDs: 0 Normal, 1 Kampf, 2 Flug, 3 Gift, 4 Boden, 5 Gestein, 6 Kaefer, 7 Geist, 8 Stahl,
//          9 Feuer, 10 Wasser, 11 Pflanze, 12 Elektro, 13 Psycho, 14 Eis, 15 Drache, 16 Unlicht, 17 Fee
int leaderType(const QString& cls) {
    static const QHash<QString, int> map = {
        {"TR_LEADER1_01", 5},  // Veit: Gestein
        {"TR_LEADER2_01", 11}, // Silvana: Pflanze
        {"TR_LEADER3_01", 10}, // Marinus: Wasser
        {"TR_LEADER4_01", 1},  // Hilda: Kampf
        {"TR_LEADER5_01", 7},  // Lamina: Geist
        {"TR_LEADER6_01", 14}, // Frida: Eis
        {"TR_LEADER7_01", 8},  // Adam: Stahl
        {"TR_LEADER8_01", 12}, // Volkner: Elektro
        {"TR_BIGFOUR1_01", 6}, // Herbaro: Kaefer
        {"TR_BIGFOUR2_01", 4}, // Teresa: Boden
        {"TR_BIGFOUR3_01", 9}, // Ignaz: Feuer
        {"TR_BIGFOUR4_01", 13},// Lucian: Psycho
    };
    return map.value(cls, -1);
}

// Arena nach Kartenname (C03GYM0101 -> C03)
int gymTypeByZone(const QString& zone) {
    static const QHash<QString, int> map = {
        {"C03", 5}, {"C04", 11}, {"C06", 10}, {"C07", 1}, {"C05", 7}, {"C09", 14}, {"C02", 8}, {"C08", 12},
    };
    if (!zone.contains("GYM")) return -1;
    return map.value(zone.left(3), -1);
}

bool isGymTrainer(const GameFiles& files, const Trainer& t) {
    if (kExtraGymTrainers.contains(t.index)) return true;
    for (const QString& zone : files.trainerZones.value(t.index)) {
        if (zone.contains("GYM")) return true;
    }
    return false;
}

const QSet<int>& legendarySet() {
    static const QSet<int> set = {144, 145, 146, 150, 151, 243, 244, 245, 249, 250, 251, 377, 378, 379, 380, 381,
                                  382, 383, 384, 385, 386, 480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490,
                                  491, 492, 493};
    return set;
}

// Items, die nur fuer bestimmte Arten gedacht sind
const QSet<int> kSpeciesItems = {236, 259, 258, 256, 257, 274, 225, 226, 227, 135, 136, 112};

struct Candidate {
    int species = 0;
    int form = 0;
};

struct Pools {
    QList<Candidate> all;
    QHash<int, QList<Candidate>> byType;
};

Pools buildPools(const GameFiles& files, bool legendaries) {
    Pools pools;
    for (int species = 1; species <= kMaxSpecies; species++) {
        if (!legendaries && isLegendary(species)) continue;
        const int base = files.personal.indexOf(species, 0);
        if (base <= 0 || !files.personal.isPresent(base)) continue;
        const int forms = files.personal.formCount(base);
        for (int form = 0; form < forms; form++) {
            const int index = files.personal.indexOf(species, form);
            if (index <= 0 || !files.personal.isPresent(index) || isExcludedForm(species, form)) continue;
            Candidate c{species, form};
            pools.all.append(c);
            const int t1 = files.personal.type1(index);
            const int t2 = files.personal.type2(index);
            pools.byType[t1].append(c);
            if (t2 != t1) pools.byType[t2].append(c);
        }
    }
    return pools;
}

} // namespace

// ----------------------------------------------------------- Pokemon-Regeln

bool isLegendary(int species) { return legendarySet().contains(species); }

bool isExcludedForm(int species, int form) {
    if (form == 0) return false;
    switch (species) {
    case 351: // Formeo: nur bei Wetter im Kampf
    case 421: // Kinoso Sonnenform: nur im Kampf
    case 487: // Giratina Urform: braucht den Platinum-Orb
    case 493: // Arceus: Typ nur mit Tafel
        return true;
    default:
        return false;
    }
}

// ------------------------------------------------------------- Einstellungen

GroupInfo groupInfo(int group) {
    switch (group) {
    case GroupRivals:
        return {"rivals", "Rivale und Partner",
                "Perl (dein Rivale) in allen Kämpfen, Lucius/Lucia und die Partner bei Multi-Kämpfen "
                "(Raissa, Urs, Charlie, Avenaro, Orisa)."};
    case GroupGymLeaders:
        return {"gymLeaders", "Arenaleiter",
                "Veit, Silvana, Hilda, Marinus, Lamina, Adam, Frida und Volkner – inklusive der Revanchen nach dem "
                "Abspann."};
    case GroupGymTrainers:
        return {"gymTrainers", "Arenatrainer", "Die Trainer in den acht Arenen vor dem Arenaleiter."};
    case GroupEliteFour:
        return {"eliteFour", "Top Vier und Champ",
                "Herbaro, Teresa, Ignaz, Lucian und Cynthia – auch die Revanchen nach dem Abspann – sowie Morimoto."};
    case GroupGalactic:
        return {"galactic", "Team Galaktik", "Die Rüpel, die Commander Mars, Jupiter und Saturn sowie Boss Zyrus."};
    case GroupRoutes:
        return {"routes", "Routentrainer", "Alle normalen Trainer auf Routen, in Höhlen und in Städten."};
    case GroupRematches:
        return {"rematches", "Revanchen (Kampffahnder)",
                "Die stärkeren Teams, wenn du Routentrainer mit dem Kampffahnder erneut herausforderst."};
    default:
        return {"other", "Sonstige", QString()};
    }
}

const TrainerOptions* TrainerSettings::optionsFor(int group) const {
    if (group < 0 || group >= GroupCount) return &base;
    switch (modes[group]) {
    case KeepOriginal: return nullptr;
    case OwnSettings: return &own[group];
    default: return &base;
    }
}

QJsonObject settingsToJson(const TrainerSettings& s) {
    QJsonObject j;
    j["enabled"] = s.enabled;
    j["keepTypeTheme"] = s.keepTypeTheme;
    j["base"] = swsh::optionsToJson(s.base);
    QJsonObject groups;
    for (int g = 0; g < GroupCount; g++) {
        QJsonObject group;
        group["mode"] = s.modes[g];
        group["own"] = swsh::optionsToJson(s.own[g]);
        groups[groupInfo(g).id] = group;
    }
    j["groups"] = groups;
    return j;
}

void settingsFromJson(const QJsonObject& j, TrainerSettings& s) {
    s.enabled = j["enabled"].toBool(s.enabled);
    s.keepTypeTheme = j["keepTypeTheme"].toBool(s.keepTypeTheme);
    s.base = swsh::optionsFromJson(j["base"].toObject(), s.base);
    const QJsonObject groups = j["groups"].toObject();
    for (int g = 0; g < GroupCount; g++) {
        const QJsonObject group = groups[groupInfo(g).id].toObject();
        s.modes[g] = qBound(0, group["mode"].toInt(s.modes[g]), 2);
        s.own[g] = swsh::optionsFromJson(group["own"].toObject(), s.own[g]);
    }
}

// ------------------------------------------------------------- Einordnung

int trainerGroup(const GameFiles& files, const Trainer& t) {
    const QString c = classKey(t);
    if (c.startsWith("TR_RIVAL") || c.startsWith("TR_HERO") || c.startsWith("TR_BTFIVE")) return GroupRivals;
    if (c.startsWith("TR_LEADER")) return GroupGymLeaders;
    if (c.startsWith("TR_BIGFOUR") || c.startsWith("TR_CHAMPION") || c.startsWith("TR_MORIMOTO")) return GroupEliteFour;
    if (c.startsWith("TR_GINGA")) return GroupGalactic;
    if (isGymTrainer(files, t)) return GroupGymTrainers;
    if (t.index >= kRematchFirst && t.index <= kRematchLast) return GroupRematches;
    return GroupRoutes;
}

int trainerTheme(const GameFiles& files, const Trainer& t) {
    const int byClass = leaderType(classKey(t));
    if (byClass >= 0) return byClass;
    if (kExtraGymTrainers.contains(t.index)) return 11;
    for (const QString& zone : files.trainerZones.value(t.index)) {
        const int type = gymTypeByZone(zone);
        if (type >= 0) return type;
    }
    return -1;
}

// ------------------------------------------------------------- Randomizer

TrainerRandomizerResult randomizeTrainers(GameFiles& files, const TrainerSettings& settings, quint64 seed) {
    TrainerRandomizerResult result;
    if (!settings.enabled) return result;

    TrainerSmartData smart;
    smart.build(files.toGameData());
    const Pools pools[2] = {buildPools(files, false), buildPools(files, true)};
    QRandomGenerator master(seed);

    for (Trainer& trainer : files.trainers.all()) {
        // Jeder Trainer bekommt seinen eigenen Seed -> Ergebnis unabhaengig von anderen Einstellungen
        QRandomGenerator rng(master.generate64());
        if (trainer.team.isEmpty()) continue;
        const TrainerOptions* opt = settings.optionsFor(trainerGroup(files, trainer));
        if (opt == nullptr) continue;
        const Pools& pool = pools[opt->legendaries ? 1 : 0];

        int theme = settings.keepTypeTheme ? trainerTheme(files, trainer) : -1;
        if (theme >= 0 && pool.byType.value(theme).isEmpty()) theme = -1;

        // --- Teamgroesse ---
        const int original = trainer.team.size();
        int maxSize = 6;
        if (trainer.fightType == 1 || kMultiBattleTrainers.contains(trainer.index)) maxSize = qMax(3, original);
        int target = original;
        if (opt->teamSize == 1 && original < maxSize) {
            target = static_cast<int>(rng.bounded(original, maxSize + 1));
        } else if (opt->teamSize == 2) {
            target = maxSize;
        }
        int teamMaxLevel = 1;
        for (const TrainerMon& m : trainer.team) teamMaxLevel = qMax(teamMaxLevel, m.level);
        while (trainer.team.size() < target) {
            TrainerMon extra = trainer.team.last();
            extra.level = teamMaxLevel;
            extra.shiny = false;
            extra.item = 0;
            trainer.team.append(extra);
        }

        // --- Pokemon waehlen ---
        QSet<int> usedSpecies;
        const bool levelRules = opt->fullyEvolvedLevel > 0 || opt->levelAppropriateEvos;
        for (TrainerMon& mon : trainer.team) {
            const int level = mon.level;
            auto pickFrom = [&](const QList<Candidate>& list, bool applyLevel, bool noDuplicates, Candidate& out) {
                QHash<int, QList<int>> bySpecies;
                QList<int> order;
                for (const Candidate& c : list) {
                    if (noDuplicates && usedSpecies.contains(c.species)) continue;
                    if (applyLevel && !smart.isAllowedAtLevel(c.species, c.form, level, opt->fullyEvolvedLevel,
                                                              opt->levelAppropriateEvos)) continue;
                    if (!bySpecies.contains(c.species)) order.append(c.species);
                    bySpecies[c.species].append(c.form);
                }
                if (order.isEmpty()) return false;
                const int species = order[static_cast<int>(rng.bounded(static_cast<int>(order.size())))];
                const QList<int>& forms = bySpecies[species];
                out = {species, forms[static_cast<int>(rng.bounded(static_cast<int>(forms.size())))]};
                return true;
            };
            // 1. alle Regeln, 2. ohne Level-Regeln, 3. Duplikate erlauben (letzter Ausweg)
            const QList<Candidate>& list = theme >= 0 ? pool.byType[theme] : pool.all;
            Candidate pick;
            if (!pickFrom(list, levelRules, true, pick) && !(levelRules && pickFrom(list, false, true, pick)) &&
                !pickFrom(list, levelRules, false, pick) && !(levelRules && pickFrom(list, false, false, pick))) {
                continue; // sollte nie passieren
            }
            usedSpecies.insert(pick.species);

            const int index = files.personal.indexOf(pick.species, pick.form);
            mon.species = pick.species;
            mon.form = pick.form;
            mon.sex = 3; // passend zur Art (zufaellig)
            // Faehigkeit: eine der beiden normalen Faehigkeiten der neuen Art
            const int a1 = files.personal.ability(index, 0);
            const int a2 = files.personal.ability(index, 1);
            mon.ability = (a2 > 0 && a2 != a1 && rng.bounded(2) == 1) ? a2 : a1;
            if (kSpeciesItems.contains(mon.item)) mon.item = 0;

            QList<int> moveset;
            if (opt->smartMovesets) moveset = smart.buildMovesetIds(pick.species, pick.form, level, opt->smartTMs, rng);
            if (moveset.isEmpty()) moveset = smart.defaultMoveset(pick.species, pick.form, level);
            for (int k = 0; k < 4; k++) mon.moves[k] = k < moveset.size() ? moveset[k] : 0;

            if (opt->perfectIVs) {
                for (int& iv : mon.ivs) iv = 31;
            }
            if (opt->shinies && !mon.shiny) mon.shiny = rng.bounded(100) < 50;
        }

        if (opt->smartAI) trainer.aiBit |= 111; // wie Arenaleiter, Top Vier und Champ
        trainer.changed = true;
        result.randomized++;
    }
    return result;
}

// ------------------------------------------------------------ Spoiler-Log

namespace {
QString esc(const QString& text) { return text.toHtmlEscaped(); }
} // namespace

bool writeSpoiler(const QString& path, const GameFiles& files, const GameTexts& texts,
                  const TrainerSettings& settings, const QList<SpoilerSection>& extra,
                  const QString& seedText, Version version) {
    QHash<int, QString> sectionHtml;
    QHash<int, int> sectionCount;
    int total = 0;

    for (const Trainer& trainer : files.trainers.all()) {
        if (!settings.enabled || trainer.team.isEmpty()) continue;
        const int group = trainerGroup(files, trainer);
        const bool original = settings.optionsFor(group) == nullptr;
        const int theme = settings.keepTypeTheme ? trainerTheme(files, trainer) : -1;
        const QString label = texts.trainerLabel(trainer);

        QString cards;
        QString search = label;
        int highest = 0;
        for (const TrainerMon& mon : trainer.team) {
            const int index = files.personal.indexOf(mon.species, mon.form);
            const int t1 = files.personal.type1(index);
            const int t2 = files.personal.type2(index);
            highest = qMax(highest, mon.level);
            const QString name = texts.pokemonName(mon.species, mon.form);
            QStringList moves;
            for (int k = 0; k < 4; k++) {
                if (mon.moves[k] > 0) moves << texts.moveName(mon.moves[k]);
            }
            const QString item = mon.item > 0 ? texts.itemName(mon.item) : QString();
            const QString ability = texts.abilities.value(mon.ability);
            search += " " + name + " " + moves.join(" ") + " " + item;

            QString card = "<div class=\"mon\"><div class=\"mon-head\"><span class=\"mon-name\">" + esc(name) + "</span>";
            if (mon.shiny) card += "<span class=\"shiny\" title=\"Schillernd\">✦</span>";
            card += "<span class=\"lvl\">Lv. " + QString::number(mon.level) + "</span></div>";
            card += "<div class=\"types\"><span class=\"type t" + QString::number(t1) + "\">" + esc(texts.typeName(t1)) + "</span>";
            if (t2 != t1) card += "<span class=\"type t" + QString::number(t2) + "\">" + esc(texts.typeName(t2)) + "</span>";
            card += "</div>";
            if (!ability.isEmpty()) card += "<div class=\"meta\">Fähigkeit: " + esc(ability) + "</div>";
            if (!item.isEmpty()) card += "<div class=\"meta\">Item: " + esc(item) + "</div>";
            card += "<ul class=\"moves\">";
            for (const QString& m : moves) card += "<li>" + esc(m) + "</li>";
            card += "</ul></div>";
            cards += card;
        }

        QStringList sub;
        sub << (trainer.fightType == 1 ? "Doppelkampf" : "Einzelkampf");
        sub << "max. Lv. " + QString::number(highest);
        if (theme >= 0) sub << "Typ: " + texts.typeName(theme);
        if (original) sub << "unverändert";
        sub << "#" + QString::number(trainer.index);

        QString block = "<article class=\"trainer\" data-search=\"" + esc(search.toLower()) + "\"><header><h3>" + esc(label);
        if (theme >= 0) block += " <span class=\"type t" + QString::number(theme) + "\">" + esc(texts.typeName(theme)) + "</span>";
        block += "</h3><div class=\"sub\">" + esc(sub.join(" · ")) + "</div></header><div class=\"team\">" + cards + "</div></article>";
        sectionHtml[group] += block;
        sectionCount[group] += 1;
        total++;
    }

    const QString gameTitle = version == Version::Unknown ? QString("Strahlender Diamant/Leuchtende Perle") : versionName(version);
    const QString info = "Erstellt am " + QDateTime::currentDateTime().toString("dd.MM.yyyy 'um' HH:mm") +
                         (settings.enabled ? " · " + QString::number(total) + " Trainer" : QString()) + " · Seed " + esc(seedText);
    QString html = spoiler::head("BDSP Randomizer", gameTitle, info);
    for (const SpoilerSection& section : extra) {
        html += "<details class=\"section\" data-cat=\"" + esc(section.title) + "\" open>";
        html += "<summary>" + esc(section.title) + "<span class=\"count\">" + QString::number(section.count) +
                " Einträge</span></summary><div class=\"section-body\">" + section.html + "</div></details>";
    }
    for (int g = 0; g < GroupCount; g++) {
        if (!sectionHtml.contains(g)) continue;
        const GroupInfo info = groupInfo(g);
        const bool open = g != GroupRoutes && g != GroupRematches;
        html += "<details class=\"section\" data-cat=\"" + esc(info.title) + "\"" + (open ? " open" : "") + ">";
        html += "<summary>" + esc(info.title) + "<span class=\"count\">" + QString::number(sectionCount.value(g)) +
                " Trainer</span></summary><div class=\"section-body\">" + sectionHtml.value(g) + "</div></details>";
    }
    html += spoiler::foot();
    return spoiler::write(path, html);
}

} // namespace bdsp
