#include "headers/swsh/swsh_trainers.h"
#include "headers/common/trainer_smart.h"

#include <QFile>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSet>
#include <QTextStream>

namespace swsh {

namespace {

// ------------------------------------------------------------ Trainerklassen
// Klassen-IDs aus trainer_data (u16 an 0x00). Die Zuordnung ist gegen den
// Dump (Version 1.3.2) geprueft.

const QSet<int> kRivalClasses = {7, 8, 184, 188, 199, 11, 13, 14};
const QSet<int> kGymLeaderClasses = {12, 15, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 185,
                                     205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218};
const QSet<int> kVillainClasses = {33, 34, 178, 202, 165, 201, 203, 204};
const QSet<int> kImportantClasses = {4, 5, 6, 30, 32, 74, 75, 186, 187, 183, 151};
const QSet<int> kIsleClasses = {219, 220, 221, 222, 223, 225, 227, 250, 251, 252, 253};
const QSet<int> kTundraClasses = {228, 229};

bool isGymTrainerClass(int c) { return (c >= 76 && c <= 93) || c == 179; }
bool isChallengerClass(int c) { return c >= 118 && c <= 147; }
bool isStarTournamentClass(int c) { return c >= 230 && c <= 249; }

// Typ-IDs: 0 Normal, 1 Kampf, 2 Flug, 3 Gift, 4 Boden, 5 Gestein, 6 Kaefer, 7 Geist, 8 Stahl,
//          9 Feuer, 10 Wasser, 11 Pflanze, 12 Elektro, 13 Psycho, 14 Eis, 15 Drache, 16 Unlicht, 17 Fee
const QHash<int, int>& themeByClass() {
    static const QHash<int, int> map = [] {
        QHash<int, int> m;
        auto add = [&m](int type, std::initializer_list<int> classes) {
            for (int c : classes) {
                m.insert(c, type);
            }
        };
        add(11, {20, 209, 248, 76, 77, 138});            // Yarro, Arenatrainer, Challenger
        add(10, {21, 205, 210, 235, 78, 124});           // Kate
        add(9,  {22, 211, 234, 79, 80, 146});            // Kabu
        add(1,  {23, 208, 212, 239, 81, 82, 128});       // Saida (Schwert)
        add(7,  {24, 207, 213, 237, 83, 84, 132});       // Nio (Schild)
        add(17, {25, 214, 249, 85, 12, 231});            // Papella, Betys als Arenaleiter
        add(5,  {26, 215, 243, 86, 87, 126});            // Mac (Schwert)
        add(14, {27, 216, 242, 88, 89, 134});            // Mel (Schild)
        add(16, {28, 217, 236, 185, 179, 15, 232});      // Nezz, Mary als Arenaleiterin
        add(15, {29, 206, 218, 238, 92, 93, 142});       // Roy
        add(13, {11, 220, 251, 253, 244});               // Betys (Rivale), Saverio
        add(3,  {219, 250, 252, 245, 130});              // Sophora
        add(0,  {118});
        add(6,  {120});
        add(2,  {122});
        add(4,  {136});
        add(12, {140});
        add(8,  {144});
        return m;
    }();
    return map;
}

// Kaempfe mit Partner: hoechstens 3 Pokemon pro Trainer
const QSet<int> kMultiBattleTrainers = {156, 157, 158, 197, 198, 199, 225, 226, 227, 312, 313, 314, 223, 224};

} // namespace

// ----------------------------------------------------------- Pokemon-Regeln

// Legendaere, Mysterioese und Ultrabestien bis Nr. 898
const QSet<int>& legendarySet() {
    static const QSet<int> kLegendary = {
    144, 145, 146, 150, 151, 243, 244, 245, 249, 250, 251, 377, 378, 379, 380, 381, 382, 383, 384, 385, 386,
    480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 494, 638, 639, 640, 641, 642, 643,
    644, 645, 646, 647, 648, 649, 716, 717, 718, 719, 720, 721, 772, 773, 785, 786, 787, 788, 789, 790, 791,
    792, 793, 794, 795, 796, 797, 798, 799, 800, 801, 802, 803, 804, 805, 806, 807, 808, 809, 888, 889, 890,
    891, 892, 893, 894, 895, 896, 897, 898};
    return kLegendary;
}

bool isLegendary(int species) { return legendarySet().contains(species); }

// Formen, die nur im Kampf oder nur mit bestimmtem Item existieren
bool isExcludedForm(int species, int form) {
    if (form == 0) {
        return false;
    }
    switch (species) {
    case 25:   // Pikachu mit Kappe
    case 421:  // Kinoso Sonnenform
    case 487:  // Giratina Urform (braucht Item)
    case 649:  // Genesect mit Modul
    case 681:  // Durengard Klingenform
    case 716:  // Xerneas aktiv
    case 746:  // Lusardin Schwarmform
    case 773:  // Amigento (braucht Disc)
    case 778:  // Mimigma entlarvt
    case 845:  // Urgl mit Beute
    case 875:  // Kubuin ohne Eis
    case 877:  // Morpeko Kohldampf
    case 888:  // Zacian Koenig des Schwertes (braucht Item)
    case 889:  // Zamazenta Koenig des Schildes
    case 890:  // Unendynamax
        return true;
    case 555:  // Flampivian Trance-Modus
        return form == 1 || form == 3;
    case 718:  // Zygarde: Komplettform und Schwarmzellen-Varianten
        return form >= 2;
    default:
        return false;
    }
}

bool canGigantamax(int species, int form) {
    if (!kGigantamaxSpecies.contains(species)) {
        return false;
    }
    if (species == 25 || species == 52 || species == 133) {
        return form == 0; // nur die Grundform
    }
    return true;
}

namespace {

// Items, die nur fuer bestimmte Arten gedacht sind
const QSet<int> kSpeciesItems = {236, 259, 258, 256, 257, 274, 225, 226, 227, 1103, 1104, 135, 136, 112};

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
        if (!legendaries && isLegendary(species)) {
            continue;
        }
        const int baseIndex = files.personal.indexOf(species, 0);
        const int forms = qMax(1, files.personal.formCount(baseIndex));
        for (int form = 0; form < forms; form++) {
            const int index = files.personal.indexOf(species, form);
            if (index <= 0 || !files.personal.isPresent(index) || isExcludedForm(species, form)) {
                continue;
            }
            Candidate c{species, form};
            pools.all.append(c);
            const int t1 = files.personal.type1(index);
            const int t2 = files.personal.type2(index);
            pools.byType[t1].append(c);
            if (t2 != t1) {
                pools.byType[t2].append(c);
            }
        }
    }
    return pools;
}

} // namespace

// ------------------------------------------------------------- Einstellungen

QJsonObject optionsToJson(const TrainerOptions& o) {
    QJsonObject j;
    j["teamSize"] = o.teamSize;
    j["fullyEvolvedLevel"] = o.fullyEvolvedLevel;
    j["levelAppropriateEvos"] = o.levelAppropriateEvos;
    j["smartMovesets"] = o.smartMovesets;
    j["smartTMs"] = o.smartTMs;
    j["keepGigantamax"] = o.keepGigantamax;
    j["legendaries"] = o.legendaries;
    j["smartAI"] = o.smartAI;
    j["perfectIVs"] = o.perfectIVs;
    j["shinies"] = o.shinies;
    return j;
}

TrainerOptions optionsFromJson(const QJsonObject& j, const TrainerOptions& f) {
    TrainerOptions o;
    o.teamSize = qBound(0, j["teamSize"].toInt(f.teamSize), 2);
    o.fullyEvolvedLevel = qBound(0, j["fullyEvolvedLevel"].toInt(f.fullyEvolvedLevel), 100);
    o.levelAppropriateEvos = j["levelAppropriateEvos"].toBool(f.levelAppropriateEvos);
    o.smartMovesets = j["smartMovesets"].toBool(f.smartMovesets);
    o.smartTMs = j["smartTMs"].toBool(f.smartTMs);
    o.keepGigantamax = j["keepGigantamax"].toBool(f.keepGigantamax);
    o.legendaries = j["legendaries"].toBool(f.legendaries);
    o.smartAI = j["smartAI"].toBool(f.smartAI);
    o.perfectIVs = j["perfectIVs"].toBool(f.perfectIVs);
    o.shinies = j["shinies"].toBool(f.shinies);
    return o;
}

GroupInfo groupInfo(int group) {
    switch (group) {
    case GroupRivals:
        return {"rivals", "Rivalen", "Hop, Betys und Mary in allen Kämpfen als Rivalen."};
    case GroupGymLeaders:
        return {"gymLeaders", "Arenaleiter",
                "Yarro, Kate, Kabu, Saida/Nio, Papella, Mac/Mel, Nezz, Roy sowie Betys und Mary als "
                "Arenaleiter – inklusive Revanchen und Champ-Cup."};
    case GroupGymTrainers:
        return {"gymTrainers", "Arenatrainer", "Die Trainer in den Arenen vor dem Arenaleiter."};
    case GroupImportant:
        return {"important", "Champ und wichtige Kämpfe",
                "Delion, Rose, Olivia, Schwerthold und Schildrich, Morimoto, Ballduin und die "
                "Arena-Challenger im Champ-Cup nach dem Abspann."};
    case GroupVillains:
        return {"villains", "Team Yell und Macro Cosmos", "Die Rüpel von Team Yell und die Angestellten von Macro Cosmos."};
    case GroupRoutes:
        return {"routes", "Routentrainer", "Alle normalen Trainer auf Routen, in Städten und in der Naturzone."};
    case GroupIsle:
        return {"isle", "Insel der Rüstung",
                "Mastrich, Enia, Sophora, Saverio und die Schüler des Meister-Dojos (Erweiterungspass)."};
    case GroupTundra:
        return {"tundra", "Krone-Tundra", "Peony und Mila (Erweiterungspass)."};
    case GroupStarTournament:
        return {"starTournament", "Galar-Star-Turnier",
                "Die Doppelkämpfe im Galar-Star-Turnier nach dem Erweiterungspass."};
    default:
        return {"other", "Sonstige", QString()};
    }
}

const TrainerOptions* TrainerSettings::optionsFor(int group) const {
    if (group < 0 || group >= GroupCount) {
        return &base;
    }
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
    j["base"] = optionsToJson(s.base);
    QJsonObject groups;
    for (int g = 0; g < GroupCount; g++) {
        QJsonObject group;
        group["mode"] = s.modes[g];
        group["own"] = optionsToJson(s.own[g]);
        groups[groupInfo(g).id] = group;
    }
    j["groups"] = groups;
    return j;
}

void settingsFromJson(const QJsonObject& j, TrainerSettings& s) {
    s.enabled = j["enabled"].toBool(s.enabled);
    s.keepTypeTheme = j["keepTypeTheme"].toBool(s.keepTypeTheme);
    s.base = optionsFromJson(j["base"].toObject(), s.base);
    const QJsonObject groups = j["groups"].toObject();
    for (int g = 0; g < GroupCount; g++) {
        const QJsonObject group = groups[groupInfo(g).id].toObject();
        s.modes[g] = qBound(0, group["mode"].toInt(s.modes[g]), 2);
        s.own[g] = optionsFromJson(group["own"].toObject(), s.own[g]);
    }
}

// ------------------------------------------------------------- Einordnung

int trainerGroup(const Trainer& trainer) {
    const int c = trainer.trainerClass();
    if (kRivalClasses.contains(c)) return GroupRivals;
    if (kGymLeaderClasses.contains(c)) return GroupGymLeaders;
    if (isGymTrainerClass(c)) return GroupGymTrainers;
    if (kImportantClasses.contains(c) || isChallengerClass(c)) return GroupImportant;
    if (kVillainClasses.contains(c)) return GroupVillains;
    if (kIsleClasses.contains(c)) return GroupIsle;
    if (kTundraClasses.contains(c)) return GroupTundra;
    if (isStarTournamentClass(c)) return GroupStarTournament;
    return GroupRoutes;
}

int trainerTheme(const Trainer& trainer) {
    return themeByClass().value(trainer.trainerClass(), -1);
}

// ------------------------------------------------------------- Spieltexte

void GameTexts::load(const QString& romfs) {
    pokemon = readMessage(romfs, "monsname");
    moves = readMessage(romfs, "wazaname");
    items = readMessage(romfs, "itemname");
    types = readMessage(romfs, "typename");
    trainerNames = readMessage(romfs, "trname");
    trainerClasses = readMessage(romfs, "trtype");
}

namespace {
QString formSuffix(int species, int form) {
    static const QSet<int> alola = {19, 20, 26, 27, 28, 37, 38, 50, 51, 53, 74, 75, 76, 88, 89, 103, 105};
    static const QSet<int> galar = {77, 78, 79, 83, 110, 122, 144, 145, 146, 199, 222, 263, 264, 554, 562, 618};
    if (form == 0) return QString();
    if (form == 1 && alola.contains(species)) return "Alola-Form";
    if (form == 1 && galar.contains(species)) return "Galar-Form";
    if (species == 52) return form == 1 ? "Alola-Form" : "Galar-Form";
    if (species == 80 && form == 2) return "Galar-Form";
    if (species == 555 && form == 2) return "Galar-Form";
    if (species == 849) return "Tief-Form";
    if (species == 892) return "Fließender Stil";
    if (species == 479) {
        static const QStringList rotom = {"", "Hitze-Rotom", "Wasch-Rotom", "Frost-Rotom", "Wirbel-Rotom", "Schneid-Rotom"};
        return form < rotom.size() ? rotom[form] : QString();
    }
    if (species == 678 || species == 876) return "weiblich";
    if (species == 898) return form == 1 ? "Schimmelreiter" : "Rappenreiter";
    return QString("Form %1").arg(form);
}

QString lineOr(const QStringList& list, int index, const QString& fallback) {
    if (index >= 0 && index < list.size() && !list[index].isEmpty()) {
        return list[index];
    }
    return fallback;
}
} // namespace

QString GameTexts::pokemonName(int species, int form) const {
    QString name = lineOr(pokemon, species, QString("#%1").arg(species));
    QString suffix = formSuffix(species, form);
    return suffix.isEmpty() ? name : name + " (" + suffix + ")";
}

QString GameTexts::moveName(int id) const { return lineOr(moves, id, QString("Attacke %1").arg(id)); }
QString GameTexts::itemName(int id) const { return lineOr(items, id, QString("Item %1").arg(id)); }
QString GameTexts::typeName(int id) const {
    static const QStringList fallback = {"Normal", "Kampf", "Flug", "Gift", "Boden", "Gestein", "Käfer", "Geist", "Stahl",
                                         "Feuer", "Wasser", "Pflanze", "Elektro", "Psycho", "Eis", "Drache", "Unlicht", "Fee"};
    return lineOr(types, id, lineOr(fallback, id, "?"));
}

QString GameTexts::trainerLabel(const Trainer& trainer) const {
    QString cls = lineOr(trainerClasses, trainer.trainerClass(), QString());
    QString name = lineOr(trainerNames, trainer.index, QString());
    if (cls.startsWith("MSG_")) cls.clear();
    if (name == "-") name.clear();
    QString label = (cls + " " + name).trimmed();
    return label.isEmpty() ? QString("Trainer %1").arg(trainer.index) : label;
}

// ------------------------------------------------------------- Randomizer

TrainerRandomizerResult randomizeTrainers(GameFiles& files, const TrainerSettings& settings, quint64 seed) {
    TrainerRandomizerResult result;
    if (!settings.enabled) {
        return result;
    }

    TrainerSmartData smart;
    smart.build(files.toGameData());

    Pools pools[2] = {buildPools(files, false), buildPools(files, true)};
    QRandomGenerator master(seed);

    for (Trainer& trainer : files.trainers.all()) {
        // Jeder Trainer bekommt seinen eigenen Seed -> Ergebnis unabhaengig von anderen Einstellungen
        QRandomGenerator rng(master.generate64());

        if (trainer.isPlaceholder() || trainer.team.isEmpty()) {
            continue;
        }
        const TrainerOptions* opt = settings.optionsFor(trainerGroup(trainer));
        if (opt == nullptr) {
            continue;
        }
        const Pools& pool = pools[opt->legendaries ? 1 : 0];

        int theme = settings.keepTypeTheme ? trainerTheme(trainer) : -1;
        if (theme >= 0 && pool.byType.value(theme).isEmpty()) {
            theme = -1;
        }

        // --- Teamgroesse ---
        const int original = trainer.team.size();
        int maxSize = 6;
        if (trainer.battleMode() == 1 || kMultiBattleTrainers.contains(trainer.index)) {
            maxSize = qMax(3, original);
        }
        int target = original;
        if (opt->teamSize == 1 && original < maxSize) {
            target = static_cast<int>(rng.bounded(original, maxSize + 1));
        } else if (opt->teamSize == 2) {
            target = maxSize;
        }
        int teamMaxLevel = 1;
        for (const TrainerPoke& p : trainer.team) {
            teamMaxLevel = qMax(teamMaxLevel, p.level());
        }
        while (trainer.team.size() < target) {
            TrainerPoke extra = trainer.team.last();
            extra.setLevel(teamMaxLevel);
            extra.setCanGigantamax(false);
            extra.setDynamaxAllowed(false);
            extra.setShiny(false);
            extra.setHeldItem(0);
            trainer.team.append(extra);
        }

        // --- Pokemon waehlen ---
        QSet<int> usedSpecies;
        const bool levelRules = opt->fullyEvolvedLevel > 0 || opt->levelAppropriateEvos;

        for (TrainerPoke& poke : trainer.team) {
            const int level = poke.level();
            const bool wantsGmax = poke.canGigantamax() && opt->keepGigantamax;

            auto pickFrom = [&](const QList<Candidate>& list, bool applyLevel, bool noDuplicates, bool gmaxOnly,
                                Candidate& out) -> bool {
                QHash<int, QList<int>> bySpecies;
                QList<int> order;
                for (const Candidate& c : list) {
                    if (noDuplicates && usedSpecies.contains(c.species)) continue;
                    if (gmaxOnly && !canGigantamax(c.species, c.form)) continue;
                    if (applyLevel && !smart.isAllowedAtLevel(c.species, c.form, level, opt->fullyEvolvedLevel,
                                                              opt->levelAppropriateEvos)) continue;
                    if (!bySpecies.contains(c.species)) order.append(c.species);
                    bySpecies[c.species].append(c.form);
                }
                if (order.isEmpty()) {
                    return false;
                }
                const int species = order[static_cast<int>(rng.bounded(static_cast<int>(order.size())))];
                const QList<int>& forms = bySpecies[species];
                out = {species, forms[static_cast<int>(rng.bounded(static_cast<int>(forms.size())))]};
                return true;
            };
            // 1. alle Regeln, 2. ohne Level-Regeln, 3. Duplikate erlauben (letzter Ausweg)
            auto pickWithFallback = [&](const QList<Candidate>& list, bool gmaxOnly, Candidate& out) -> bool {
                if (pickFrom(list, levelRules, true, gmaxOnly, out)) return true;
                if (levelRules && pickFrom(list, false, true, gmaxOnly, out)) return true;
                if (pickFrom(list, levelRules, false, gmaxOnly, out)) return true;
                return levelRules && pickFrom(list, false, false, gmaxOnly, out);
            };

            const QList<Candidate>& list = theme >= 0 ? pool.byType[theme] : pool.all;
            Candidate pick;
            bool gmax = false;
            // Gigadynamax-Ass: nur wenn alle Regeln (Typ, Level, keine Duplikate) erfuellbar sind
            if (wantsGmax && pickFrom(list, levelRules, true, true, pick)) {
                gmax = true;
            } else if (!pickWithFallback(list, false, pick)) {
                continue; // sollte nie passieren
            }
            usedSpecies.insert(pick.species);

            const int personalIndex = files.personal.indexOf(pick.species, pick.form);
            poke.setSpecies(pick.species);
            poke.setForm(pick.form);

            // Geschlecht: zufaellig, ausser bei Geschlechtsformen
            if (pick.species == 678 || pick.species == 876) {
                poke.setGender(pick.form == 1 ? 2 : 1);
            } else {
                poke.setGender(0);
            }

            if (kSpeciesItems.contains(poke.heldItem())) {
                poke.setHeldItem(0);
            }

            // Dynamax / Gigadynamax
            if (!files.personal.canDynamax(personalIndex)) {
                poke.setDynamaxAllowed(false);
                gmax = false;
            }
            poke.setCanGigantamax(gmax || (poke.canGigantamax() && canGigantamax(pick.species, pick.form)
                                           && files.personal.canDynamax(personalIndex)));

            // Attacken
            QList<int> moveset;
            if (opt->smartMovesets) {
                moveset = smart.buildMovesetIds(pick.species, pick.form, level, opt->smartTMs, rng);
            }
            if (moveset.isEmpty()) {
                moveset = smart.defaultMoveset(pick.species, pick.form, level);
            }
            for (int slot = 0; slot < 4; slot++) {
                poke.setMove(slot, slot < moveset.size() ? moveset[slot] : 0);
            }

            if (opt->perfectIVs) {
                poke.setPerfectIVs();
            }
            if (opt->shinies && !poke.shiny()) {
                poke.setShiny(rng.bounded(100) < 50);
            }
        }

        if (opt->smartAI) {
            trainer.setAi(trainer.ai() | 0x47); // Basis, Stark, Experte, Wechseln
        }

        result.randomized++;
        result.changedIndexes.append(trainer.index);
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
        if (!settings.enabled || trainer.isPlaceholder() || trainer.team.isEmpty()) {
            continue;
        }
        const int group = trainerGroup(trainer);
        const bool original = settings.optionsFor(group) == nullptr || !settings.enabled;
        const int theme = settings.keepTypeTheme ? trainerTheme(trainer) : -1;
        const QString label = texts.trainerLabel(trainer);

        QString cards;
        QString search = label;
        int highest = 0;
        for (const TrainerPoke& poke : trainer.team) {
            const int species = poke.species();
            const int form = poke.form();
            const int index = files.personal.indexOf(species, form);
            const int t1 = files.personal.type1(index);
            const int t2 = files.personal.type2(index);
            highest = qMax(highest, poke.level());

            const QString name = texts.pokemonName(species, form);
            QStringList moves;
            for (int slot = 0; slot < 4; slot++) {
                if (poke.move(slot) > 0) {
                    moves << texts.moveName(poke.move(slot));
                }
            }
            const QString item = poke.heldItem() > 0 ? texts.itemName(poke.heldItem()) : QString();
            search += " " + name + " " + moves.join(" ") + " " + item;

            QString card = "<div class=\"mon\"><div class=\"mon-head\"><span class=\"mon-name\">" + esc(name) + "</span>";
            if (poke.shiny()) card += "<span class=\"shiny\" title=\"Schillernd\">✦</span>";
            card += "<span class=\"lvl\">Lv. " + QString::number(poke.level()) + "</span></div>";
            card += "<div class=\"types\"><span class=\"type t" + QString::number(t1) + "\">" + esc(texts.typeName(t1)) + "</span>";
            if (t2 != t1) {
                card += "<span class=\"type t" + QString::number(t2) + "\">" + esc(texts.typeName(t2)) + "</span>";
            }
            if (poke.canGigantamax()) card += "<span class=\"gmax\">Gigadynamax</span>";
            card += "</div>";
            if (!item.isEmpty()) card += "<div class=\"meta\">Item: " + esc(item) + "</div>";
            card += "<ul class=\"moves\">";
            for (const QString& m : moves) card += "<li>" + esc(m) + "</li>";
            card += "</ul></div>";
            cards += card;
        }

        QStringList sub;
        sub << (trainer.battleMode() == 1 ? "Doppelkampf" : "Einzelkampf");
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

    const QString gameTitle = version == Version::Unknown ? QString("Pokémon Schwert/Schild") : versionName(version);
    QString html = R"(<!DOCTYPE html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Spoiler-Log · Schwert/Schild Randomizer</title>
<style>
:root{--bg:#0f1117;--panel:#171a23;--card:#1e2230;--line:#2a2f40;--text:#e8eaf0;--muted:#8b91a5;--accent:#8b5cf6}
*{box-sizing:border-box}body{margin:0;font-family:"Segoe UI",system-ui,sans-serif;background:var(--bg);color:var(--text)}
.top{position:sticky;top:0;z-index:5;background:rgba(15,17,23,.95);backdrop-filter:blur(6px);border-bottom:1px solid var(--line);padding:16px 24px}
.top h1{margin:0 0 4px;font-size:22px}.top .info{color:var(--muted);font-size:13px;margin-bottom:12px}
.controls{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
#search{flex:1;min-width:220px;padding:10px 14px;border-radius:10px;border:1px solid var(--line);background:var(--panel);color:var(--text);font-size:15px}
#search:focus{outline:2px solid var(--accent);border-color:transparent}
.chip{padding:7px 12px;border-radius:999px;border:1px solid var(--line);background:var(--panel);color:var(--text);cursor:pointer;font-size:13px}
.chip.active{background:var(--accent);border-color:var(--accent)}
main{padding:20px 24px;max-width:1500px;margin:0 auto}
details.section{margin-bottom:18px;background:var(--panel);border:1px solid var(--line);border-radius:14px}
details.section>summary{cursor:pointer;padding:14px 18px;font-size:18px;font-weight:600;list-style:none}
details.section>summary::-webkit-details-marker{display:none}
details.section>summary .count{color:var(--muted);font-weight:400;font-size:14px;margin-left:8px}
.section-body{padding:0 18px 18px}
.trainer{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-top:14px}
.trainer h3{margin:0;font-size:16px;display:flex;gap:8px;align-items:center}.trainer .sub{color:var(--muted);font-size:12px;margin-top:3px}
.team{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:10px;margin-top:12px}
.mon{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:10px}
.mon-head{display:flex;align-items:baseline;gap:6px}.mon-name{font-weight:600;flex:1}
.lvl{color:var(--muted);font-size:13px}.shiny{color:#facc15}
.types{margin:6px 0;display:flex;gap:4px;flex-wrap:wrap}
.type{display:inline-block;padding:2px 8px;border-radius:6px;font-size:11px;font-weight:600;color:#fff;text-shadow:0 1px 1px rgba(0,0,0,.4)}
.gmax{display:inline-block;padding:2px 8px;border-radius:6px;font-size:11px;font-weight:600;color:#fff;background:linear-gradient(90deg,#e11d48,#9333ea)}
.meta{font-size:12px;margin-top:4px;color:#cfd3df}
.moves{margin:8px 0 0;padding-left:16px;font-size:13px}.moves li{margin:2px 0}
.hidden{display:none!important}
.t0{background:#9fa19f}.t1{background:#ff8000}.t2{background:#81b9ef}.t3{background:#9141cb}.t4{background:#915121}
.t5{background:#afa981}.t6{background:#91a119}.t7{background:#704170}.t8{background:#60a1b8}.t9{background:#e62829}
.t10{background:#2980ef}.t11{background:#3fa129}.t12{background:#d4b000}.t13{background:#ef4179}.t14{background:#3dcef3}
.t15{background:#5060e1}.t16{background:#624d4e}.t17{background:#ef70ef}
#empty{color:var(--muted);text-align:center;padding:40px}
.section-body>.team{margin-top:4px}
table.enc{width:100%;border-collapse:collapse;font-size:14px}
table.enc th{text-align:left;color:var(--muted);font-weight:600;font-size:12px;padding:6px 8px;border-bottom:1px solid var(--line)}
table.enc td{padding:6px 8px;border-bottom:1px solid var(--line);vertical-align:middle}
table.enc td .type{margin-left:4px}
table.enc .num{color:var(--muted);white-space:nowrap}.arrow{color:var(--muted)}
.hint{color:var(--muted);font-size:12px}
</style></head><body>
<div class="top"><h1>Spoiler-Log · )" + esc(gameTitle) + "</h1>";
    html += "<div class=\"info\">Erstellt am " + QDateTime::currentDateTime().toString("dd.MM.yyyy 'um' HH:mm") +
            (settings.enabled ? " · " + QString::number(total) + " Trainer" : QString()) + " · Seed " + esc(seedText) + "</div>";
    html += R"(<div class="controls"><input id="search" type="search" placeholder="Suchen: Trainer, Pokémon, Attacke, Item …" autocomplete="off">
<button class="chip" id="expand">Alle aufklappen</button><button class="chip" id="collapse">Alle zuklappen</button></div>
<div class="controls" id="chips" style="margin-top:10px"></div></div><main>)";

    for (const SpoilerSection& section : extra) {
        html += "<details class=\"section\" data-cat=\"" + esc(section.title) + "\" open>";
        html += "<summary>" + esc(section.title) + "<span class=\"count\">" + QString::number(section.count) +
                " Einträge</span></summary><div class=\"section-body\">" + section.html + "</div></details>";
    }

    for (int g = 0; g < GroupCount; g++) {
        if (!sectionHtml.contains(g)) continue;
        const GroupInfo info = groupInfo(g);
        const bool open = g != GroupRoutes;
        html += "<details class=\"section\" data-cat=\"" + esc(info.title) + "\"" + (open ? " open" : "") + ">";
        html += "<summary>" + esc(info.title) + "<span class=\"count\">" + QString::number(sectionCount.value(g)) +
                " Trainer</span></summary><div class=\"section-body\">" + sectionHtml.value(g) + "</div></details>";
    }

    html += R"(<div id="empty" class="hidden">Keine Treffer.</div></main>
<script>
(function(){
  var sections=[].slice.call(document.querySelectorAll('details.section'));
  var search=document.getElementById('search');var chips=document.getElementById('chips');var activeCats=new Set();
  sections.forEach(function(s){
    var b=document.createElement('button');b.className='chip';b.textContent=s.dataset.cat;
    b.onclick=function(){if(activeCats.has(s.dataset.cat)){activeCats.delete(s.dataset.cat);b.classList.remove('active');}else{activeCats.add(s.dataset.cat);b.classList.add('active');}apply();};
    chips.appendChild(b);
  });
  function apply(){
    var q=search.value.trim().toLowerCase();var terms=q?q.split(/\s+/):[];var any=false;
    sections.forEach(function(s){
      var catOk=activeCats.size===0||activeCats.has(s.dataset.cat);var visible=0;
      s.querySelectorAll('[data-search]').forEach(function(t){
        var ok=catOk&&terms.every(function(w){return t.dataset.search.indexOf(w)!==-1;});
        t.classList.toggle('hidden',!ok);if(ok)visible++;
      });
      s.classList.toggle('hidden',visible===0);if(visible>0)any=true;if(terms.length&&visible>0)s.open=true;
    });
    document.getElementById('empty').classList.toggle('hidden',any);
  }
  search.addEventListener('input',apply);
  document.getElementById('expand').onclick=function(){sections.forEach(function(s){s.open=true;});};
  document.getElementById('collapse').onclick=function(){sections.forEach(function(s){s.open=false;});};
})();
</script></body></html>)";

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << html;
    return true;
}

} // namespace swsh
