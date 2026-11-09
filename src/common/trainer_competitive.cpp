// ---------------------------------------------------------------------------
// Competitive-Modus fuer Trainer (Schwert/Schild, Strahlender Diamant/Leuchtende
// Perle, Karmesin/Purpur): Attacken, Item, Faehigkeit, Wesen und Fleisspunkte
// wie bei einem Competitive-Team.
//
//  - Attacken ohne Level-Grenze: alle Level-Up-Attacken (inkl. Vorentwicklungen)
//    und alle TM-Attacken. Typ-Bonus, Typ-Abdeckung, Setup-, Heil- und
//    Statusattacken je nach Rolle; in Doppelkaempfen Schutzschild, Mogelhieb
//    und Rueckenwind.
//  - Item passend zum Set; jedes Item nur einmal pro Team.
//  - Beste der drei Faehigkeiten (inkl. versteckter Faehigkeit).
//  - Wesen und Fleisspunkte passend zur Rolle (252/252/4).
// ---------------------------------------------------------------------------

#include "headers/common/trainer_smart.h"

#include <algorithm>

namespace {

// Typen in Spielreihenfolge
enum Type {
    Normal = 0, Fighting, Flying, Poison, Ground, Rock, Bug, Ghost, Steel,
    Fire, Water, Grass, Electric, Psychic, Ice, Dragon, Dark, Fairy, TypeCount
};

// Typ-Tabelle: Effektivitaet Angriffstyp -> Verteidigungstyp
struct TypeChart {
    double m[TypeCount][TypeCount];
    TypeChart() {
        for (int a = 0; a < TypeCount; a++)
            for (int d = 0; d < TypeCount; d++) m[a][d] = 1.0;
        auto se = [this](int a, std::initializer_list<int> d) { for (int t : d) m[a][t] = 2.0; };
        auto nve = [this](int a, std::initializer_list<int> d) { for (int t : d) m[a][t] = 0.5; };
        auto imm = [this](int a, std::initializer_list<int> d) { for (int t : d) m[a][t] = 0.0; };
        nve(Normal, {Rock, Steel}); imm(Normal, {Ghost});
        se(Fighting, {Normal, Rock, Steel, Ice, Dark}); nve(Fighting, {Flying, Poison, Bug, Psychic, Fairy}); imm(Fighting, {Ghost});
        se(Flying, {Fighting, Bug, Grass}); nve(Flying, {Rock, Steel, Electric});
        se(Poison, {Grass, Fairy}); nve(Poison, {Poison, Ground, Rock, Ghost}); imm(Poison, {Steel});
        se(Ground, {Poison, Rock, Steel, Fire, Electric}); nve(Ground, {Bug, Grass}); imm(Ground, {Flying});
        se(Rock, {Flying, Bug, Fire, Ice}); nve(Rock, {Fighting, Ground, Steel});
        se(Bug, {Grass, Psychic, Dark}); nve(Bug, {Fighting, Flying, Poison, Ghost, Steel, Fire, Fairy});
        se(Ghost, {Ghost, Psychic}); nve(Ghost, {Dark}); imm(Ghost, {Normal});
        se(Steel, {Rock, Ice, Fairy}); nve(Steel, {Steel, Fire, Water, Electric});
        se(Fire, {Bug, Steel, Grass, Ice}); nve(Fire, {Rock, Fire, Water, Dragon});
        se(Water, {Ground, Rock, Fire}); nve(Water, {Water, Grass, Dragon});
        se(Grass, {Ground, Rock, Water}); nve(Grass, {Flying, Poison, Bug, Steel, Fire, Grass, Dragon});
        se(Electric, {Flying, Water}); nve(Electric, {Grass, Electric, Dragon}); imm(Electric, {Ground});
        se(Psychic, {Fighting, Poison}); nve(Psychic, {Steel, Psychic}); imm(Psychic, {Dark});
        se(Ice, {Flying, Ground, Grass, Dragon}); nve(Ice, {Steel, Fire, Water, Ice});
        se(Dragon, {Dragon}); nve(Dragon, {Steel}); imm(Dragon, {Fairy});
        se(Dark, {Ghost, Psychic}); nve(Dark, {Fighting, Dark, Fairy});
        se(Fairy, {Fighting, Dragon, Dark}); nve(Fairy, {Poison, Steel, Fire});
    }
    double eff(int a, int d) const {
        if (a < 0 || a >= TypeCount || d < 0 || d >= TypeCount) return 1.0;
        return m[a][d];
    }
};
const TypeChart& typeChart() {
    static const TypeChart chart; // thread-sicher initialisiert
    return chart;
}

// --- Statusattacken nach Rolle ---
const QList<int> kSetupPhysical = {14, 349, 339, 468, 489, 508, 837, 882}; // Schwerttanz, Drachentanz, Protzer, Klauenwetzer, Einrollen, Gangwechsel, Siegestanz, Aufraeumen
const QList<int> kSetupSpecial = {417, 347, 483};                          // Raenkeschmied, Gedankengut, Falterreigen
const QList<int> kSetupBoth = {504, 74};                                   // Hausbruch, Wachstum
const QList<int> kSpeedSetup = {97, 397};                                  // Agilitaet, Steinpolitur
const QList<int> kRecovery = {105, 355, 208, 135, 234, 235, 236, 303, 456, 659}; // Genesung, Ruheort, Milchgetraenk, Weichei, Morgengrauen, Synthese, Mondschein, Tagedieb, Heilbefehl, Sandsammler
const QList<int> kStatusInflict = {92, 261, 86, 79, 147};                  // Toxin, Irrlicht, Donnerwelle, Schlafpuder, Pilzspore
const QList<int> kHazards = {446, 191, 390};                               // Tarnsteine, Stachler, Giftspitzen
constexpr int kProtect = 182;  // Schutzschild
constexpr int kFakeOut = 252;  // Mogelhieb
constexpr int kTailwind = 366; // Rueckenwind
constexpr int kTeraBlast = 851; // Tera-Ausbruch

// --- Faehigkeiten: Bewertung fuer Trainer-Kaempfe (hoeher = besser, unbekannt = 0) ---
const QHash<int, int> kAbilityScore = {
    // sehr stark
    {37, 10}, {74, 10}, {3, 9}, {153, 8}, {98, 8}, {136, 8}, {25, 9}, {91, 8}, {156, 7}, {168, 8}, {236, 8},
    {2, 8}, {70, 8}, {45, 7}, {117, 6}, {22, 8}, {144, 7}, {158, 6}, {283, 8}, {284, 8}, {285, 8}, {286, 8},
    {287, 8}, {288, 9}, {289, 9}, {281, 7}, {282, 7}, {224, 8}, {226, 7}, {227, 7}, {228, 6}, {229, 7},
    {182, 8}, {184, 8}, {174, 7}, {206, 7}, {181, 7}, {209, 9}, {231, 9}, {232, 7}, {233, 7}, {234, 10},
    {235, 9}, {199, 9}, {255, 8}, {264, 9}, {265, 9}, {266, 10}, {267, 10}, {272, 8}, {293, 7}, {213, 6},
    {218, 7}, {92, 7}, {110, 7}, {109, 7}, {126, 7}, {90, 7}, {125, 8}, {101, 7},
    // gut
    {26, 7}, {47, 6}, {5, 6}, {62, 6}, {172, 5}, {128, 5}, {86, 6}, {89, 5}, {173, 6}, {178, 6}, {292, 6},
    {177, 6}, {33, 4}, {34, 4}, {146, 4}, {202, 4}, {11, 5}, {10, 5}, {18, 5}, {157, 5}, {114, 5}, {31, 5},
    {78, 5}, {87, 4}, {63, 5}, {147, 4}, {240, 4}, {230, 5}, {241, 5}, {263, 6}, {278, 6}, {296, 5},
    {299, 5}, {275, 6}, {271, 6}, {297, 5}, {32, 6}, {200, 6}, {120, 5}, {245, 5}, {246, 6}, {243, 4},
    {252, 4}, {290, 4}, {111, 5}, {116, 5}, {80, 1}, {29, 4}, {9, 3}, {38, 3}, {49, 3}, {24, 3}, {160, 4},
    {97, 3}, {105, 2}, {154, 3}, {84, 2}, {162, 4}, {163, 5}, {164, 5}, {186, 6}, {187, 6}, {189, 9},
    {190, 9}, {191, 7}, {197, 5}, {201, 5}, {242, 6}, {244, 5}, {249, 2}, {262, 6}, {276, 5}, {298, 3},
    {302, 5}, {305, 7}, {307, 6},
    // schaedlich oder nutzlos fuer Trainer
    {54, -10}, {112, -10}, {129, -5}, {103, -6}, {193, -2}, {194, -2}, {155, -1}, {50, -1}, {12, -1},
    {53, -1}, {118, -1}, {124, -1}, {261, -1}
};

int abilityScore(int ability) {
    return ability > 0 ? kAbilityScore.value(ability, 0) : -100;
}

// Typ-verstaerkende Items je Typ (Seidenschal ... Feendaune)
const int kTypeBoostItem[TypeCount] = {251, 241, 244, 245, 237, 238, 222, 247, 233, 249, 243, 239, 242, 248, 246,
                                       250, 240, 2401};

// Items aus der 4. Generation (Strahlender Diamant/Leuchtende Perle)
const QSet<int> kGen4Items = {
    234, 270, 220, 297, 287, 275, 158, 157, 268, 269, 217, 253, 266, 267, 232, 213, 221, 214, 219, 155, 230, 276,
    265, 272, 273, 281,
    222, 233, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251
};

bool itemAvailable(int item, int generation) {
    if (item <= 0) return false;
    if (generation >= 9) return true;
    if (generation >= 5) return item < 1700; // bis Schwert/Schild inkl. Erweiterungspass
    return kGen4Items.contains(item);
}

// Faehigkeiten, die Normal-Attacken in einen anderen Typ verwandeln (Feenschicht, Zenithaut, Frostschicht, Elektrohaut)
int skinType(int ability) {
    switch (ability) {
    case 182: return Fairy;
    case 184: return Flying;
    case 174: return Ice;
    case 206: return Electric;
    default: return -1;
    }
}

} // namespace

void TrainerSmartData::collectCompetitivePool(qint64 k, bool withTMs, QSet<int>& pool, QSet<qint64>& visited) const {
    if (visited.contains(k)) return;
    visited.insert(k);
    qint64 lookup = pokeInfo.contains(k) ? k : key(static_cast<int>(k / 1000), 0);
    if (pokeInfo.contains(lookup)) {
        const PokeInfo& info = pokeInfo[lookup];
        for (const auto& lm : info.levelMoves) pool.insert(lm.first); // alle Level, keine Grenze
        if (withTMs) {
            for (int tm : info.tmMoves) pool.insert(tm);
        }
    }
    // Level-Attacken der Vorentwicklungen
    for (const auto& parent : parents.value(k)) {
        collectCompetitivePool(parent.first, false, pool, visited);
    }
}

QString TrainerSmartData::moveDevName(int moveId) const {
    return QString::fromStdString(moves.value(moveId).devName);
}

CompetitiveSet TrainerSmartData::buildCompetitiveSet(int natdex, int form, bool doubles, int itemGeneration,
                                                     QSet<int>& usedItems, QRandomGenerator& rng) const {
    CompetitiveSet set;
    if (!ready) return set;
    const qint64 k = key(natdex, form);
    const qint64 infoKey = pokeInfo.contains(k) ? k : key(natdex, 0);
    if (!pokeInfo.contains(infoKey)) return set;
    const PokeInfo& self = pokeInfo[infoKey];
    const TypeChart& chart = typeChart();

    // --- Faehigkeit: die beste der drei (bei Gleichstand die vordere) ---
    int abilitySlot = 0;
    for (int slot = 1; slot < 3; slot++) {
        if (abilityScore(self.abilities[slot]) > abilityScore(self.abilities[abilitySlot])) abilitySlot = slot;
    }
    const int ability = self.abilities[abilitySlot];
    const int skin = skinType(ability);

    // --- Rolle ---
    const int atk = (ability == 37 || ability == 74) ? self.atk * 2 : self.atk; // Kraftkoloss, Mentalkraft
    const bool physical = atk >= self.spa;
    const int offense = std::max(atk, self.spa);
    const double statRatio = offense > 0 ? std::min(atk, self.spa) / static_cast<double>(offense) : 1.0;
    const bool mixed = statRatio >= 0.9;
    const int bulk = self.hp + self.def + self.spd;
    const bool frail = bulk < 200;
    const bool defensive = offense < 90 && bulk >= 250;
    const bool slow = self.spe < 50;
    auto isOwnType = [&](int type) { return type == self.type1 || type == self.type2; };

    // --- Attacken-Pool: alle Level-Up- und TM-Attacken, keine Level-Grenze ---
    QSet<int> poolSet;
    QSet<qint64> visited;
    collectCompetitivePool(k, true, poolSet, visited);
    if (poolSet.isEmpty() && infoKey != k) collectCompetitivePool(infoKey, true, poolSet, visited);
    QList<int> pool(poolSet.begin(), poolSet.end());
    std::sort(pool.begin(), pool.end()); // feste Reihenfolge (Determinismus)
    auto has = [&](int id) { return poolSet.contains(id) && moves.contains(id) && moves[id].usable; };
    auto available = [&](const QList<int>& list) {
        QList<int> out;
        for (int id : list) if (has(id)) out.append(id);
        return out;
    };

    // --- Schadensattacken bewerten ---
    struct Scored { int id; int type; double score; };
    QList<Scored> damaging;
    for (int id : pool) {
        if (!moves.contains(id)) continue;
        const MoveInfo& m = moves[id];
        if (!m.damaging) continue;
        if (id == kFakeOut) continue; // nur gezielt in Doppelkaempfen
        if (id == kTeraBlast) continue; // ohne Terakristallisierung nur eine Normal-Attacke
        int type = m.type;
        double power = m.power * m.avgHits;
        if (ability == 101 && m.power <= 60) power *= 1.5;          // Techniker
        if (skin >= 0 && m.type == Normal) { type = skin; power *= 1.2; }
        double score = power * (m.accuracy / 100.0);
        if (isOwnType(type)) score *= (ability == 91 ? 2.0 : 1.5);   // Typ-Bonus (Anpassung: x2)
        if (mixed) {
            score *= (m.category == 1 ? atk : self.spa) / static_cast<double>(std::max(1, offense));
        } else if ((m.category == 1) != physical) {
            score *= 0.45; // passt nicht zum staerkeren Angriffswert
        }
        if (m.priority > 0) score *= (self.spe < 80 ? 1.15 : 0.9); // Erstschlag hilft langsamen Pokemon
        score *= 0.95 + rng.generateDouble() * 0.1;                 // etwas Abwechslung
        damaging.append({id, type, score});
    }

    // --- Statusattacken je nach Rolle ---
    QList<int> setup;
    // Setup passend zum hoeheren Angriffswert (bei gemischten Angreifern ebenso)
    setup << available(physical ? kSetupPhysical : kSetupSpecial);
    setup << available(kSetupBoth);
    // Agilitaet/Steinpolitur nur, wenn es keine echte Setup-Attacke gibt
    if (setup.isEmpty() && self.spe >= 60 && self.spe < 100) setup << available(kSpeedSetup);
    if (ability == 126) setup.clear(); // Umkehrung: Setup-Attacken wuerden die Werte senken
    const QList<int> recovery = available(kRecovery);
    QList<int> utility = available(kHazards);
    utility << available(kStatusInflict);

    auto pickRandom = [&](const QList<int>& list) {
        return list[static_cast<int>(rng.bounded(static_cast<int>(list.size())))];
    };

    QList<int> chosen;
    bool hasSetup = false;
    if (doubles) {
        if (has(kProtect)) chosen << kProtect;
        if (!defensive && has(kFakeOut)) {
            chosen << kFakeOut;
        } else if (self.spe >= 80 && has(kTailwind)) {
            chosen << kTailwind;
        }
    } else if (defensive) {
        if (!recovery.isEmpty()) chosen << pickRandom(recovery);
        if (!utility.isEmpty()) chosen << pickRandom(utility);
    } else if (!setup.isEmpty()) {
        chosen << pickRandom(setup);
        hasSetup = true;
        if (bulk >= 230 && !recovery.isEmpty() && rng.bounded(2) == 0) chosen << pickRandom(recovery);
    } else if (!frail && !utility.isEmpty() && rng.bounded(100) < 40) {
        chosen << pickRandom(utility);
    }

    // --- Angriffe mit Typ-Abdeckung waehlen ---
    double best[TypeCount];
    for (double& b : best) b = 0.0;
    QSet<int> usedTypes;
    for (int id : chosen) {
        if (moves.value(id).damaging) {
            usedTypes.insert(moves.value(id).type);
        }
    }
    int attacks = 0;
    while (chosen.size() < 4 && !damaging.isEmpty()) {
        int bestIndex = -1;
        double bestValue = -1.0;
        for (int i = 0; i < damaging.size(); i++) {
            int newSuper = 0, newNeutral = 0;
            for (int t = 0; t < TypeCount; t++) {
                const double e = chart.eff(damaging[i].type, t);
                if (e >= 2.0 && best[t] < 2.0) newSuper++;
                if (e >= 1.0 && best[t] < 1.0) newNeutral++;
            }
            double value = damaging[i].score * (usedTypes.contains(damaging[i].type) ? 0.35 : 1.0) *
                           (1.0 + 0.08 * newSuper + 0.04 * newNeutral);
            if (value > bestValue) {
                bestValue = value;
                bestIndex = i;
            }
        }
        const Scored pick = damaging.takeAt(bestIndex);
        chosen << pick.id;
        usedTypes.insert(pick.type);
        for (int t = 0; t < TypeCount; t++) best[t] = std::max(best[t], chart.eff(pick.type, t));
        attacks++;
    }
    // Zu wenige Angriffe (z. B. Pokemon mit kaum Attacken): mit nuetzlichen Statusattacken auffuellen
    QList<int> rest = recovery + utility + setup;
    for (int id : rest) {
        if (chosen.size() >= 4) break;
        if (!chosen.contains(id)) chosen << id;
    }
    if (chosen.isEmpty()) return set;
    // Nur Angriffe (ohne Mogelhieb, der nur in der ersten Runde wirkt): Wahl-Items und Offensivweste passen
    const bool allAttacks = attacks >= 4;

    // --- Item (jedes nur einmal pro Team) ---
    const bool poison = isOwnType(Poison);
    QList<int> prefs;
    if (ability == 90) prefs << 272;                              // Aufheber + Toxik-Orb
    if (ability == 62 && !isOwnType(Fire)) prefs << 273;          // Adrenalin + Heiss-Orb
    if (ability == 98 || ability == 125) prefs << 270;            // Magieschild / Rohe Gewalt + Leben-Orb
    if (ability == 281 || ability == 282) prefs << 1880;          // Paradox-Pokemon + Energiekapsel
    if (!isFullyEvolved(natdex, form)) prefs << 538;              // Evolith
    if (allAttacks) {
        if (bulk >= 250) prefs << 640;                            // Offensivweste
        if (self.spe >= 60 && self.spe <= 100) prefs << 287;      // Wahlschal
        prefs << (physical ? 220 : 297);                          // Wahlband / Wahlglas
        prefs << 270 << 268;                                      // Leben-Orb, Expertengurt
    } else if (hasSetup) {
        if (bulk >= 240) {
            prefs << 639;                                         // Schwaechenschutz
            if (poison) prefs << 281;
            prefs << 234 << 158;
        } else {
            prefs << 270 << 157;
            if (frail && ability != 5) prefs << 275;
        }
    } else if (defensive) {
        if (poison) prefs << 281;                                 // Giftschleim
        prefs << 234;                                             // Ueberreste
        if (self.def >= self.spd) prefs << 540;                   // Beulenhelm
        prefs << 1120 << 158;                                     // Plateauschuhe, Tsitrubeere
    } else {
        if (frail && ability != 5 && self.spe >= 80) prefs << 275; // Fokusgurt
        prefs << 270 << 268;
    }
    // Ausweichliste, falls alles schon im Team vergeben ist
    prefs << 158 << 157;
    if (poison) prefs << 281;
    prefs << 234;
    if (self.type1 >= 0 && self.type1 < TypeCount) prefs << kTypeBoostItem[self.type1];
    if (self.type2 >= 0 && self.type2 < TypeCount) prefs << kTypeBoostItem[self.type2];
    prefs << (physical ? 266 : 267) << 232 << 253 << 214 << 217 << 213 << 268 << 270;

    int item = 0;
    if (ability != 103) { // Tollpatsch kann keine Items nutzen
        for (int candidate : prefs) {
            if (itemAvailable(candidate, itemGeneration) && !usedItems.contains(candidate)) {
                item = candidate;
                break;
            }
        }
    }
    if (item > 0) usedItems.insert(item);
    const bool scarf = item == 287;

    // --- Wesen und Fleisspunkte ---
    // Wesen-Nummern: 3 Hart, 4 Frech, 5 Kuehn, 8 Pfiffig, 10 Scheu, 11 Hastig, 13 Froh, 14 Naiv, 15 Maessig,
    //                19 Hitzig, 20 Still, 23 Sacht
    int nature;
    int evs[6] = {0, 0, 0, 0, 0, 0}; // KP, Ang, Vert, SpAng, SpVert, Init
    if (defensive && !scarf) {
        const bool physDef = self.def >= self.spd;
        if (physDef) nature = physical ? 8 : 5;
        else nature = physical ? 23 : 20;
        evs[0] = 252;
        evs[physDef ? 2 : 4] = 252;
        evs[physDef ? 4 : 2] = 4;
    } else {
        const bool speedNature = scarf || self.spe >= 70;
        if (mixed) {
            if (physical) nature = speedNature ? 14 : 4;
            else nature = speedNature ? 11 : 19;
        } else if (physical) {
            nature = speedNature ? 13 : 3;
        } else {
            nature = speedNature ? 10 : 15;
        }
        evs[physical ? 1 : 3] = 252;
        if (slow && !scarf) {
            evs[0] = 252;
            evs[2] = 4;
        } else {
            evs[5] = 252;
            evs[0] = 4;
        }
    }

    set.valid = true;
    set.moves = chosen.mid(0, 4);
    set.item = item;
    set.abilitySlot = abilitySlot;
    set.nature = nature;
    for (int i = 0; i < 6; i++) set.evs[i] = evs[i];
    return set;
}
