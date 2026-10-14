// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Starter, Geschenke,
// statische Begegnungen und Tausch
// ---------------------------------------------------------------------------

#include "headers/bdsp/bdsp_encounters.h"
#include "headers/bdsp/bdsp_exefs.h"
#include "headers/common/trainer_smart.h"

#include <QHash>
#include <QRandomGenerator>
#include <QSet>
#include <cstring>
#include <functional>

namespace bdsp {

namespace {

// Skriptbefehle (EvCmdID)
enum Cmd {
    CmdIfValJump = 32, CmdIfValCall = 33, CmdCaseJump = 37, CmdVoicePlay = 112, CmdAddTamago = 219,
    CmdPokemonNameExtra = 322, CmdFirstPokeNoGet = 326, CmdSecretPokeRetryCheck = 346, CmdWildBtlSet = 401,
    CmdSpWildBtlSet = 402, CmdTemotiPokeChk = 563, CmdLdvalVersion = 840
};
enum ArgType { ArgCmd = 0, ArgFloat = 1, ArgWork = 2 };

const int kOriginalStarters[3] = {387, 390, 393}; // Chelast, Panflam, Plinfa (Koffer-Plaetze 0, 1, 2)
const int kStarterTypes[3] = {11, 9, 10};         // Pflanze, Feuer, Wasser

struct Mon {
    int species = 0;
    int form = 0;
};

int cmdId(const UValue& cmd) {
    const std::vector<UValue>& args = cmd["Arg"].list();
    return args.empty() ? -1 : args[0]["data"].toInt();
}
float argFloat(const UValue& arg) {
    const qint32 bits = static_cast<qint32>(arg["data"].i);
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
bool argIsFloat(const UValue& arg) { return arg["argType"].i == ArgFloat; }
int argSpecies(const UValue& arg) { return argIsFloat(arg) ? static_cast<int>(argFloat(arg)) : -1; }
void setArgFloat(UValue& arg, float value) {
    qint32 bits;
    std::memcpy(&bits, &value, 4);
    arg["data"].i = bits;
}

// Ortsnamen der statischen Begegnungen (Kartencode -> Ort)
QString placeForZone(const QString& zone) {
    static const QList<QPair<QString, QString>> places = {
        {"d05r0116", "Saal des Ursprungs"}, {"d05r01", "Speersäule"}, {"d10", "Ramanas-Park"},
        {"d02", "Windkraftwerk"}, {"d16", "Kraterberg"}, {"d17", "Höhle der Umkehr"}, {"d18", "Blumenparadies"},
        {"d20", "Tempel von Blizzach"}, {"d25", "Alte Villa"}, {"d28", "Kühnheitsee"}, {"d29", "Scharfsinnsee"},
        {"d30", "Neumondinsel"}, {"r209", "Route 209"}, {"c05", "Herzhofen"}, {"d24", "Eiseninsel"},
    };
    for (const auto& p : places) {
        if (zone.startsWith(p.first)) return p.second;
    }
    return zone;
}

class Picker {
public:
    explicit Picker(const GameFiles& f) : files(f) {
        for (int species = 1; species <= kMaxSpecies; species++) {
            const int index = files.personal.indexOf(species, 0);
            if (index > 0 && files.personal.isPresent(index)) all.append({species, 0});
        }
    }
    int bst(int species) const {
        const int index = files.personal.indexOf(species, 0);
        int sum = 0;
        for (int s = 0; s < 6; s++) sum += files.personal.stat(index, s);
        return sum;
    }
    QSet<int> types(int species) const {
        const int index = files.personal.indexOf(species, 0);
        return {files.personal.type1(index), files.personal.type2(index)};
    }
    // Gleichmaessig eine Art aus allen Kandidaten; bei "aehnlicher Staerke" wird der Bereich schrittweise erweitert
    bool pick(QRandomGenerator& rng, const std::function<bool(int)>& filter, int referenceBst, const QSet<int>& avoid,
              int& out) const {
        for (int tolerance : {10, 20, 35, 1000}) {
            if (referenceBst <= 0 && tolerance != 1000) continue;
            QList<int> candidates;
            for (const Mon& m : all) {
                if (avoid.contains(m.species) || !filter(m.species)) continue;
                if (tolerance != 1000 && qAbs(bst(m.species) - referenceBst) * 100 > referenceBst * tolerance) continue;
                candidates.append(m.species);
            }
            if (!candidates.isEmpty()) {
                out = candidates[static_cast<int>(rng.bounded(static_cast<int>(candidates.size())))];
                return true;
            }
        }
        return false;
    }
    const GameFiles& files;
    QList<Mon> all;
};

// Entwicklungsdaten: Vorentwicklung und erste Entwicklung je Art (Grundform)
struct EvolutionInfo {
    QHash<int, int> parent;
    QHash<int, int> firstChild;
    explicit EvolutionInfo(const GameFiles& files) {
        for (int species = 1; species <= kMaxSpecies; species++) {
            const int index = files.personal.indexOf(species, 0);
            if (index <= 0) continue;
            for (const Evolution& e : files.evolutions.get(index)) {
                if (e.species <= 0 || e.species > kMaxSpecies) continue;
                if (!parent.contains(e.species)) parent.insert(e.species, species);
                if (!firstChild.contains(species)) firstChild.insert(species, e.species);
            }
        }
    }
    bool isBase(int species) const { return !parent.contains(species); }
    bool isThreeStageBase(int species) const {
        return isBase(species) && firstChild.contains(species) && firstChild.contains(firstChild.value(species));
    }
    QList<int> line(int base) const {
        QList<int> l{base};
        while (firstChild.contains(l.last()) && l.size() < 3) l.append(firstChild.value(l.last()));
        return l;
    }
    int stageOf(int species) const {
        int stage = 0;
        int s = species;
        while (parent.contains(s) && stage < 3) {
            s = parent.value(s);
            stage++;
        }
        return stage;
    }
    int baseOf(int species) const {
        int s = species;
        int guard = 0;
        while (parent.contains(s) && guard++ < 3) s = parent.value(s);
        return s;
    }
};

int sexFor(const GameFiles& files, int species, int original) {
    const int ratio = files.personal.genderRatio(files.personal.indexOf(species, 0));
    if (ratio == 255) return 2; // geschlechtslos
    if (ratio == 0) return 0;   // nur maennlich
    if (ratio == 254) return 1; // nur weiblich
    return original == 2 ? 0 : original;
}

} // namespace

// ------------------------------------------------------------- Einstellungen

QJsonObject encounterSettingsToJson(const EncounterSettings& s) {
    QJsonObject j;
    j["starterMode"] = s.starterMode;
    j["starterTypes"] = s.starterTypes;
    j["starterThreeStages"] = s.starterThreeStages;
    for (int i = 0; i < 3; i++) j[QString("wished%1").arg(i)] = QString("%1:%2").arg(s.wished[i].species).arg(s.wished[i].form);
    j["gifts"] = s.gifts;
    j["statics"] = s.statics;
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
    s.trades = j["trades"].toBool(s.trades);
    s.similarStrength = j["similarStrengthOptional"].toBool(s.similarStrength);
    s.starterSimilarStrength = j["starterSimilarStrength"].toBool(s.starterSimilarStrength);
    s.legendaries = j["legendaries"].toBool(s.legendaries);
}

QList<StarterChoice> availablePokemon(const GameFiles& files) {
    QList<StarterChoice> list;
    Picker picker(files);
    for (const Mon& m : picker.all) list.append({m.species, 0});
    return list;
}

// ------------------------------------------------------------- Randomizer

bool randomizeEncounters(const QString& romfs, const QString& exefs, GameFiles& files, const EncounterSettings& s,
                         quint64 seed, EncounterResult& result, QString* error) {
    result = EncounterResult();
    if (!s.anyEnabled()) return true;
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };

    Picker picker(files);
    const EvolutionInfo evo(files);
    QRandomGenerator rng(seed ^ 0xB0D5B0D5ULL);
    TrainerSmartData smart;
    smart.build(files.toGameData());

    QString err;
    if (!result.scripts.loadWhere(romfs, path::EvScript, [](const UValue& v) { return v.has("Scripts"); }, &err)) {
        return fail("Die Skripte (ev_script) konnten nicht gelesen werden:\n" + err);
    }
    QSet<QString> changedScripts;

    ExeFsPatcher exe;
    bool exeLoaded = false;
    auto needExe = [&]() -> bool {
        if (exeLoaded) return true;
        if (exefs.isEmpty()) return fail("Für Starter und Evoli wird der ExeFS-Ordner deines Dumps benötigt.");
        if (!exe.load(exefs, &err)) return fail(err);
        exeLoaded = true;
        return true;
    };

    // ------------------------------------------------------------ Starter
    if (s.starterMode != 0) {
        int chosen[3] = {kOriginalStarters[0], kOriginalStarters[1], kOriginalStarters[2]};
        if (s.starterMode == 2) {
            for (int i = 0; i < 3; i++) {
                const int index = files.personal.indexOf(s.wished[i].species, 0);
                if (s.wished[i].species > 0 && index > 0 && files.personal.isPresent(index)) chosen[i] = s.wished[i].species;
            }
        } else {
            QSet<int> used;
            QSet<int> usedTypes;
            for (int i = 0; i < 3; i++) {
                auto filter = [&](int species) {
                    if (!s.legendaries && isLegendary(species)) return false;
                    if (s.starterThreeStages && !evo.isThreeStageBase(species)) return false;
                    const QSet<int> t = picker.types(species);
                    if (s.starterTypes == 1 && t.intersects(usedTypes)) return false;
                    if (s.starterTypes == 2 && !t.contains(kStarterTypes[i])) return false;
                    return true;
                };
                const int reference = s.starterSimilarStrength ? picker.bst(kOriginalStarters[i]) : 0;
                int m = 0;
                if (!picker.pick(rng, filter, reference, used, m)) {
                    auto loose = [&](int species) {
                        return (s.legendaries || !isLegendary(species)) &&
                               (!s.starterThreeStages || evo.isThreeStageBase(species));
                    };
                    if (!picker.pick(rng, loose, reference, used, m)) return fail("Es wurden keine passenden Starter gefunden.");
                }
                chosen[i] = m;
                used.insert(m);
                usedTypes += picker.types(m);
            }
        }

        if (!needExe()) return false;
        if (!exe.patchStarters(chosen[0], chosen[1], chosen[2], &err)) return fail(err);
        bool ok = false;
        const QByteArray metadata = readFile(romfs + "/Data/Managed/Metadata/global-metadata.dat", &ok);
        if (!ok) return fail("Im RomFS fehlt die Datei „Data/Managed/Metadata/global-metadata.dat“.");
        result.metadata = patchMetadataStarters(metadata, chosen[0], chosen[1], chosen[2]);
        if (result.metadata.isEmpty()) return fail("Die Starter-Liste wurde in global-metadata.dat nicht gefunden.");

        auto newStarter = [&](int species) {
            for (int i = 0; i < 3; i++) {
                if (species == kOriginalStarters[i]) return chosen[i];
            }
            return species;
        };
        // Skripte: Vergleiche mit dem Starter des Spielers (Rivale, Lucius/Lucia, Labor, Accessoires ...)
        for (const QString& zone : result.scripts.tableNames()) {
            UValue& data = result.scripts.table(zone);
            for (UValue& script : data["Scripts"].list()) {
                QSet<qint64> works;
                for (UValue& cmd : script["Commands"].list()) {
                    std::vector<UValue>& args = cmd["Arg"].list();
                    const int id = cmdId(cmd);
                    if (id == CmdFirstPokeNoGet && args.size() >= 2 && args[1]["argType"].i == ArgWork) {
                        works.insert(args[1]["data"].i);
                    } else if ((id == CmdIfValJump || id == CmdIfValCall) && args.size() >= 4 &&
                               args[1]["argType"].i == ArgWork && works.contains(args[1]["data"].i)) {
                        const int species = argSpecies(args[3]);
                        if (species > 0 && newStarter(species) != species) {
                            setArgFloat(args[3], static_cast<float>(newStarter(species)));
                            changedScripts.insert(zone);
                        }
                    } else if (id == CmdCaseJump && args.size() >= 2) {
                        const int species = argSpecies(args[1]);
                        if (species > 0 && newStarter(species) != species) {
                            setArgFloat(args[1], static_cast<float>(newStarter(species)));
                            changedScripts.insert(zone);
                        }
                    }
                }
            }
        }

        // Teams von Rivale und Lucius/Lucia: Starter-Linie mittauschen (gleiche Entwicklungsstufe)
        const QList<int> oldLines[3] = {evo.line(387), evo.line(390), evo.line(393)};
        const QList<int> newLines[3] = {evo.line(chosen[0]), evo.line(chosen[1]), evo.line(chosen[2])};
        for (Trainer& t : files.trainers.all()) {
            const QString c = t.classLabel;
            if (!c.contains("TR_RIVAL") && !c.contains("TR_HERO")) continue;
            for (TrainerMon& mon : t.team) {
                for (int i = 0; i < 3; i++) {
                    const int stage = oldLines[i].indexOf(mon.species);
                    if (stage < 0) continue;
                    const int species = newLines[i].value(qMin(stage, static_cast<int>(newLines[i].size()) - 1));
                    const int index = files.personal.indexOf(species, 0);
                    mon.species = species;
                    mon.form = 0;
                    mon.sex = 3;
                    mon.ability = files.personal.ability(index, 0);
                    const QList<int> moves = smart.defaultMoveset(species, 0, mon.level);
                    for (int k = 0; k < 4; k++) mon.moves[k] = k < moves.size() ? moves[k] : 0;
                    mon.locked = true;
                    t.changed = true;
                    result.rivalPokemon++;
                    break;
                }
            }
        }

        for (int i = 0; i < 3; i++) {
            EncounterChange c;
            c.category = 0;
            c.index = i;
            c.oldSpecies = kOriginalStarters[i];
            c.newSpecies = chosen[i];
            c.level = 5;
            result.changes.append(c);
        }
    }

    // ---------------------------------------------------------- Geschenke
    if (s.gifts) {
        QSet<int> used;
        // Evoli von Bebe (Programmcode)
        int eevee = 0;
        auto filter = [&](int species) { return s.legendaries || !isLegendary(species); };
        if (picker.pick(rng, filter, s.similarStrength ? picker.bst(133) : 0, used, eevee)) {
            if (!needExe()) return false;
            if (!exe.patchEevee(eevee, &err)) return fail(err);
            used.insert(eevee);
            result.changes.append({1, 0, 133, eevee, 5, "Bebe in Herzhofen"});
        }
        // Eier (Skript): nur Basis-Pokemon, damit das Ei sinnvoll schluepft
        for (const QString& zone : result.scripts.tableNames()) {
            UValue& data = result.scripts.table(zone);
            for (UValue& script : data["Scripts"].list()) {
                for (UValue& cmd : script["Commands"].list()) {
                    std::vector<UValue>& args = cmd["Arg"].list();
                    if (cmdId(cmd) != CmdAddTamago || args.size() < 2) continue;
                    const int old = argSpecies(args[1]);
                    if (old <= 0) continue;
                    auto eggFilter = [&](int species) { return evo.isBase(species) && !isLegendary(species); };
                    int m = 0;
                    if (!picker.pick(rng, eggFilter, s.similarStrength ? picker.bst(old) : 0, used, m)) continue;
                    used.insert(m);
                    setArgFloat(args[1], static_cast<float>(m));
                    changedScripts.insert(zone);
                    result.changes.append({1, 1, old, m, 1, "Ei · " + placeForZone(zone)});
                }
            }
        }
    }

    // ---------------------------------------------- Statische Begegnungen
    if (s.statics) {
        struct Found { QString zone; int species; int level; };
        QList<Found> found;
        QSet<QString> zones;
        for (const QString& zone : result.scripts.tableNames()) {
            const UValue& data = result.scripts.table(zone);
            for (const UValue& script : data["Scripts"].list()) {
                for (const UValue& cmd : script["Commands"].list()) {
                    const int id = cmdId(cmd);
                    const std::vector<UValue>& args = cmd["Arg"].list();
                    if ((id != CmdSpWildBtlSet && id != CmdWildBtlSet) || args.size() < 3) continue;
                    const int species = argSpecies(args[1]);
                    if (species <= 0) continue; // Wiederholung ueber Variable (Dialga/Palkia)
                    found.append({zone, species, argSpecies(args[2])});
                    zones.insert(zone);
                }
            }
        }
        // Eine neue Art je Original-Art (z. B. Giratina in der Höhle der Umkehr und im Ramanas-Park gleich)
        QHash<int, int> mapping;
        QSet<int> used;
        for (const Found& f : found) {
            if (mapping.contains(f.species)) continue;
            const bool legendary = isLegendary(f.species);
            auto filter = [&](int species) {
                if (species == f.species) return false; // immer eine andere Art
                if (legendary) return isLegendary(species);
                return s.legendaries || !isLegendary(species);
            };
            const int reference = s.similarStrength ? picker.bst(f.species) : 0;
            int m = 0;
            if (!picker.pick(rng, filter, reference, used, m) && !picker.pick(rng, filter, reference, QSet<int>(), m)) continue;
            used.insert(m);
            mapping.insert(f.species, m);
        }
        QSet<QString> listed; // jede Art je Ort nur einmal im Spoiler-Log
        for (const Found& f : found) {
            if (!mapping.contains(f.species)) continue;
            const QString note = placeForZone(f.zone);
            const QString key = QString::number(f.species) + "|" + note;
            if (listed.contains(key)) continue;
            listed.insert(key);
            result.changes.append({2, 0, f.species, mapping.value(f.species), f.level, note});
        }
        // Alle zugehoerigen Befehle in diesen Karten anpassen (Kampf, Ruf, Wiederholung, Name, Version, Team-Pruefung)
        for (const QString& zone : zones) {
            UValue& data = result.scripts.table(zone);
            for (UValue& script : data["Scripts"].list()) {
                // Ruf und Name nur in Skripten mit dem Kampf selbst (nicht z. B. Selfe/Tobutz in Zwischenszenen)
                bool hasBattle = false;
                for (const UValue& cmd : script["Commands"].list()) {
                    const int id = cmdId(cmd);
                    hasBattle |= id == CmdSpWildBtlSet || id == CmdWildBtlSet;
                }
                for (UValue& cmd : script["Commands"].list()) {
                    std::vector<UValue>& args = cmd["Arg"].list();
                    QList<int> positions;
                    switch (cmdId(cmd)) {
                    case CmdSpWildBtlSet: case CmdWildBtlSet: positions = {1}; break;
                    case CmdVoicePlay: if (hasBattle) positions = {1}; break;
                    case CmdPokemonNameExtra: if (hasBattle) positions = {2}; break;
                    case CmdSecretPokeRetryCheck: case CmdTemotiPokeChk: positions = {2}; break;
                    case CmdLdvalVersion: positions = {1, 2}; break;
                    default: break;
                    }
                    for (int p : positions) {
                        if (p >= static_cast<int>(args.size())) continue;
                        const int species = argSpecies(args[static_cast<size_t>(p)]);
                        if (species > 0 && mapping.contains(species)) {
                            setArgFloat(args[static_cast<size_t>(p)], static_cast<float>(mapping.value(species)));
                            changedScripts.insert(zone);
                        }
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------- Tausch
    if (s.trades) {
        UValue& table = files.masterBundle.table("LocalKoukanData");
        std::vector<UValue>& entries = table["data"].list();
        QHash<QString, int> chosen; // gleiche Tauschperson (alle Sprachen) -> gleiches Pokemon
        QSet<int> used;
        int index = 0;
        for (UValue& e : entries) {
            const QString key = e["name_label"].toString();
            const int old = e["monsno"].toInt();
            if (old <= 0) continue;
            if (!chosen.contains(key)) {
                auto filter = [&](int species) { return s.legendaries || !isLegendary(species); };
                int m = 0;
                if (!picker.pick(rng, filter, s.similarStrength ? picker.bst(old) : 0, used, m)) continue;
                used.insert(m);
                chosen.insert(key, m);
                result.changes.append({4, index++, old, m, e["level"].toInt(), QString(), e["target"].toInt()});
            }
            const int species = chosen.value(key);
            const int level = e["level"].toInt();
            e["monsno"].i = species;
            e["tokusei"].i = 0;
            e["sex"].i = sexFor(files, species, e["sex"].toInt());
            const QList<int> moves = smart.defaultMoveset(species, 0, level);
            std::vector<UValue>& waza = e["waza"].list();
            for (size_t k = 0; k < waza.size(); k++) waza[k].i = static_cast<int>(k) < moves.size() ? moves[static_cast<int>(k)] : 0;
        }
        files.tradesChanged = true;
    }

    result.changedScripts = QStringList(changedScripts.begin(), changedScripts.end());
    result.changedScripts.sort();
    if (exeLoaded && exe.hasPatches()) {
        result.ips = exe.ips();
        result.ipsName = exe.buildIdName() + ".ips";
    }
    return true;
}

bool writeEncounters(const QString& outFolder, const EncounterResult& result, QString* error) {
    const QString romfsOut = outFolder + "/romfs";
    if (!result.changedScripts.isEmpty() && !result.scripts.save(romfsOut, result.changedScripts, error)) return false;
    if (!result.metadata.isEmpty() &&
        !writeFile(romfsOut + "/Data/Managed/Metadata/global-metadata.dat", result.metadata)) {
        if (error) *error = "global-metadata.dat konnte nicht geschrieben werden.";
        return false;
    }
    if (!result.ips.isEmpty() && !writeFile(outFolder + "/exefs/" + result.ipsName, result.ips)) {
        if (error) *error = "Der ExeFS-Patch konnte nicht geschrieben werden.";
        return false;
    }
    return true;
}

// ------------------------------------------------------------ Spoiler-Log

namespace {
QString esc(const QString& text) { return text.toHtmlEscaped(); }
} // namespace

QList<SpoilerSection> encounterSpoiler(const EncounterResult& result, const GameFiles& files, const GameTexts& texts) {
    QList<SpoilerSection> sections;
    auto typeChips = [&](int species) {
        const int index = files.personal.indexOf(species, 0);
        const int t1 = files.personal.type1(index), t2 = files.personal.type2(index);
        QString html = "<span class=\"type t" + QString::number(t1) + "\">" + esc(texts.typeName(t1)) + "</span>";
        if (t2 != t1) html += "<span class=\"type t" + QString::number(t2) + "\">" + esc(texts.typeName(t2)) + "</span>";
        return html;
    };
    const QStringList titles = {"Starter", "Geschenke", "Legendäre und Begegnungen", "", "Tausch"};
    for (int category : {0, 1, 2, 4}) {
        SpoilerSection section;
        section.title = titles[category];
        QString rows;
        for (const EncounterChange& c : result.changes) {
            if (c.category != category) continue;
            section.count++;
            const QString oldName = texts.pokemonName(c.oldSpecies, 0);
            const QString newName = texts.pokemonName(c.newSpecies, 0);
            QString search = (oldName + " " + newName).toLower();
            if (category == 0) {
                rows += "<div class=\"mon\" data-search=\"" + esc(search) + "\"><div class=\"mon-head\"><span class=\"mon-name\">" +
                        esc(newName) + "</span><span class=\"lvl\">Lv. 5</span></div><div class=\"types\">" +
                        typeChips(c.newSpecies) + "</div><div class=\"meta\">statt " + esc(oldName) + "</div></div>";
                continue;
            }
            QString note = c.note;
            if (category == 4 && c.requiredSpecies > 0) {
                note = "für " + texts.pokemonName(c.requiredSpecies, 0);
                search += " " + note.toLower();
            }
            search += " " + note.toLower();
            rows += "<tr data-search=\"" + esc(search) + "\"><td>" + esc(oldName) + "</td><td class=\"arrow\">→</td><td><b>" +
                    esc(newName) + "</b> " + typeChips(c.newSpecies) + "</td><td class=\"num\">" +
                    (c.level > 1 ? "Lv. " + QString::number(c.level) : QString("Ei")) + "</td><td class=\"hint\">" +
                    esc(note) + "</td></tr>";
        }
        if (section.count == 0) continue;
        if (category == 0) {
            section.html = "<div class=\"team\">" + rows + "</div>";
            if (result.rivalPokemon > 0) {
                section.html += "<p class=\"hint\">Der Rivale und Lucius/Lucia haben passend dazu die neuen Starter im Team.</p>";
            }
        } else {
            section.html = "<table class=\"enc\"><thead><tr><th>Original</th><th></th><th>Neu</th><th>Level</th><th></th></tr>"
                           "</thead><tbody>" + rows + "</tbody></table>";
        }
        sections.append(section);
    }
    return sections;
}

} // namespace bdsp
