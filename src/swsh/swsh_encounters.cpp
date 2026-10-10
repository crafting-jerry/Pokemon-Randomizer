#include "headers/swsh/swsh_encounters.h"
#include "headers/swsh/swsh_trainers.h"

#include <QHash>
#include <QtEndian>
#include <QRandomGenerator>
#include <QSet>
#include <functional>

namespace swsh {

namespace {

// Geschenke, die nicht veraendert werden (Story)
const QSet<int> kFixedGifts = {13, 16, 17}; // Dakuma, Polaross, Phantoross
// Starter in add_poke.bin: Chimpep, Hopplo, Memmeon
const int kStarterGifts[3] = {0, 3, 4};
// Modelle auf dem Tisch in a_0101.bin (Zone 0): Chimpep, Hopplo, Memmeon
const int kStarterCritters[3] = {3, 1, 2};

// Szenarien in event_encount_data.bin
enum Scenario {
    ScenarioNone = 0, ScenarioMotostoke = 8, ScenarioRaid1 = 9, ScenarioRaid2 = 10, ScenarioRaid3 = 11,
    ScenarioRaid4 = 12, ScenarioZacianBoss = 13, ScenarioSlowpoke = 14, ScenarioRegigigas = 15,
    ScenarioSpecialRaid = 16, ScenarioCalyrex = 17, ScenarioSteeds = 18, ScenarioCalyrexFusion = 19
};
bool isRaidScenario(int s) {
    return s == ScenarioRaid1 || s == ScenarioRaid2 || s == ScenarioRaid3 || s == ScenarioRaid4 ||
           s == ScenarioRegigigas || s == ScenarioSpecialRaid;
}

const QSet<int> kSpeciesItems = {236, 259, 258, 256, 257, 274, 225, 226, 227, 1103, 1104, 135, 136, 112};

struct Mon {
    int species = 0;
    int form = 0;
};

class Picker {
public:
    Picker(const GameFiles& f) : files(f) {
        for (int species = 1; species <= kMaxSpecies; species++) {
            const int forms = qMax(1, files.personal.formCount(files.personal.indexOf(species, 0)));
            for (int form = 0; form < forms; form++) {
                const int index = files.personal.indexOf(species, form);
                if (index > 0 && files.personal.isPresent(index) && !isExcludedForm(species, form)) {
                    all.append({species, form});
                }
            }
        }
    }

    int bst(int species, int form) const {
        const int index = files.personal.indexOf(species, form);
        int sum = 0;
        for (int s = 0; s < 6; s++) sum += files.personal.stat(index, s);
        return sum;
    }

    // Waehlt gleichmaessig eine Art (danach eine Form) aus allen Kandidaten, die filter erfuellen.
    // Bei "aehnlicher Staerke" wird der Bereich schrittweise erweitert, bis etwas passt.
    bool pick(QRandomGenerator& rng, const std::function<bool(const Mon&)>& filter, int referenceBst,
              const QSet<int>& avoid, Mon& out) const {
        for (int tolerance : {10, 20, 35, 1000}) {
            if (referenceBst <= 0 && tolerance != 1000) continue;
            QHash<int, QList<int>> bySpecies;
            QList<int> order;
            for (const Mon& m : all) {
                if (avoid.contains(m.species) || !filter(m)) continue;
                if (tolerance != 1000) {
                    const int b = bst(m.species, m.form);
                    if (qAbs(b - referenceBst) * 100 > referenceBst * tolerance) continue;
                }
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

    const GameFiles& files;
    QList<Mon> all;
};

// Basis-Pokemon (ohne Vorentwicklung), die sich zweimal entwickeln
QSet<qint64> threeStageBases(const GameFiles& files) {
    auto key = [](int s, int f) { return static_cast<qint64>(s) * 100 + f; };
    QSet<qint64> targets;
    QHash<qint64, QList<qint64>> children;
    for (int species = 1; species <= kMaxSpecies; species++) {
        const int forms = qMax(1, files.personal.formCount(files.personal.indexOf(species, 0)));
        for (int form = 0; form < forms; form++) {
            const int index = files.personal.indexOf(species, form);
            if (index <= 0) continue;
            for (const Evolution& e : files.evolutions.get(index)) {
                if (e.species <= 0) continue;
                targets.insert(key(e.species, e.form));
                children[key(species, form)].append(key(e.species, e.form));
            }
        }
    }
    QSet<qint64> result;
    for (auto it = children.constBegin(); it != children.constEnd(); ++it) {
        if (targets.contains(it.key())) continue;
        for (qint64 child : it.value()) {
            if (!children.value(child).isEmpty()) {
                result.insert(it.key());
                break;
            }
        }
    }
    return result;
}

// Setzt die Arten der drei Starter-Modelle in a_0101.bin (Feld wird direkt ueberschrieben)
bool patchStarterModels(QByteArray& data, const int critters[3], int s0, int s1, int s2) {
    auto u32 = [&](int pos) -> qint64 {
        if (pos < 0 || pos + 4 > data.size()) return -1;
        return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + pos));
    };
    auto u16 = [&](int pos) -> int {
        if (pos < 0 || pos + 2 > data.size()) return 0;
        return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(data.constData() + pos));
    };
    auto fieldPos = [&](int table, int field) -> int {
        const qint64 soff = u32(table);
        if (soff < 0) return -1;
        const int vt = table - static_cast<qint32>(soff);
        const int vtSize = u16(vt);
        if (4 + 2 * field + 2 > vtSize) return -1;
        const int off = u16(vt + 4 + 2 * field);
        return off ? table + off : -1;
    };
    auto deref = [&](int pos) -> int {
        const qint64 v = u32(pos);
        return v < 0 ? -1 : pos + static_cast<int>(v);
    };
    auto element = [&](int vec, int i) -> int {
        if (vec < 0 || i >= u32(vec)) return -1;
        return deref(vec + 4 + 4 * i);
    };

    const int root = static_cast<int>(u32(0));
    const int zones = deref(fieldPos(root, 0));
    const int zone = element(zones, 0);
    if (zone < 0) return false;
    const int list = deref(fieldPos(zone, 2));
    const int species[3] = {s0, s1, s2};
    for (int i = 0; i < 3; i++) {
        const int critter = element(list, critters[i]);
        const int pos = critter < 0 ? -1 : fieldPos(critter, 2);
        if (pos < 0) return false;
        // Nur die drei Starter-Modelle (Chimpep, Hopplo, Memmeon) ueberschreiben
        const qint64 current = u32(pos);
        if (current != 810 && current != 813 && current != 816) return false;
        qToLittleEndian<quint32>(static_cast<quint32>(species[i]), reinterpret_cast<uchar*>(data.data() + pos));
    }
    return true;
}

int genderFor(int species, int form) {
    if (species == 678 || species == 876) return form == 1 ? 2 : 1; // Psiaugon, Servol
    return 0; // zufaellig
}

} // namespace

// ------------------------------------------------------------- Einstellungen

QJsonObject encounterSettingsToJson(const EncounterSettings& s) {
    QJsonObject j;
    j["starterMode"] = s.starterMode;
    j["starterTypes"] = s.starterTypes;
    j["starterThreeStages"] = s.starterThreeStages;
    for (int i = 0; i < 3; i++) {
        j[QString("wished%1").arg(i)] = QString("%1:%2").arg(s.wished[i].species).arg(s.wished[i].form);
    }
    j["gifts"] = s.gifts;
    j["statics"] = s.statics;
    j["overworld"] = s.overworld;
    j["trades"] = s.trades;
    j["similarStrengthOptional"] = s.similarStrength;
    j["starterSimilarStrength"] = s.starterSimilarStrength;
    j["legendaries"] = s.legendaries;
    return j;
}

void encounterSettingsFromJson(const QJsonObject& j, EncounterSettings& s) {
    s.starterMode = qBound(0, j["starterMode"].toInt(s.starterMode), 2);
    s.starterTypes = qBound(0, j["starterTypes"].toInt(s.starterTypes), 2);
    s.starterThreeStages = j["starterThreeStages"].toBool(s.starterThreeStages);
    for (int i = 0; i < 3; i++) {
        const QStringList parts = j[QString("wished%1").arg(i)].toString().split(':');
        if (parts.size() == 2) {
            s.wished[i].species = parts[0].toInt();
            s.wished[i].form = parts[1].toInt();
        }
    }
    s.gifts = j["gifts"].toBool(s.gifts);
    s.statics = j["statics"].toBool(s.statics);
    s.overworld = j["overworld"].toBool(s.overworld);
    s.trades = j["trades"].toBool(s.trades);
    s.similarStrength = j["similarStrengthOptional"].toBool(s.similarStrength);
    s.starterSimilarStrength = j["starterSimilarStrength"].toBool(s.starterSimilarStrength);
    s.legendaries = j["legendaries"].toBool(s.legendaries);
}

QList<StarterChoice> availablePokemon(const GameFiles& files) {
    QList<StarterChoice> list;
    Picker picker(files);
    for (const Mon& m : picker.all) {
        list.append({m.species, m.form});
    }
    return list;
}

// ------------------------------------------------------------- Randomizer

bool randomizeEncounters(const QString& romfs, const GameFiles& files, const EncounterSettings& s, quint64 seed,
                         EncounterResult& result, QString* error) {
    result = EncounterResult();
    if (!s.anyEnabled()) {
        return true;
    }
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };

    Picker picker(files);
    QRandomGenerator rng(seed ^ 0x5EED5EED5EEDULL);

    auto canDynamax = [&](const Mon& m) {
        return files.personal.canDynamax(files.personal.indexOf(m.species, m.form));
    };
    auto types = [&](const Mon& m) {
        const int index = files.personal.indexOf(m.species, m.form);
        return QSet<int>{files.personal.type1(index), files.personal.type2(index)};
    };

    // ---------------------------------------------------------- Geschenke
    bool ok = false;
    QList<FlatRecord> gifts = readFlatArchive(readFile(romfs + "/" + path::Gifts), gift::kSizes, &ok);
    if (!ok || gifts.size() <= 4) {
        return fail("Die Geschenk-Daten (add_poke.bin) konnten nicht gelesen werden.");
    }
    bool giftsChanged = false;

    // Starter
    if (s.starterMode != 0) {
        Mon chosen[3];
        if (s.starterMode == 2) {
            for (int i = 0; i < 3; i++) {
                chosen[i] = {s.wished[i].species, s.wished[i].form};
                const int index = files.personal.indexOf(chosen[i].species, chosen[i].form);
                if (chosen[i].species <= 0 || index <= 0 || !files.personal.isPresent(index)) {
                    // Nicht gesetzt: Original behalten
                    chosen[i] = {static_cast<int>(gifts[kStarterGifts[i]].get(gift::Species)),
                                 static_cast<int>(gifts[kStarterGifts[i]].get(gift::Form))};
                }
            }
        } else {
            const QSet<qint64> bases = threeStageBases(files);
            QSet<int> used;
            QSet<int> usedTypes;
            const int triangle[3] = {11, 9, 10}; // Pflanze, Feuer, Wasser
            for (int i = 0; i < 3; i++) {
                auto filter = [&](const Mon& m) {
                    if (!s.legendaries && isLegendary(m.species)) return false;
                    if (s.starterThreeStages && !bases.contains(static_cast<qint64>(m.species) * 100 + m.form)) return false;
                    const QSet<int> t = types(m);
                    if (s.starterTypes == 1 && t.intersects(usedTypes)) return false;
                    if (s.starterTypes == 2 && !t.contains(triangle[i])) return false;
                    return true;
                };
                Mon m;
                const FlatRecord& original = gifts[kStarterGifts[i]];
                const int reference = s.starterSimilarStrength
                    ? picker.bst(static_cast<int>(original.get(gift::Species)), static_cast<int>(original.get(gift::Form)))
                    : 0;
                if (!picker.pick(rng, filter, reference, used, m)) {
                    // Zu streng: ohne Typ-Regel versuchen
                    auto loose = [&](const Mon& c) {
                        return (s.legendaries || !isLegendary(c.species)) &&
                               (!s.starterThreeStages || bases.contains(static_cast<qint64>(c.species) * 100 + c.form));
                    };
                    if (!picker.pick(rng, loose, reference, used, m)) return fail("Es wurden keine passenden Starter gefunden.");
                }
                chosen[i] = m;
                used.insert(m.species);
                usedTypes += types(m);
            }
        }

        for (int i = 0; i < 3; i++) {
            FlatRecord& g = gifts[kStarterGifts[i]];
            EncounterChange c;
            c.category = 0;
            c.index = i;
            c.oldSpecies = static_cast<int>(g.get(gift::Species));
            c.oldForm = static_cast<int>(g.get(gift::Form));
            c.newSpecies = chosen[i].species;
            c.newForm = chosen[i].form;
            c.level = static_cast<int>(g.get(gift::Level));
            g.set(gift::Species, chosen[i].species);
            g.set(gift::Form, chosen[i].form);
            g.set(gift::Gender, genderFor(chosen[i].species, chosen[i].form));
            g.set(gift::CanGigantamax, 0);
            result.changes.append(c);
        }
        giftsChanged = true;

        // Modelle auf dem Tisch anpassen (nur Art; Formen ohne Feld bleiben die Grundform)
        GfPak pack;
        if (pack.load(readFile(romfs + "/" + path::Placement))) {
            const int fileIndex = pack.indexOf("a_0101.bin");
            QByteArray zone = pack.file(fileIndex);
            if (!zone.isEmpty() && patchStarterModels(zone, kStarterCritters, chosen[0].species, chosen[1].species,
                                                      chosen[2].species)) {
                pack.setFile(fileIndex, zone);
                result.placement = pack.save();
            }
        }
    }

    // Andere Geschenke
    if (s.gifts) {
        QSet<int> used;
        for (int i = 0; i < gifts.size(); i++) {
            if (kFixedGifts.contains(i) || i == kStarterGifts[0] || i == kStarterGifts[1] || i == kStarterGifts[2]) {
                continue;
            }
            FlatRecord& g = gifts[i];
            const Mon old{static_cast<int>(g.get(gift::Species)), static_cast<int>(g.get(gift::Form))};
            if (old.species <= 0) continue;
            const bool gmax = g.get(gift::CanGigantamax) != 0;
            const bool legendary = isLegendary(old.species);
            auto filter = [&](const Mon& m) {
                if (gmax && !canGigantamax(m.species, m.form)) return false;
                if (legendary) return isLegendary(m.species);
                return s.legendaries || !isLegendary(m.species);
            };
            Mon m;
            if (!picker.pick(rng, filter, s.similarStrength ? picker.bst(old.species, old.form) : 0, used, m)) continue;
            used.insert(m.species);
            g.set(gift::Species, m.species);
            g.set(gift::Form, m.form);
            g.set(gift::Gender, genderFor(m.species, m.form));
            g.set(gift::SpecialMove, 0);
            if (kSpeciesItems.contains(static_cast<int>(g.get(gift::HeldItem)))) g.set(gift::HeldItem, 0);
            result.changes.append({1, i, old.species, old.form, m.species, m.form, static_cast<int>(g.get(gift::Level)),
                                   gmax ? QString("Gigadynamax") : QString()});
        }
        giftsChanged = true;
    }
    if (giftsChanged) {
        result.gifts = writeFlatArchive(gifts, gift::kSizes);
    }

    // ---------------------------------------------- Statische Begegnungen
    if (s.statics || s.overworld) {
        QList<FlatRecord> statics = readFlatArchive(readFile(romfs + "/" + path::Statics), encounter::kSizes, &ok);
        if (!ok || statics.isEmpty()) {
            return fail("Die Begegnungs-Daten (event_encount_data.bin) konnten nicht gelesen werden.");
        }
        for (int i = 0; i < statics.size(); i++) {
            FlatRecord& e = statics[i];
            const Mon old{static_cast<int>(e.get(encounter::Species)), static_cast<int>(e.get(encounter::Form))};
            const int scenario = static_cast<int>(e.get(encounter::Scenario));
            if (old.species <= 0) continue;
            // Story-kritisch: unveraendert
            if (old.species >= 888 && old.species <= 892) continue; // Zacian, Zamazenta, Endynalos, Dakuma, Wulaosu
            if (scenario == ScenarioSlowpoke || scenario == ScenarioCalyrex || scenario == ScenarioSteeds ||
                scenario == ScenarioCalyrexFusion || scenario == ScenarioZacianBoss) continue;

            const bool legendary = isLegendary(old.species);
            const bool story = legendary || scenario != ScenarioNone || i < 53;
            if ((story && !s.statics) || (!story && !s.overworld)) continue;

            const bool raid = isRaidScenario(scenario) || e.get(encounter::DynamaxLevel) > 0;
            auto filter = [&](const Mon& m) {
                if (raid && !canDynamax(m)) return false;
                if (legendary) return isLegendary(m.species);
                return s.legendaries || !isLegendary(m.species);
            };
            Mon m;
            if (!picker.pick(rng, filter, s.similarStrength ? picker.bst(old.species, old.form) : 0, QSet<int>(), m)) continue;
            e.set(encounter::Species, m.species);
            e.set(encounter::Form, m.form);
            e.set(encounter::Gender, genderFor(m.species, m.form));
            for (int f : {encounter::Move0, encounter::Move1, encounter::Move2, encounter::Move3}) e.set(f, 0);
            if (!canGigantamax(m.species, m.form)) e.set(encounter::CanGigantamax, 0);
            if (kSpeciesItems.contains(static_cast<int>(e.get(encounter::HeldItem)))) e.set(encounter::HeldItem, 0);
            result.changes.append({story ? 2 : 3, i, old.species, old.form, m.species, m.form,
                                   static_cast<int>(e.get(encounter::Level)),
                                   raid ? QString("Dynamax-Kampf") : QString()});
        }
        result.statics = writeFlatArchive(statics, encounter::kSizes);
    }

    // ------------------------------------------------------------- Tausch
    if (s.trades) {
        QList<FlatRecord> trades = readFlatArchive(readFile(romfs + "/" + path::Trades), trade::kSizes, &ok);
        if (!ok || trades.isEmpty()) {
            return fail("Die Tausch-Daten (field_trade.bin) konnten nicht gelesen werden.");
        }
        QSet<int> used;
        for (int i = 0; i < trades.size(); i++) {
            FlatRecord& t = trades[i];
            const Mon old{static_cast<int>(t.get(trade::Species)), static_cast<int>(t.get(trade::Form))};
            if (old.species <= 0) continue;
            auto filter = [&](const Mon& m) { return s.legendaries || !isLegendary(m.species); };
            Mon m;
            if (!picker.pick(rng, filter, s.similarStrength ? picker.bst(old.species, old.form) : 0, used, m)) continue;
            used.insert(m.species);
            t.set(trade::Species, m.species);
            t.set(trade::Form, m.form);
            t.set(trade::Gender, genderFor(m.species, m.form));
            for (int f : {trade::Relearn1, trade::Relearn2, trade::Relearn3, trade::Relearn4}) t.set(f, 0);
            if (!canGigantamax(m.species, m.form)) t.set(trade::CanGigantamax, 0);
            if (kSpeciesItems.contains(static_cast<int>(t.get(trade::HeldItem)))) t.set(trade::HeldItem, 0);
            result.changes.append({4, i, old.species, old.form, m.species, m.form, static_cast<int>(t.get(trade::Level)),
                                   QString(), static_cast<int>(t.get(trade::RequiredSpecies))});
        }
        result.trades = writeFlatArchive(trades, trade::kSizes);
    }
    return true;
}

bool writeEncounters(const QString& outRomfs, const EncounterResult& result) {
    bool ok = true;
    if (!result.gifts.isEmpty()) ok &= writeFile(outRomfs + "/" + path::Gifts, result.gifts);
    if (!result.statics.isEmpty()) ok &= writeFile(outRomfs + "/" + path::Statics, result.statics);
    if (!result.trades.isEmpty()) ok &= writeFile(outRomfs + "/" + path::Trades, result.trades);
    if (!result.placement.isEmpty()) ok &= writeFile(outRomfs + "/" + path::Placement, result.placement);
    return ok;
}

} // namespace swsh

// ------------------------------------------------------------ Spoiler-Log

namespace swsh {

QList<SpoilerSection> encounterSpoiler(const EncounterResult& result, const GameFiles& files, const GameTexts& texts) {
    auto esc = [](const QString& t) { return t.toHtmlEscaped(); };
    auto typeChips = [&](int species, int form) {
        const int index = files.personal.indexOf(species, form);
        const int t1 = files.personal.type1(index);
        const int t2 = files.personal.type2(index);
        QString html = "<span class=\"type t" + QString::number(t1) + "\">" + esc(texts.typeName(t1)) + "</span>";
        if (t2 != t1) html += "<span class=\"type t" + QString::number(t2) + "\">" + esc(texts.typeName(t2)) + "</span>";
        return html;
    };

    const QStringList titles = {"Starter", "Geschenkte Pokémon", "Legendäre und Story-Begegnungen",
                                "Feste Pokémon in der Spielwelt", "Tausch"};
    QList<SpoilerSection> sections;
    for (int category = 0; category < titles.size(); category++) {
        SpoilerSection section;
        section.title = titles[category];
        QString rows;
        for (const EncounterChange& c : result.changes) {
            if (c.category != category) continue;
            section.count++;
            const QString oldName = texts.pokemonName(c.oldSpecies, c.oldForm);
            const QString newName = texts.pokemonName(c.newSpecies, c.newForm);
            QString search = (oldName + " " + newName).toLower();
            if (category == 0) {
                static const QStringList slotNames = {"statt Chimpep", "statt Hopplo", "statt Memmeon"};
                rows += "<div class=\"mon\" data-search=\"" + esc(search) + "\"><div class=\"mon-head\"><span class=\"mon-name\">" +
                        esc(newName) + "</span><span class=\"lvl\">Lv. " + QString::number(c.level) +
                        "</span></div><div class=\"types\">" + typeChips(c.newSpecies, c.newForm) +
                        "</div><div class=\"meta\">" + esc(slotNames.value(c.index)) + "</div></div>";
                continue;
            }
            QString note = c.note;
            if (category == 4 && c.requiredSpecies > 0) {
                note = "für " + texts.pokemonName(c.requiredSpecies, 0);
                search += " " + note.toLower();
            }
            rows += "<tr data-search=\"" + esc(search) + "\"><td class=\"num\">" + QString::number(c.index) + "</td><td>" +
                    esc(oldName) + "</td><td class=\"arrow\">→</td><td><b>" + esc(newName) + "</b> " +
                    typeChips(c.newSpecies, c.newForm) + "</td><td class=\"num\">Lv. " + QString::number(c.level) +
                    "</td><td class=\"hint\">" + esc(note) + "</td></tr>";
        }
        if (section.count == 0) continue;
        if (category == 0) {
            section.html = "<div class=\"team\">" + rows + "</div>";
        } else {
            section.html = "<table class=\"enc\"><thead><tr><th>#</th><th>Original</th><th></th><th>Neu</th><th>Level</th>"
                           "<th></th></tr></thead><tbody>" + rows + "</tbody></table>";
        }
        sections.append(section);
    }
    return sections;
}

} // namespace swsh
