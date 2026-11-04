// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant/Leuchtende Perle: Bearbeiten-Modus
// ---------------------------------------------------------------------------

#include "headers/bdsp/bdsp_session.h"
#include "headers/bdsp/bdsp_exefs.h"

#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QtEndian>
#include <algorithm>
#include <cstring>

namespace bdsp {

namespace {

// Skriptbefehle (EvCmdID), wie in bdsp_encounters.cpp / bdsp_items.cpp
enum Cmd {
    CmdLdval = 60, CmdIfValJump = 32, CmdIfValCall = 33, CmdCaseJump = 37, CmdVoicePlay = 112, CmdFldItemEvent = 187,
    CmdAddTamago = 219, CmdPokemonNameExtra = 322, CmdFirstPokeNoGet = 326, CmdSecretPokeRetryCheck = 346,
    CmdWildBtlSet = 401, CmdSpWildBtlSet = 402, CmdTemotiPokeChk = 563, CmdLdvalVersion = 840
};
enum ArgType { ArgFloat = 1, ArgWork = 2 };
constexpr int kWorkHiddenItem = 234;
constexpr int kWorkHiddenCount = 235;
const int kOriginalStarters[3] = {387, 390, 393};
const QStringList kStatNames = {"KP", "Angriff", "Verteidigung", "Sp.-Angriff", "Sp.-Verteidigung", "Initiative"};
const int kStatOrder[6] = {0, 1, 2, 4, 5, 3}; // Anzeige-Reihenfolge -> PersonalTable::stat

// ------------------------------------------------------------------ Werte

bool sameValue(const UValue& a, const UValue& b) {
    if (a.i != b.i || a.f != b.f || a.s != b.s || a.items.size() != b.items.size()) return false;
    for (size_t k = 0; k < a.items.size(); k++) {
        if (!sameValue(a.items[k], b.items[k])) return false;
    }
    return true;
}

bool sameTable(const BundleData& a, const BundleData& b, const QString& name) {
    if (!a.has(name) || !b.has(name)) return a.has(name) == b.has(name);
    return sameValue(a.table(name), b.table(name));
}

bool sameMon(const TrainerMon& a, const TrainerMon& b) {
    if (a.species != b.species || a.form != b.form || a.shiny != b.shiny || a.level != b.level || a.sex != b.sex ||
        a.nature != b.nature || a.ability != b.ability || a.item != b.item || a.ball != b.ball || a.seal != b.seal) {
        return false;
    }
    for (int k = 0; k < 4; k++) if (a.moves[k] != b.moves[k]) return false;
    for (int k = 0; k < 6; k++) if (a.ivs[k] != b.ivs[k] || a.evs[k] != b.evs[k]) return false;
    return true;
}

bool sameTrainer(const Trainer& a, const Trainer& b) {
    if (a.aiBit != b.aiBit || a.team.size() != b.team.size()) return false;
    for (int k = 0; k < a.team.size(); k++) {
        if (!sameMon(a.team[k], b.team[k])) return false;
    }
    return true;
}

const Trainer* trainerAt(const GameFiles& files, int index) {
    const QList<Trainer>& list = files.trainers.all();
    return index >= 0 && index < list.size() ? &list[index] : nullptr;
}

bool sameEvolutions(const QList<Evolution>& a, const QList<Evolution>& b) {
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); i++) {
        if (a[i].method != b[i].method || a[i].argument != b[i].argument || a[i].species != b[i].species ||
            a[i].form != b[i].form || a[i].level != b[i].level) {
            return false;
        }
    }
    return true;
}

bool evolutionsDiffer(const GameFiles& a, const GameFiles& b) {
    for (int i = 0; i < a.evolutions.count(); i++) {
        if (!sameEvolutions(a.evolutions.get(i), b.evolutions.get(i))) return true;
    }
    return false;
}

bool learnsetsDiffer(const GameFiles& a, const GameFiles& b) {
    for (int i = 0; i < a.learnsets.count(); i++) {
        if (a.learnsets.get(i) != b.learnsets.get(i)) return true;
    }
    return false;
}

const UValue& personalRoot(const GameFiles& f) { return f.personalBundle.table("PersonalTable"); }
const UValue& masterTable(const GameFiles& f, const char* name) { return f.masterBundle.table(name); }

// ---------------------------------------------------------------- Skripte

UValue* scriptArg(ScriptData& sd, const ScriptPos& p) {
    if (!sd.bundle.has(p.zone)) return nullptr;
    std::vector<UValue>& scripts = sd.bundle.table(p.zone)["Scripts"].list();
    if (p.script < 0 || static_cast<size_t>(p.script) >= scripts.size()) return nullptr;
    std::vector<UValue>& cmds = scripts[static_cast<size_t>(p.script)]["Commands"].list();
    if (p.command < 0 || static_cast<size_t>(p.command) >= cmds.size()) return nullptr;
    std::vector<UValue>& args = cmds[static_cast<size_t>(p.command)]["Arg"].list();
    if (p.arg < 0 || static_cast<size_t>(p.arg) >= args.size()) return nullptr;
    return &args[static_cast<size_t>(p.arg)];
}

float floatOf(const UValue& arg) {
    const qint32 bits = static_cast<qint32>(arg["data"].i);
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

int readInt(ScriptData& sd, const ScriptPos& p) {
    UValue* a = scriptArg(sd, p);
    return a ? static_cast<int>(floatOf(*a)) : 0;
}

void writeInt(ScriptData& sd, const ScriptPos& p, int value) {
    UValue* a = scriptArg(sd, p);
    if (!a) return;
    const float f = static_cast<float>(value);
    qint32 bits;
    std::memcpy(&bits, &f, 4);
    (*a)["data"].i = bits;
}

int cmdId(const UValue& cmd) {
    const std::vector<UValue>& args = cmd["Arg"].list();
    return args.empty() ? -1 : args[0]["data"].toInt();
}

int argSpecies(const UValue& arg) { return arg["argType"].i == ArgFloat ? static_cast<int>(floatOf(arg)) : -1; }

// Alle bearbeitbaren Stellen in den Original-Skripten suchen (gleiche Regeln wie der Randomizer)
void findScriptPositions(EditSession& s) {
    ScriptData& sd = s.scriptsOriginal;
    const QStringList zones = sd.bundle.tableNames();
    for (int k = 0; k < 3; k++) s.starterPos[k].clear();
    s.eggPos.clear();
    s.statics.clear();
    s.itemSpots.clear();

    // Starter-Vergleiche und Eier
    for (const QString& zone : zones) {
        const std::vector<UValue>& scripts = sd.bundle.table(zone)["Scripts"].list();
        for (size_t si = 0; si < scripts.size(); si++) {
            QSet<qint64> works;
            const std::vector<UValue>& cmds = scripts[si]["Commands"].list();
            for (size_t ci = 0; ci < cmds.size(); ci++) {
                const std::vector<UValue>& args = cmds[ci]["Arg"].list();
                const int id = cmdId(cmds[ci]);
                auto addStarter = [&](int argIndex) {
                    const int species = argSpecies(args[static_cast<size_t>(argIndex)]);
                    for (int k = 0; k < 3; k++) {
                        if (species == kOriginalStarters[k]) {
                            s.starterPos[k].append({zone, static_cast<int>(si), static_cast<int>(ci), argIndex});
                        }
                    }
                };
                if (id == CmdFirstPokeNoGet && args.size() >= 2 && args[1]["argType"].i == ArgWork) {
                    works.insert(args[1]["data"].i);
                } else if ((id == CmdIfValJump || id == CmdIfValCall) && args.size() >= 4 &&
                           args[1]["argType"].i == ArgWork && works.contains(args[1]["data"].i)) {
                    addStarter(3);
                } else if (id == CmdCaseJump && args.size() >= 2) {
                    addStarter(1);
                } else if (id == CmdAddTamago && args.size() >= 2 && argSpecies(args[1]) > 0) {
                    s.eggPos.append({zone, static_cast<int>(si), static_cast<int>(ci), 1});
                }
            }
        }
    }

    // Statische Begegnungen
    struct Found { QString zone; int script; int command; int species; int level; };
    QList<Found> found;
    QSet<QString> staticZones;
    for (const QString& zone : zones) {
        const std::vector<UValue>& scripts = sd.bundle.table(zone)["Scripts"].list();
        for (size_t si = 0; si < scripts.size(); si++) {
            const std::vector<UValue>& cmds = scripts[si]["Commands"].list();
            for (size_t ci = 0; ci < cmds.size(); ci++) {
                const int id = cmdId(cmds[ci]);
                const std::vector<UValue>& args = cmds[ci]["Arg"].list();
                if ((id != CmdSpWildBtlSet && id != CmdWildBtlSet) || args.size() < 3) continue;
                const int species = argSpecies(args[1]);
                if (species <= 0) continue;
                found.append({zone, static_cast<int>(si), static_cast<int>(ci), species, argSpecies(args[2])});
                staticZones.insert(zone);
            }
        }
    }
    QHash<int, int> staticIndex;
    for (const Found& f : found) {
        if (!staticIndex.contains(f.species)) {
            staticIndex.insert(f.species, static_cast<int>(s.statics.size()));
            EditSession::Static st;
            st.species = f.species;
            st.level = f.level;
            s.statics.append(st);
        }
        EditSession::Static& st = s.statics[staticIndex.value(f.species)];
        st.battle.append({f.zone, f.script, f.command, 1});
        const QString place = placeForZone(f.zone);
        if (!st.places.contains(place)) st.places.append(place);
    }
    QStringList sortedZones(staticZones.begin(), staticZones.end());
    sortedZones.sort();
    for (const QString& zone : sortedZones) {
        const std::vector<UValue>& scripts = sd.bundle.table(zone)["Scripts"].list();
        for (size_t si = 0; si < scripts.size(); si++) {
            const std::vector<UValue>& cmds = scripts[si]["Commands"].list();
            bool hasBattle = false;
            for (const UValue& cmd : cmds) {
                const int id = cmdId(cmd);
                hasBattle |= id == CmdSpWildBtlSet || id == CmdWildBtlSet;
            }
            for (size_t ci = 0; ci < cmds.size(); ci++) {
                const std::vector<UValue>& args = cmds[ci]["Arg"].list();
                QList<int> positions;
                switch (cmdId(cmds[ci])) {
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
                    if (species > 0 && staticIndex.contains(species)) {
                        s.statics[staticIndex.value(species)].positions.append(
                            {zone, static_cast<int>(si), static_cast<int>(ci), p});
                    }
                }
            }
        }
    }

    // Items auf dem Boden und versteckte Items: Ort ueber PlaceData (TalkLabel -> Zone)
    QHash<QString, int> labelZone;
    for (const auto& place : s.original.masterBundle.readPrefixed("PlaceData_")) {
        for (const UValue& e : place.second["Data"].list()) {
            const QString label = e["TalkLabel"].toString();
            if (label.startsWith("ev_fld_item_") || label.startsWith("ev_hide_item_")) {
                if (!labelZone.contains(label)) labelZone.insert(label, e["zoneID"].toInt());
            }
        }
    }
    auto placeOf = [&](const QString& label) {
        const int zone = labelZone.value(label, -1);
        return zone < 0 ? QString("Ohne Ort") : s.zoneNames.value(zone, QString("Zone %1").arg(zone));
    };
    if (sd.bundle.has("fld_item")) {
        const std::vector<UValue>& scripts = sd.bundle.table("fld_item")["Scripts"].list();
        for (size_t si = 0; si < scripts.size(); si++) {
            const QString label = scripts[si]["Label"].toString();
            if (!label.startsWith("ev_fld_item_")) continue;
            const std::vector<UValue>& cmds = scripts[si]["Commands"].list();
            for (size_t ci = 0; ci < cmds.size(); ci++) {
                if (cmdId(cmds[ci]) != CmdFldItemEvent) continue;
                const std::vector<UValue>& args = cmds[ci]["Arg"].list();
                if (args.size() < 2 || args[1]["argType"].i != ArgFloat) continue;
                EditSession::ItemSpot spot;
                spot.item = {"fld_item", static_cast<int>(si), static_cast<int>(ci), 1};
                if (args.size() >= 3 && args[2]["argType"].i == ArgFloat) {
                    spot.count = {"fld_item", static_cast<int>(si), static_cast<int>(ci), 2};
                }
                spot.label = placeOf(label);
                s.itemSpots.append(spot);
            }
        }
    }
    if (sd.bundle.has("hide_item")) {
        const std::vector<UValue>& scripts = sd.bundle.table("hide_item")["Scripts"].list();
        for (size_t si = 0; si < scripts.size(); si++) {
            const QString label = scripts[si]["Label"].toString();
            if (!label.startsWith("ev_hide_item_")) continue;
            EditSession::ItemSpot spot;
            spot.hidden = true;
            const std::vector<UValue>& cmds = scripts[si]["Commands"].list();
            for (size_t ci = 0; ci < cmds.size(); ci++) {
                if (cmdId(cmds[ci]) != CmdLdval) continue;
                const std::vector<UValue>& args = cmds[ci]["Arg"].list();
                if (args.size() < 3 || args[1]["argType"].i != ArgWork || args[2]["argType"].i != ArgFloat) continue;
                const ScriptPos pos{"hide_item", static_cast<int>(si), static_cast<int>(ci), 2};
                if (args[1]["data"].i == kWorkHiddenItem) spot.item = pos;
                else if (args[1]["data"].i == kWorkHiddenCount) spot.count = pos;
            }
            if (spot.item.zone.isEmpty()) continue;
            spot.label = placeOf(label);
            s.itemSpots.append(spot);
        }
    }
}

// --------------------------------------------------------- Entwicklungslinien

QList<int> evolutionLine(const GameFiles& files, int base) {
    QHash<int, int> firstChild;
    for (int species = 1; species <= kMaxSpecies; species++) {
        const int index = files.personal.indexOf(species, 0);
        if (index <= 0) continue;
        for (const Evolution& e : files.evolutions.get(index)) {
            if (e.species <= 0 || e.species > kMaxSpecies) continue;
            if (!firstChild.contains(species)) firstChild.insert(species, e.species);
        }
    }
    QList<int> line{base};
    while (firstChild.contains(line.last()) && line.size() < 3) line.append(firstChild.value(line.last()));
    return line;
}

// Starter von Koffer-Platz slot aendern: Skript-Vergleiche und die Starter-Linie bei Rivale und Lucius/Lucia
void setStarter(EditSession& s, int slot, int species) {
    const int old = s.starters[slot];
    if (species <= 0 || species == old) return;
    for (const ScriptPos& p : s.starterPos[slot]) writeInt(s.scripts, p, species);
    const QList<int> oldLine = evolutionLine(s.work, old);
    const QList<int> newLine = evolutionLine(s.work, species);
    for (Trainer& t : s.work.trainers.all()) {
        if (!t.classLabel.contains("TR_RIVAL") && !t.classLabel.contains("TR_HERO")) continue;
        for (TrainerMon& mon : t.team) {
            const int stage = oldLine.indexOf(mon.species);
            if (stage < 0) continue;
            const int now = newLine.value(qMin(stage, static_cast<int>(newLine.size()) - 1));
            const int index = s.work.personal.indexOf(now, 0);
            mon.species = now;
            mon.form = 0;
            mon.sex = 3;
            mon.ability = s.work.personal.ability(index, 0);
            const QList<int> moves = s.smart.defaultMoveset(now, 0, mon.level);
            for (int k = 0; k < 4; k++) mon.moves[k] = k < moves.size() ? moves[k] : 0;
            mon.locked = true;
        }
    }
    s.starters[slot] = species;
}

// --------------------------------------------------------------- Felder

QStringList genderChoices() { return {"Männlich", "Weiblich", "Geschlechtslos", "Standard / zufällig"}; }

// Feld fuer ein Unterfeld einer UValue-Zeile (Zahl)
using RowFn = std::function<UValue&()>;
using ConstRowFn = std::function<const UValue&()>;

edit::Field valueNumber(const QString& label, int min, int max, RowFn row, ConstRowFn orig, const char* field) {
    const QByteArray name(field);
    return edit::number(label, min, max, [row, name]() { return static_cast<int>(row()[name.constData()].i); },
                        [row, name](int v) { row()[name.constData()].i = v; },
                        orig ? std::function<int()>([orig, name]() { return static_cast<int>(orig()[name.constData()].i); })
                             : std::function<int()>());
}

edit::Field valuePicker(const QString& label, edit::Kind kind, RowFn row, ConstRowFn orig, const char* field) {
    const QByteArray name(field);
    return edit::picker(label, kind, [row, name]() { return static_cast<int>(row()[name.constData()].i); },
                        [row, name](int v) { row()[name.constData()].i = v; },
                        orig ? std::function<int()>([orig, name]() { return static_cast<int>(orig()[name.constData()].i); })
                             : std::function<int()>());
}

edit::Field valueChoice(const QString& label, const QStringList& choices, RowFn row, ConstRowFn orig, const char* field) {
    const QByteArray name(field);
    return edit::choice(label, choices, [row, name]() { return static_cast<int>(row()[name.constData()].i); },
                        [row, name](int v) { row()[name.constData()].i = v; },
                        orig ? std::function<int()>([orig, name]() { return static_cast<int>(orig()[name.constData()].i); })
                             : std::function<int()>());
}

edit::Field valueCheck(const QString& label, RowFn row, ConstRowFn orig, const char* field) {
    const QByteArray name(field);
    return edit::check(label, [row, name]() { return row()[name.constData()].i != 0 ? 1 : 0; },
                       [row, name](int v) { row()[name.constData()].i = v ? 1 : 0; },
                       orig ? std::function<int()>([orig, name]() { return orig()[name.constData()].i != 0 ? 1 : 0; })
                            : std::function<int()>());
}

// Art + Form aus zwei Unterfeldern
edit::Field valuePokemon(const QString& label, RowFn row, ConstRowFn orig, const char* speciesField, const char* formField) {
    const QByteArray sp(speciesField), fm(formField ? formField : "");
    auto value = [sp, fm](const UValue& r) {
        return edit::pokemonValue(static_cast<int>(r[sp.constData()].i), fm.isEmpty() ? 0 : static_cast<int>(r[fm.constData()].i));
    };
    edit::Field f = edit::picker(label, edit::Kind::Pokemon, [row, value]() { return value(row()); },
                                 [row, sp, fm](int v) {
                                     if (edit::speciesOf(v) <= 0) return;
                                     row()[sp.constData()].i = edit::speciesOf(v);
                                     if (!fm.isEmpty()) row()[fm.constData()].i = edit::formOf(v);
                                 },
                                 orig ? std::function<int()>([orig, value]() { return value(orig()); }) : std::function<int()>());
    if (fm.isEmpty()) f.info = "Hier zählt nur die Art. Eine gewählte Form wird ignoriert.";
    return f;
}

QString pokemonText(const EditSession& s, int species, int form) { return s.texts.pokemonName(species, form); }

// ----------------------------------------------------------------- Trainer

edit::Section trainerSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Trainer";
    section.subtitle = "Alle Trainer mit ihren Teams. Fahre mit der Maus über ein Feld, um den Vorher-Wert zu sehen.";

    struct Item { int group; int index; };
    QList<Item> order;
    for (const Trainer& t : s->work.trainers.all()) {
        if (t.team.isEmpty()) continue;
        order.append({trainerGroup(s->work, t), t.index});
    }
    std::stable_sort(order.begin(), order.end(), [](const Item& a, const Item& b) { return a.group < b.group; });

    for (const Item& it : order) {
        const int ti = it.index;
        const Trainer* orig = trainerAt(s->original, ti);
        int highest = 0;
        for (const TrainerMon& m : (orig ? orig->team : s->work.trainers.all()[ti].team)) highest = qMax(highest, m.level);

        edit::Entry entry;
        entry.group = groupInfo(it.group).title;
        entry.title = s->texts.trainerLabel(s->work.trainers.all()[ti]) + "  ·  Lv. " + QString::number(highest);
        entry.search = [s, ti]() {
            QString text = "#" + QString::number(ti);
            for (const TrainerMon& m : s->work.trainers.all()[ti].team) text += " " + s->texts.pokemonName(m.species, m.form);
            return text;
        };
        entry.changed = [s, ti]() {
            const Trainer* o = trainerAt(s->original, ti);
            return o && !sameTrainer(s->work.trainers.all()[ti], *o);
        };
        entry.build = [s, ti]() {
            edit::Content c;
            Trainer& trainer = s->work.trainers.all()[ti];
            QStringList facts;
            facts << (trainer.fightType == 1 ? "Doppelkampf" : "Einzelkampf");
            const int theme = trainerTheme(s->work, trainer);
            if (theme >= 0) facts << "Typ-Trainer: " + s->texts.typeName(theme);
            const QStringList zones = s->work.trainerZones.value(ti);
            if (!zones.isEmpty()) facts << "Karte: " + zones.join(", ");
            facts << "#" + QString::number(ti);
            c.hint = facts.join(" · ");
            if (trainer.classLabel.contains("TR_RIVAL") || trainer.classLabel.contains("TR_HERO")) {
                c.hint += "\nRivale bzw. Lucius/Lucia: wenn du einen Starter änderst, wird die Starter-Linie hier automatisch mitgetauscht.";
            }
            c.warnings = [s, ti]() {
                QStringList w;
                QHash<int, int> count;
                for (const TrainerMon& m : s->work.trainers.all()[ti].team) count[m.species]++;
                for (auto i = count.constBegin(); i != count.constEnd(); ++i)
                    if (i.value() > 1) w << s->texts.pokemonName(i.key(), 0) + " ist mehrfach im Team.";
                w.sort();
                return w;
            };

            edit::Table team;
            team.title = "Team";
            team.cards = true;
            team.columnsPerLine = 4;
            team.rowCount = [s, ti]() { return static_cast<int>(s->work.trainers.all()[ti].team.size()); };
            team.rowTitle = [s, ti](int r) {
                const TrainerMon& m = s->work.trainers.all()[ti].team[r];
                return QString("Pokémon %1 – %2").arg(r + 1).arg(s->texts.pokemonName(m.species, m.form));
            };
            team.canAdd = [s, ti]() { return s->work.trainers.all()[ti].team.size() < 6; };
            team.add = [s, ti]() {
                QList<TrainerMon>& list = s->work.trainers.all()[ti].team;
                if (list.size() < 6 && !list.isEmpty()) list.append(list.last());
            };
            team.canRemove = [s, ti]() { return s->work.trainers.all()[ti].team.size() > 1; };
            team.remove = [s, ti](int r) {
                QList<TrainerMon>& list = s->work.trainers.all()[ti].team;
                if (list.size() > 1 && r >= 0 && r < list.size()) list.removeAt(r);
            };
            team.row = [s, ti](int r) {
                auto mon = [s, ti, r]() -> TrainerMon& { return s->work.trainers.all()[ti].team[r]; };
                const Trainer* o = trainerAt(s->original, ti);
                const bool hasOrig = o && r < o->team.size();
                auto orig = [s, ti, r, hasOrig](std::function<int(const TrainerMon&)> fn) -> std::function<int()> {
                    if (!hasOrig) return {};
                    return [s, ti, r, fn]() { return fn(s->original.trainers.all()[ti].team[r]); };
                };
                QList<edit::Field> f;
                edit::Field poke = edit::picker("Pokémon", edit::Kind::Pokemon,
                    [mon]() { return edit::pokemonValue(mon().species, mon().form); },
                    [mon, s](int v) {
                        if (edit::speciesOf(v) <= 0) return;
                        mon().species = edit::speciesOf(v);
                        mon().form = edit::formOf(v);
                        // Faehigkeit passend zur neuen Art (Faehigkeit 1)
                        const int index = s->work.personal.indexOf(mon().species, mon().form);
                        if (index > 0) mon().ability = s->work.personal.ability(index, 0);
                    },
                    orig([](const TrainerMon& m) { return edit::pokemonValue(m.species, m.form); }));
                poke.rebuild = true;
                poke.span = 2;
                f << poke;
                f << edit::number("Level", 1, 100, [mon]() { return mon().level; }, [mon](int v) { mon().level = v; },
                                  orig([](const TrainerMon& m) { return m.level; }));
                f << edit::check("Schillernd", [mon]() { return mon().shiny ? 1 : 0; }, [mon](int v) { mon().shiny = v != 0; },
                                 orig([](const TrainerMon& m) { return m.shiny ? 1 : 0; }));
                for (int k = 0; k < 4; k++) {
                    f << edit::picker(QString("Attacke %1").arg(k + 1), edit::Kind::Move, [mon, k]() { return mon().moves[k]; },
                                      [mon, k](int v) { mon().moves[k] = v; },
                                      orig([k](const TrainerMon& m) { return m.moves[k]; }));
                }
                f << edit::picker("Item", edit::Kind::Item, [mon]() { return mon().item; }, [mon](int v) { mon().item = v; },
                                  orig([](const TrainerMon& m) { return m.item; }));
                f << edit::picker("Fähigkeit", edit::Kind::Ability, [mon]() { return mon().ability; },
                                  [mon](int v) { mon().ability = v; }, orig([](const TrainerMon& m) { return m.ability; }));
                f << edit::picker("Wesen", edit::Kind::Nature, [mon]() { return mon().nature; }, [mon](int v) { mon().nature = v; },
                                  orig([](const TrainerMon& m) { return m.nature; }));
                f << edit::choice("Geschlecht", genderChoices(), [mon]() { return qBound(0, mon().sex, 3); },
                                  [mon](int v) { mon().sex = v; }, orig([](const TrainerMon& m) { return qBound(0, m.sex, 3); }));
                edit::Field ball = edit::number("Ball (Nr.)", 0, 30, [mon]() { return mon().ball; }, [mon](int v) { mon().ball = v; },
                                                orig([](const TrainerMon& m) { return m.ball; }));
                f << ball;
                for (int k = 0; k < 6; k++) {
                    const int st = kStatOrder[k];
                    f << edit::number("DV " + kStatNames[k], 0, 31, [mon, st]() { return mon().ivs[st]; },
                                      [mon, st](int v) { mon().ivs[st] = v; },
                                      orig([st](const TrainerMon& m) { return m.ivs[st]; }));
                }
                for (int k = 0; k < 6; k++) {
                    const int st = kStatOrder[k];
                    f << edit::number("FP " + kStatNames[k], 0, 252, [mon, st]() { return mon().evs[st]; },
                                      [mon, st](int v) { mon().evs[st] = v; },
                                      orig([st](const TrainerMon& m) { return m.evs[st]; }));
                }
                return f;
            };
            c.tables << team;
            return c;
        };
        section.entries.append(entry);
    }
    return section;
}

// ----------------------------------------------------------- Wilde Pokemon

struct SlotList { const char* name; const char* title; };
const SlotList kSlotLists[] = {
    {"ground_mons", "Gras / Höhle"}, {"tairyo", "Schwarm"}, {"day", "Tag"}, {"night", "Nacht"},
    {"swayGrass", "Poké-Radar"}, {"gbaRuby", "GBA-Slot Rubin"}, {"gbaSapp", "GBA-Slot Saphir"},
    {"gbaEme", "GBA-Slot Smaragd"}, {"gbaFire", "GBA-Slot Feuerrot"}, {"gbaLeaf", "GBA-Slot Blattgrün"},
    {"water_mons", "Surfen"}, {"boro_mons", "Angel"}, {"ii_mons", "Profi-Angel"}, {"sugoi_mons", "Super-Angel"}};

// Icognito-Raum: Buchstaben-Tabelle nur, wenn im Gebiet noch Icognito vorkommt (wie beim Randomizer)
void fixUnown(UValue& zone, const UValue& original) {
    std::vector<UValue>& annoon = zone["AnnoonTable"].list();
    const std::vector<UValue>& before = original["AnnoonTable"].list();
    if (annoon.size() < 2 || before.size() < 2 || before[1].i == 0) return;
    bool unown = false;
    for (const SlotList& l : kSlotLists) {
        for (const UValue& slot : zone[l.name].list()) unown |= slot["monsNo"].i == 201;
    }
    annoon[1].i = unown ? before[1].i : 0;
}

edit::Section wildSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Wilde Pokémon";
    section.subtitle = "Alle Gebiete beider Editionen mit ihren Listen (Gras, Tag/Nacht, Surfen, Angeln …), Honigbäume, "
                       "Großes Moor, Trophäengarten und Untergrund.";
    for (const QString& table : {QString("FieldEncountTable_d"), QString("FieldEncountTable_p")}) {
        if (!s->field.has(table) || !s->fieldOriginal.has(table)) continue;
        const QString edition = table.endsWith("_d") ? "Strahlender Diamant" : "Leuchtende Perle";
        const std::vector<UValue>& zones = s->fieldOriginal.table(table)["table"].list();
        QHash<QString, int> seen;
        for (size_t z = 0; z < zones.size(); z++) {
            const int zoneId = zones[z]["zoneID"].toInt();
            if (zoneId < 0) continue;
            bool any = false;
            for (const SlotList& l : kSlotLists)
                for (const UValue& slot : zones[z][l.name].list()) any |= slot["monsNo"].i > 0;
            if (!any) continue;
            QString place = s->zoneNames.value(zoneId, QString("Zone %1").arg(zoneId));
            const int n = ++seen[place];
            if (n > 1) place += QString(" (%1)").arg(n);
            auto zoneRef = [s, table, z]() -> UValue& { return s->field.table(table)["table"].list()[z]; };
            auto zoneOrig = [s, table, z]() -> const UValue& { return s->fieldOriginal.table(table)["table"].list()[z]; };
            edit::Entry entry;
            entry.group = edition;
            entry.title = place;
            entry.search = [s, zoneRef]() {
                QString text;
                for (const SlotList& l : kSlotLists)
                    for (const UValue& slot : zoneRef()[l.name].list())
                        if (slot["monsNo"].i > 0) text += " " + s->texts.pokemonName(static_cast<int>(slot["monsNo"].i), 0);
                return text;
            };
            entry.changed = [zoneRef, zoneOrig]() { return !sameValue(zoneRef(), zoneOrig()); };
            entry.build = [s, zoneRef, zoneOrig, zoneId]() {
                edit::Content c;
                c.hint = QString("Zone %1. Leere Plätze sind ausgeblendet.").arg(zoneId);
                for (const SlotList& l : kSlotLists) {
                    const std::vector<UValue>& before = zoneOrig()[l.name].list();
                    QList<int> rows;
                    for (size_t k = 0; k < before.size(); k++) if (before[k]["monsNo"].i > 0) rows.append(static_cast<int>(k));
                    if (rows.isEmpty()) continue;
                    const QByteArray list(l.name);
                    edit::Table t;
                    t.title = l.title;
                    t.columns = {"Pokémon", "Min. Level", "Max. Level"};
                    t.rowCount = [rows]() { return static_cast<int>(rows.size()); };
                    t.row = [zoneRef, zoneOrig, rows, list](int r) {
                        const size_t k = static_cast<size_t>(rows[r]);
                        RowFn row = [zoneRef, list, k]() -> UValue& { return zoneRef()[list.constData()].list()[k]; };
                        ConstRowFn orig = [zoneOrig, list, k]() -> const UValue& { return zoneOrig()[list.constData()].list()[k]; };
                        edit::Field p = valuePokemon("Pokémon", row, orig, "monsNo", nullptr);
                        const auto baseSet = p.set;
                        p.set = [baseSet, zoneRef, zoneOrig](int v) {
                            baseSet(v);
                            fixUnown(zoneRef(), zoneOrig());
                        };
                        return QList<edit::Field>{p, valueNumber("Min. Level", 1, 100, row, orig, "minlv"),
                                                  valueNumber("Max. Level", 1, 100, row, orig, "maxlv")};
                    };
                    c.tables << t;
                }
                return c;
            };
            section.entries.append(entry);
        }

        // Honigbaeume, Grosses Moor, Trophaeengarten
        struct Special { const char* list; QString title; QList<QPair<QByteArray, QString>> columns; QString hint; };
        const QList<Special> specials = {
            {"mistu", "Honigbäume", {{"Normal", "Normal"}, {"Rare", "Selten"}, {"SuperRare", "Sehr selten"}},
             "Jede Zeile ist eine Gruppe von Honigbäumen."},
            {"safari", "Großes Moor (Safari)", {{"MonsNo", "Pokémon"}}, "Pokémon im Großen Moor (je nach Tag und Bereich)."},
            {"urayama", "Trophäengarten", {{"monsNo", "Pokémon"}}, "Täglich wechselnde Pokémon im Trophäengarten."}};
        for (const Special& sp : specials) {
            const QByteArray list(sp.list);
            if (s->fieldOriginal.table(table)[sp.list].list().empty()) continue;
            auto tableRef = [s, table, list]() -> std::vector<UValue>& { return s->field.table(table)[list.constData()].list(); };
            auto tableOrig = [s, table, list]() -> const std::vector<UValue>& {
                return s->fieldOriginal.table(table)[list.constData()].list();
            };
            edit::Entry entry;
            entry.group = edition + " – Besondere Orte";
            entry.title = sp.title;
            const auto columns = sp.columns;
            entry.search = [s, tableRef, columns]() {
                QString text;
                for (const UValue& row : tableRef())
                    for (const auto& c : columns) text += " " + s->texts.pokemonName(static_cast<int>(row[c.first.constData()].i), 0);
                return text;
            };
            entry.changed = [tableRef, tableOrig]() {
                const std::vector<UValue>& a = tableRef();
                const std::vector<UValue>& b = tableOrig();
                if (a.size() != b.size()) return true;
                for (size_t k = 0; k < a.size(); k++) if (!sameValue(a[k], b[k])) return true;
                return false;
            };
            const QString hint = sp.hint;
            entry.build = [tableRef, tableOrig, columns, hint]() {
                edit::Content c;
                c.hint = hint;
                edit::Table t;
                t.title = "Pokémon";
                for (const auto& col : columns) t.columns << col.second;
                const int rows = static_cast<int>(tableOrig().size());
                t.rowCount = [rows]() { return rows; };
                t.row = [tableRef, tableOrig, columns](int r) {
                    const size_t k = static_cast<size_t>(r);
                    RowFn row = [tableRef, k]() -> UValue& { return tableRef()[k]; };
                    ConstRowFn orig = [tableOrig, k]() -> const UValue& { return tableOrig()[k]; };
                    QList<edit::Field> f;
                    for (const auto& col : columns) {
                        edit::Field p = valuePokemon(col.second, row, orig, col.first.constData(), nullptr);
                        f << p;
                    }
                    return f;
                };
                c.tables << t;
                return c;
            };
            section.entries.append(entry);
        }
    }

    // Untergrund
    if (s->ug.has("UgSpecialPokemon") && s->ugOriginal.has("UgSpecialPokemon")) {
        const QStringList versions = {"—", "Diamant", "Perle", "Beide"};
        for (const QString& name : s->ugOriginal.tableNames()) {
            if (!name.startsWith("UgEncount_") || name == "UgEncountLevel") continue;
            if (s->ugOriginal.table(name)["table"].list().empty()) continue;
            edit::Entry entry;
            entry.group = "Untergrund";
            entry.title = "Höhlen-Tabelle " + name.mid(10);
            entry.search = [s, name]() {
                QString text;
                for (const UValue& row : s->ug.table(name)["table"].list())
                    text += " " + s->texts.pokemonName(static_cast<int>(row["monsno"].i), 0);
                return text;
            };
            entry.changed = [s, name]() { return !sameTable(s->ug, s->ugOriginal, name); };
            entry.build = [s, name]() {
                edit::Content c;
                c.hint = "Welche Pokémon in den Höhlen des Untergrunds erscheinen können (Level steigt mit den Orden).";
                edit::Table t;
                t.title = "Pokémon";
                t.columns = {"Pokémon", "Edition (1 D, 2 P, 3 beide)"};
                const int rows = static_cast<int>(s->ugOriginal.table(name)["table"].list().size());
                t.rowCount = [rows]() { return rows; };
                t.row = [s, name](int r) {
                    const size_t k = static_cast<size_t>(r);
                    RowFn row = [s, name, k]() -> UValue& { return s->ug.table(name)["table"].list()[k]; };
                    ConstRowFn orig = [s, name, k]() -> const UValue& { return s->ugOriginal.table(name)["table"].list()[k]; };
                    return QList<edit::Field>{valuePokemon("Pokémon", row, orig, "monsno", nullptr),
                                              valueNumber("Edition", 0, 3, row, orig, "version")};
                };
                c.tables << t;
                return c;
            };
            section.entries.append(entry);
        }
        // Seltene Pokemon je Hoehlen-Art
        QMap<int, QList<int>> byId;
        const std::vector<UValue>& special = s->ugOriginal.table("UgSpecialPokemon")["Sheet1"].list();
        for (size_t k = 0; k < special.size(); k++) byId[special[k]["id"].toInt()].append(static_cast<int>(k));
        for (auto it = byId.constBegin(); it != byId.constEnd(); ++it) {
            const QList<int> rows = it.value();
            edit::Entry entry;
            entry.group = "Untergrund";
            entry.title = QString("Seltene Pokémon %1").arg(it.key());
            entry.search = [s, rows]() {
                QString text;
                for (int r : rows)
                    text += " " + s->texts.pokemonName(
                        static_cast<int>(s->ug.table("UgSpecialPokemon")["Sheet1"].list()[static_cast<size_t>(r)]["monsno"].i), 0);
                return text;
            };
            entry.changed = [s, rows]() {
                for (int r : rows) {
                    if (!sameValue(s->ug.table("UgSpecialPokemon")["Sheet1"].list()[static_cast<size_t>(r)],
                                   s->ugOriginal.table("UgSpecialPokemon")["Sheet1"].list()[static_cast<size_t>(r)])) return true;
                }
                return false;
            };
            entry.build = [s, rows]() {
                edit::Content c;
                c.hint = "Seltene Pokémon einer Höhlen-Art im Untergrund mit ihren Chancen je Edition.";
                edit::Table t;
                t.title = "Pokémon";
                t.columns = {"Pokémon", "Edition", "Chance Diamant", "Chance Perle"};
                t.rowCount = [rows]() { return static_cast<int>(rows.size()); };
                t.row = [s, rows](int r) {
                    const size_t k = static_cast<size_t>(rows[r]);
                    RowFn row = [s, k]() -> UValue& { return s->ug.table("UgSpecialPokemon")["Sheet1"].list()[k]; };
                    ConstRowFn orig = [s, k]() -> const UValue& { return s->ugOriginal.table("UgSpecialPokemon")["Sheet1"].list()[k]; };
                    return QList<edit::Field>{valuePokemon("Pokémon", row, orig, "monsno", nullptr),
                                              valueNumber("Edition", 0, 3, row, orig, "version"),
                                              valueNumber("Chance Diamant", 0, 10000, row, orig, "Dspecialrate"),
                                              valueNumber("Chance Perle", 0, 10000, row, orig, "Pspecialrate")};
                };
                c.tables << t;
                return c;
            };
            section.entries.append(entry);
        }
    }
    return section;
}

// ---------------------------------------------- Starter, Geschenke, Begegnungen

edit::Section encounterSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Starter & Geschenke";
    section.subtitle = "Starter, Evoli von Bebe, Eier, Legendäre und feste Begegnungen sowie Tauschpartner.";

    // Starter
    static const QStringList slotNames = {"Koffer links (Original Chelast)", "Koffer Mitte (Original Panflam)",
                                          "Koffer rechts (Original Plinfa)"};
    {
        edit::Entry entry;
        entry.group = "Starter";
        entry.title = "Starter im Koffer";
        entry.search = [s]() {
            QString text;
            for (int k = 0; k < 3; k++) text += " " + s->texts.pokemonName(s->starters[k], 0);
            return text;
        };
        entry.changed = [s]() {
            for (int k = 0; k < 3; k++) if (s->starters[k] != kOriginalStarters[k]) return true;
            return false;
        };
        entry.build = [s]() {
            edit::Content c;
            c.fieldsPerLine = 3;
            c.hint = "Die Starter stehen im Programmcode (exefs-Patch). Rivale und Lucius/Lucia bekommen automatisch die "
                     "passende Starter-Linie.";
            for (int k = 0; k < 3; k++) {
                edit::Field f = edit::picker(slotNames[k], edit::Kind::Pokemon, [s, k]() { return edit::pokemonValue(s->starters[k], 0); },
                                             [s, k](int v) { setStarter(*s, k, edit::speciesOf(v)); },
                                             [k]() { return edit::pokemonValue(kOriginalStarters[k], 0); });
                f.info = "Nur die Art zählt (Level 5).";
                c.fields << f;
            }
            c.warnings = [s]() {
                QStringList w;
                if (s->starters[0] == s->starters[1] || s->starters[0] == s->starters[2] || s->starters[1] == s->starters[2]) {
                    w << "Zwei Starter sind gleich.";
                }
                if (s->exefs.isEmpty()) w << "Ohne ExeFS-Ordner kann der Starter-Patch nicht geschrieben werden.";
                return w;
            };
            return c;
        };
        section.entries.append(entry);
    }

    // Geschenke: Evoli und Eier
    {
        edit::Entry entry;
        entry.group = "Geschenkte Pokémon";
        entry.title = "Evoli von Bebe (Herzhofen)";
        entry.search = [s]() { return s->texts.pokemonName(s->eevee, 0); };
        entry.changed = [s]() { return s->eevee != 133; };
        entry.build = [s]() {
            edit::Content c;
            c.hint = "Steht im Programmcode (exefs-Patch). Level 5.";
            c.fields << edit::picker("Pokémon", edit::Kind::Pokemon, [s]() { return edit::pokemonValue(s->eevee, 0); },
                                     [s](int v) { if (edit::speciesOf(v) > 0) s->eevee = edit::speciesOf(v); },
                                     []() { return edit::pokemonValue(133, 0); });
            return c;
        };
        section.entries.append(entry);
    }
    for (int e = 0; e < s->eggPos.size(); e++) {
        const ScriptPos pos = s->eggPos[e];
        edit::Entry entry;
        entry.group = "Geschenkte Pokémon";
        entry.title = "Ei · " + placeForZone(pos.zone);
        entry.search = [s, pos]() { return s->texts.pokemonName(readInt(s->scripts, pos), 0); };
        entry.changed = [s, pos]() { return readInt(s->scripts, pos) != readInt(s->scriptsOriginal, pos); };
        entry.build = [s, pos]() {
            edit::Content c;
            c.hint = "Aus dem Ei schlüpft dieses Pokémon (Level 1). Am besten ein Basis-Pokémon.";
            c.fields << edit::picker("Pokémon", edit::Kind::Pokemon, [s, pos]() { return edit::pokemonValue(readInt(s->scripts, pos), 0); },
                                     [s, pos](int v) { if (edit::speciesOf(v) > 0) writeInt(s->scripts, pos, edit::speciesOf(v)); },
                                     [s, pos]() { return edit::pokemonValue(readInt(s->scriptsOriginal, pos), 0); });
            return c;
        };
        section.entries.append(entry);
    }

    // Statische Begegnungen
    for (int i = 0; i < s->statics.size(); i++) {
        const EditSession::Static& st = s->statics[i];
        if (st.battle.isEmpty()) continue;
        edit::Entry entry;
        entry.group = isLegendary(st.species) ? "Legendäre Pokémon" : "Feste Begegnungen";
        entry.title = pokemonText(*s, st.species, 0) + "  ·  " + st.places.join(", ");
        entry.search = [s, i]() { return s->texts.pokemonName(readInt(s->scripts, s->statics[i].battle.first()), 0); };
        entry.changed = [s, i]() {
            for (const ScriptPos& p : s->statics[i].positions)
                if (readInt(s->scripts, p) != readInt(s->scriptsOriginal, p)) return true;
            for (const ScriptPos& p : s->statics[i].battle) {
                const ScriptPos level{p.zone, p.script, p.command, 2};
                if (readInt(s->scripts, level) != readInt(s->scriptsOriginal, level)) return true;
            }
            return false;
        };
        entry.build = [s, i]() {
            edit::Content c;
            const EditSession::Static& st = s->statics[i];
            c.hint = "Orte: " + st.places.join(", ") + ". Kampf, Ruf, Name und Prüfungen in den Skripten werden gemeinsam geändert.";
            c.fields << edit::picker("Pokémon", edit::Kind::Pokemon,
                                     [s, i]() { return edit::pokemonValue(readInt(s->scripts, s->statics[i].battle.first()), 0); },
                                     [s, i](int v) {
                                         if (edit::speciesOf(v) <= 0) return;
                                         for (const ScriptPos& p : s->statics[i].positions) writeInt(s->scripts, p, edit::speciesOf(v));
                                     },
                                     [s, i]() { return edit::pokemonValue(s->statics[i].species, 0); });
            const ScriptPos first = st.battle.first();
            const ScriptPos levelPos{first.zone, first.script, first.command, 2};
            c.fields << edit::number("Level", 1, 100, [s, levelPos]() { return readInt(s->scripts, levelPos); },
                                     [s, i](int v) {
                                         for (const ScriptPos& p : s->statics[i].battle) writeInt(s->scripts, {p.zone, p.script, p.command, 2}, v);
                                     },
                                     [s, levelPos]() { return readInt(s->scriptsOriginal, levelPos); });
            c.warnings = [s, i]() {
                QStringList w;
                const int now = readInt(s->scripts, s->statics[i].battle.first());
                const int before = s->statics[i].species;
                if (now != before && isLegendary(before) && !isLegendary(now)) {
                    w << "Im Original ist hier ein Legendäres. Der Randomizer setzt hier immer wieder ein Legendäres ein.";
                }
                if (now != before && (before == 483 || before == 484 || before == 487)) {
                    w << "Story-Begegnung: Zwischensequenzen zeigen weiterhin das alte Pokémon.";
                }
                return w;
            };
            return c;
        };
        section.entries.append(entry);
    }

    // Tausch: Zeilen mit gleichem name_label (alle Sprachen) gemeinsam
    const std::vector<UValue>& trades = masterTable(s->original, "LocalKoukanData")["data"].list();
    QStringList labels;
    QHash<QString, QList<int>> rowsOf;
    for (size_t k = 0; k < trades.size(); k++) {
        const QString label = trades[k]["name_label"].toString();
        if (trades[k]["monsno"].i <= 0) continue;
        if (!rowsOf.contains(label)) labels.append(label);
        rowsOf[label].append(static_cast<int>(k));
    }
    for (const QString& label : labels) {
        const QList<int> rows = rowsOf.value(label);
        const int first = rows.first();
        auto rowRef = [s, first]() -> UValue& { return s->work.masterBundle.table("LocalKoukanData")["data"].list()[static_cast<size_t>(first)]; };
        auto rowOrig = [s, first]() -> const UValue& {
            return s->original.masterBundle.table("LocalKoukanData")["data"].list()[static_cast<size_t>(first)];
        };
        // Wert in alle Zeilen dieses Tauschpartners schreiben
        auto setAll = [s, rows](const char* field, int value) {
            std::vector<UValue>& data = s->work.masterBundle.table("LocalKoukanData")["data"].list();
            for (int r : rows) data[static_cast<size_t>(r)][field].i = value;
        };
        auto setWaza = [s, rows](int slot, int value) {
            std::vector<UValue>& data = s->work.masterBundle.table("LocalKoukanData")["data"].list();
            for (int r : rows) {
                std::vector<UValue>& waza = data[static_cast<size_t>(r)]["waza"].list();
                if (slot < static_cast<int>(waza.size())) waza[static_cast<size_t>(slot)].i = value;
            }
        };
        edit::Entry entry;
        entry.group = "Tausch";
        entry.title = pokemonText(*s, static_cast<int>(trades[static_cast<size_t>(first)]["monsno"].i), 0) + " für " +
                      pokemonText(*s, static_cast<int>(trades[static_cast<size_t>(first)]["target"].i), 0);
        entry.search = [s, rowRef]() { return s->texts.pokemonName(static_cast<int>(rowRef()["monsno"].i), 0); };
        entry.changed = [s, rows]() {
            const std::vector<UValue>& a = s->work.masterBundle.table("LocalKoukanData")["data"].list();
            const std::vector<UValue>& b = s->original.masterBundle.table("LocalKoukanData")["data"].list();
            for (int r : rows) if (!sameValue(a[static_cast<size_t>(r)], b[static_cast<size_t>(r)])) return true;
            return false;
        };
        entry.build = [rowRef, rowOrig, setAll, setWaza]() {
            edit::Content c;
            c.fieldsPerLine = 4;
            c.hint = "Gilt für alle Sprachen dieses Tauschpartners.";
            auto num = [&](const QString& label, int min, int max, const char* field) {
                const QByteArray name(field);
                return edit::number(label, min, max, [rowRef, name]() { return static_cast<int>(rowRef()[name.constData()].i); },
                                    [setAll, name](int v) { setAll(name.constData(), v); },
                                    [rowOrig, name]() { return static_cast<int>(rowOrig()[name.constData()].i); });
            };
            auto pick = [&](const QString& label, edit::Kind kind, const char* field) {
                const QByteArray name(field);
                return edit::picker(label, kind, [rowRef, name]() { return static_cast<int>(rowRef()[name.constData()].i); },
                                    [setAll, name, kind](int v) {
                                        setAll(name.constData(), kind == edit::Kind::Pokemon ? edit::speciesOf(v) : v);
                                    },
                                    [rowOrig, name]() { return static_cast<int>(rowOrig()[name.constData()].i); });
            };
            c.fields << pick("Pokémon (bekommst du)", edit::Kind::Pokemon, "monsno");
            c.fields << pick("Gewünschtes Pokémon", edit::Kind::Pokemon, "target");
            c.fields << num("Level", 1, 100, "level");
            c.fields << pick("Item", edit::Kind::Item, "itemno");
            c.fields << pick("Wesen", edit::Kind::Nature, "seikaku");
            c.fields << edit::choice("Geschlecht", genderChoices(), [rowRef]() { return qBound(0, static_cast<int>(rowRef()["sex"].i), 3); },
                                     [setAll](int v) { setAll("sex", v); },
                                     [rowOrig]() { return qBound(0, static_cast<int>(rowOrig()["sex"].i), 3); });
            edit::Field ability = num("Fähigkeit (Nr.)", 0, 3, "tokusei");
            ability.info = "Fähigkeit als Zahl, wie im Spiel gespeichert (im Original 0).";
            c.fields << ability;
            for (int k = 0; k < 4; k++) {
                c.fields << edit::picker(QString("Attacke %1").arg(k + 1), edit::Kind::Move,
                    [rowRef, k]() { return static_cast<int>(rowRef()["waza"].list()[static_cast<size_t>(k)].i); },
                    [setWaza, k](int v) { setWaza(k, v); },
                    [rowOrig, k]() { return static_cast<int>(rowOrig()["waza"].list()[static_cast<size_t>(k)].i); });
            }
            return c;
        };
        section.entries.append(entry);
    }
    return section;
}

// ------------------------------------------------------------------ Items

struct ItemPocketInfo {
    QHash<int, int> pocket;
    void load(const UValue& table) {
        for (const UValue& item : table["Item"].list()) pocket.insert(item["no"].toInt(), item["fld_pocket"].toInt());
    }
    bool isProtected(int id) const {
        const int p = pocket.value(id, 9);
        return p == 5 || p == 8 || p == 9; // TMs, Basis-Items, ungenutzt
    }
};

edit::Section itemSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Items";
    section.subtitle = "Items auf dem Boden, versteckte Items und die Angebote der Läden.";
    auto pockets = std::make_shared<ItemPocketInfo>();
    pockets->load(s->original.personalBundle.table("ItemTable"));

    // Boden und versteckt, pro Ort
    for (int hidden = 0; hidden <= 1; hidden++) {
        QStringList places;
        QHash<QString, QList<int>> byPlace;
        for (int i = 0; i < s->itemSpots.size(); i++) {
            if (s->itemSpots[i].hidden != (hidden == 1)) continue;
            const QString place = s->itemSpots[i].label;
            if (!byPlace.contains(place)) places.append(place);
            byPlace[place].append(i);
        }
        for (const QString& place : places) {
            const QList<int> indexes = byPlace.value(place);
            edit::Entry entry;
            entry.group = hidden ? "Versteckte Items" : "Items auf dem Boden";
            entry.title = place;
            entry.search = [s, indexes]() {
                QString text;
                for (int i : indexes) text += " " + s->texts.itemName(readInt(s->scripts, s->itemSpots[i].item));
                return text;
            };
            entry.changed = [s, indexes]() {
                for (int i : indexes) {
                    const EditSession::ItemSpot& spot = s->itemSpots[i];
                    if (readInt(s->scripts, spot.item) != readInt(s->scriptsOriginal, spot.item)) return true;
                    if (!spot.count.zone.isEmpty() && readInt(s->scripts, spot.count) != readInt(s->scriptsOriginal, spot.count)) return true;
                }
                return false;
            };
            entry.build = [s, indexes, hidden, pockets]() {
                edit::Content c;
                c.hint = hidden ? "Versteckte Items (mit dem Itemradar oder durch Glitzern zu finden)." : "Items in Poké-Bällen auf dem Boden.";
                c.warnings = [s, indexes, pockets]() {
                    QStringList w;
                    for (int i : indexes) {
                        const int before = readInt(s->scriptsOriginal, s->itemSpots[i].item);
                        if (readInt(s->scripts, s->itemSpots[i].item) != before && pockets->isProtected(before)) {
                            w << "Hier lag im Original " + s->texts.itemName(before) +
                                     " (TM oder Basis-Item). Ohne es fehlt es dir eventuell im Spiel.";
                        }
                    }
                    return w;
                };
                edit::Table t;
                t.title = "Items";
                t.columns = {"Item", "Anzahl"};
                t.rowCount = [indexes]() { return static_cast<int>(indexes.size()); };
                t.row = [s, indexes](int r) {
                    const EditSession::ItemSpot spot = s->itemSpots[indexes[r]];
                    QList<edit::Field> f;
                    f << edit::picker("Item", edit::Kind::Item, [s, spot]() { return readInt(s->scripts, spot.item); },
                                      [s, spot](int v) { if (v > 0) writeInt(s->scripts, spot.item, v); },
                                      [s, spot]() { return readInt(s->scriptsOriginal, spot.item); });
                    if (spot.count.zone.isEmpty()) {
                        f << edit::text("Anzahl", []() { return QString("1"); });
                    } else {
                        f << edit::number("Anzahl", 1, 99, [s, spot]() { return readInt(s->scripts, spot.count); },
                                          [s, spot](int v) { writeInt(s->scripts, spot.count, v); },
                                          [s, spot]() { return readInt(s->scriptsOriginal, spot.count); });
                    }
                    return f;
                };
                c.tables << t;
                return c;
            };
            section.entries.append(entry);
        }
    }

    // Laeden: Pokemon-Center je Zone (FS), feste Laeden (FixedShop), Kampfzone (BPShop)
    struct Shop { QString list; QString key; int value; QString title; QString hint; };
    QList<Shop> shops;
    const UValue& shopTable = s->original.masterBundle.table("ShopTable");
    {
        QList<int> zones;
        for (const UValue& e : shopTable["FS"].list()) if (!zones.contains(e["ZoneID"].toInt())) zones.append(e["ZoneID"].toInt());
        for (int z : zones) {
            shops.append({"FS", "ZoneID", z,
                          z < 0 ? QString("Pokémon-Center (Grundsortiment)")
                                : "Pokémon-Center " + s->zoneNames.value(z, QString("Zone %1").arg(z)),
                          "Pokémon-Center-Laden. „Orden“ = ab wie vielen Orden das Angebot erscheint."});
        }
        QList<int> ids;
        for (const UValue& e : shopTable["FixedShop"].list()) if (!ids.contains(e["ShopID"].toInt())) ids.append(e["ShopID"].toInt());
        for (int id : ids) shops.append({"FixedShop", "ShopID", id, fixedShopName(id) + QString(" (#%1)").arg(id), "Fester Laden."});
        QList<int> npcs;
        for (const UValue& e : shopTable["BPShop"].list()) if (!npcs.contains(e["NPCID"].toInt())) npcs.append(e["NPCID"].toInt());
        for (int n : npcs) shops.append({"BPShop", "NPCID", n, QString("Kampfzone: BP-Laden %1").arg(n + 1), "Items gegen Kampfpunkte (BP)."});
    }
    for (const Shop& shop : shops) {
        const QByteArray list = shop.list.toUtf8(), key = shop.key.toUtf8();
        QList<int> rows;
        const std::vector<UValue>& entries = shopTable[list.constData()].list();
        for (size_t k = 0; k < entries.size(); k++) if (entries[k][key.constData()].toInt() == shop.value) rows.append(static_cast<int>(k));
        auto row = [s, list](int k) -> UValue& { return s->work.masterBundle.table("ShopTable")[list.constData()].list()[static_cast<size_t>(k)]; };
        auto orig = [s, list](int k) -> const UValue& {
            return s->original.masterBundle.table("ShopTable")[list.constData()].list()[static_cast<size_t>(k)];
        };
        edit::Entry entry;
        entry.group = "Läden";
        entry.title = shop.title;
        entry.search = [s, rows, row]() {
            QString text;
            for (int k : rows) text += " " + s->texts.itemName(static_cast<int>(row(k)["ItemNo"].i));
            return text;
        };
        entry.changed = [rows, row, orig]() {
            for (int k : rows) if (!sameValue(row(k), orig(k))) return true;
            return false;
        };
        const QString hint = shop.hint;
        const bool badges = shop.list == "FS";
        entry.build = [rows, row, orig, hint, badges]() {
            edit::Content c;
            c.hint = hint;
            edit::Table t;
            t.title = "Angebote";
            t.columns = badges ? QStringList{"Item", "Orden"} : QStringList{"Item"};
            t.rowCount = [rows]() { return static_cast<int>(rows.size()); };
            t.row = [rows, row, orig, badges](int r) {
                const int k = rows[r];
                RowFn rf = [row, k]() -> UValue& { return row(k); };
                ConstRowFn of = [orig, k]() -> const UValue& { return orig(k); };
                QList<edit::Field> f{valuePicker("Item", edit::Kind::Item, rf, of, "ItemNo")};
                if (badges) f << valueNumber("Orden", 0, 8, rf, of, "BadgeNum");
                return f;
            };
            c.tables << t;
            return c;
        };
        section.entries.append(entry);
    }
    return section;
}

// ---------------------------------------------------------- Pokemon-Daten

QStringList evolutionMethods() {
    static const QStringList names = {
        "—", "Freundschaft", "Freundschaft (Tag)", "Freundschaft (Nacht)", "Level", "Tausch", "Tausch mit Item",
        "Tausch gegen Pokémon", "Item benutzen", "Level (Ang. > Vert.)", "Level (Ang. = Vert.)", "Level (Ang. < Vert.)",
        "Level (Zufall A)", "Level (Zufall B)", "Level (Ninjask)", "Level (Ninjatom)", "Level (Schönheit)",
        "Item (männlich)", "Item (weiblich)", "Level mit Item (Tag)", "Level mit Item (Nacht)", "Level mit Attacke",
        "Level mit Pokémon im Team", "Level (männlich)", "Level (weiblich)", "Level (Magnetfeld)", "Level (Moosfels)",
        "Level (Eisfels)"};
    QStringList out;
    for (int i = 0; i < names.size(); i++) out << QString("%1: %2").arg(i).arg(names[i]);
    for (int i = names.size(); i < 50; i++) out << QString("%1: (anderes)").arg(i);
    return out;
}

bool samePersonalEntry(const EditSession& s, int index) {
    const std::vector<UValue>& a = personalRoot(s.work)["Personal"].list();
    const std::vector<UValue>& b = personalRoot(s.original)["Personal"].list();
    if (static_cast<size_t>(index) >= a.size() || static_cast<size_t>(index) >= b.size()) return true;
    return sameValue(a[static_cast<size_t>(index)], b[static_cast<size_t>(index)]) &&
           s.work.learnsets.get(index) == s.original.learnsets.get(index) &&
           sameEvolutions(s.work.evolutions.get(index), s.original.evolutions.get(index));
}

edit::Section dataSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Pokémon-Daten";
    section.subtitle = "Typen, Basiswerte, Fähigkeiten, Level-Attacken, TMs und Entwicklungen jedes Pokémon.";
    static const QList<QPair<int, QString>> generations = {
        {151, "Kanto (1–151)"}, {251, "Johto (152–251)"}, {386, "Hoenn (252–386)"}, {493, "Sinnoh (387–493)"}};
    const PersonalTable& p = s->work.personal;
    for (int species = 1; species <= kMaxSpecies; species++) {
        const int forms = p.formCount(p.indexOf(species, 0));
        for (int form = 0; form < forms; form++) {
            const int index = p.indexOf(species, form);
            if (index <= 0 || !p.isPresent(index)) continue;
            edit::Entry entry;
            for (const auto& g : generations) {
                if (species <= g.first) { entry.group = g.second; break; }
            }
            entry.title = QString("#%1 %2").arg(species, 3, 10, QChar('0')).arg(s->texts.pokemonName(species, form));
            entry.changed = [s, index]() { return !samePersonalEntry(*s, index); };
            entry.build = [s, index]() {
                edit::Content c;
                c.fieldsPerLine = 4;
                c.hint = "Werte gelten nur für diese Form. Entwicklungsreihen werden beim Bearbeiten nicht automatisch angeglichen.";
                PersonalTable* w = &s->work.personal;
                const PersonalTable* o = &s->original.personal;
                c.fields << edit::picker("Typ 1", edit::Kind::Type, [w, index]() { return w->type1(index); },
                                         [w, index](int v) { w->set(index, "type1", v); }, [o, index]() { return o->type1(index); });
                c.fields << edit::picker("Typ 2", edit::Kind::Type, [w, index]() { return w->type2(index); },
                                         [w, index](int v) { w->set(index, "type2", v); }, [o, index]() { return o->type2(index); });
                for (int a = 0; a < 3; a++) {
                    c.fields << edit::picker(a == 2 ? QString("Versteckte Fähigkeit") : QString("Fähigkeit %1").arg(a + 1),
                                             edit::Kind::Ability, [w, index, a]() { return w->ability(index, a); },
                                             [w, index, a](int v) { if (v > 0) w->setAbility(index, a, v); },
                                             [o, index, a]() { return o->ability(index, a); });
                }
                c.fields << edit::text("Summe der Basiswerte", [w, index]() {
                    int sum = 0;
                    for (int k = 0; k < 6; k++) sum += w->stat(index, k);
                    return QString::number(sum);
                });
                for (int k = 0; k < 6; k++) {
                    const int st = kStatOrder[k];
                    edit::Field f = edit::number(kStatNames[k], 1, 255, [w, index, st]() { return w->stat(index, st); },
                                                 [w, index, st](int v) { w->setStat(index, st, v); },
                                                 [o, index, st]() { return o->stat(index, st); });
                    f.rebuild = true;
                    c.fields << f;
                }

                LearnsetTable* lw = &s->work.learnsets;
                const LearnsetTable* lo = &s->original.learnsets;
                edit::Table moves;
                moves.title = "Level-Attacken";
                moves.columns = {"Level", "Attacke"};
                moves.rowCount = [lw, index]() { return static_cast<int>(lw->get(index).size()); };
                moves.row = [lw, lo, index](int r) {
                    const bool hasOrig = r < lo->get(index).size();
                    QList<edit::Field> fields;
                    edit::Field level = edit::number("Level", 0, 100, [lw, index, r]() { return lw->get(index).value(r).second; },
                        [lw, index, r](int v) { auto l = lw->get(index); if (r < l.size()) { l[r].second = v; lw->set(index, l); } },
                        hasOrig ? std::function<int()>([lo, index, r]() { return lo->get(index).value(r).second; }) : std::function<int()>());
                    level.info = "0 = beim Entwickeln";
                    fields << level;
                    fields << edit::picker("Attacke", edit::Kind::Move, [lw, index, r]() { return lw->get(index).value(r).first; },
                        [lw, index, r](int v) { auto l = lw->get(index); if (r < l.size() && v > 0) { l[r].first = v; lw->set(index, l); } },
                        hasOrig ? std::function<int()>([lo, index, r]() { return lo->get(index).value(r).first; }) : std::function<int()>());
                    return fields;
                };
                moves.canAdd = [lw, index]() { return lw->get(index).size() < 40; };
                moves.add = [lw, index]() {
                    auto l = lw->get(index);
                    l.append({1, l.isEmpty() ? 1 : l.last().second});
                    lw->set(index, l);
                };
                moves.canRemove = [lw, index]() { return lw->get(index).size() > 1; };
                moves.remove = [lw, index](int r) {
                    auto l = lw->get(index);
                    if (r >= 0 && r < l.size()) l.removeAt(r);
                    lw->set(index, l);
                };
                c.tables << moves;

                EvolutionTable* ew = &s->work.evolutions;
                const EvolutionTable* eo = &s->original.evolutions;
                edit::Table evos;
                evos.title = "Entwicklungen";
                evos.columns = {"Methode", "Wert", "Entwickelt sich zu", "Level"};
                evos.rowCount = [ew, index]() { return static_cast<int>(ew->get(index).size()); };
                evos.row = [ew, eo, index](int r) {
                    const bool hasOrig = r < eo->get(index).size();
                    auto update = [ew, index, r](std::function<void(Evolution&)> fn) {
                        QList<Evolution> l = ew->get(index);
                        if (r < l.size()) { fn(l[r]); ew->set(index, l); }
                    };
                    auto orig = [eo, index, r, hasOrig](std::function<int(const Evolution&)> fn) -> std::function<int()> {
                        if (!hasOrig) return {};
                        return [eo, index, r, fn]() { return fn(eo->get(index)[r]); };
                    };
                    QList<edit::Field> fields;
                    fields << edit::choice("Methode", evolutionMethods(), [ew, index, r]() { return ew->get(index).value(r).method; },
                                           [update](int v) { if (v > 0) update([v](Evolution& e) { e.method = v; }); },
                                           orig([](const Evolution& e) { return e.method; }));
                    edit::Field arg = edit::number("Wert", 0, 65535, [ew, index, r]() { return ew->get(index).value(r).argument; },
                                                   [update](int v) { update([v](Evolution& e) { e.argument = v; }); },
                                                   orig([](const Evolution& e) { return e.argument; }));
                    arg.info = "Zusatzwert der Methode: Item-Nr. (Item benutzen, mit Item), Attacken-Nr. oder Pokémon-Nr.";
                    fields << arg;
                    fields << edit::picker("Ziel", edit::Kind::Pokemon,
                        [ew, index, r]() { const Evolution e = ew->get(index).value(r); return edit::pokemonValue(e.species, e.form); },
                        [update](int v) { if (edit::speciesOf(v) > 0) update([v](Evolution& e) { e.species = edit::speciesOf(v); e.form = edit::formOf(v); }); },
                        orig([](const Evolution& e) { return edit::pokemonValue(e.species, e.form); }));
                    fields << edit::number("Level", 0, 100, [ew, index, r]() { return ew->get(index).value(r).level; },
                                           [update](int v) { update([v](Evolution& e) { e.level = v; }); },
                                           orig([](const Evolution& e) { return e.level; }));
                    return fields;
                };
                evos.canAdd = [ew, index]() { return ew->get(index).size() < 8; };
                evos.add = [ew, index, s]() {
                    QList<Evolution> l = ew->get(index);
                    const QPair<int, int> self = s->work.personal.speciesForm(index);
                    Evolution e;
                    e.method = 4;
                    e.species = self.first;
                    e.form = self.second;
                    e.level = 100;
                    l.append(e);
                    ew->set(index, l);
                };
                evos.remove = [ew, index](int r) {
                    QList<Evolution> l = ew->get(index);
                    if (r >= 0 && r < l.size()) l.removeAt(r);
                    ew->set(index, l);
                };
                c.tables << evos;

                edit::Table tms;
                tms.title = "TMs";
                tms.columns = {"Nr.", "Attacke", "Lernbar"};
                tms.rowCount = []() { return 100; };
                tms.row = [s, w, o, index](int r) {
                    const int no = r + 1;
                    QList<edit::Field> fields;
                    fields << edit::text("", [no]() { return QString("TM%1").arg(no, 2, 10, QChar('0')); });
                    const int move = s->work.tmMoves.value(no);
                    fields << edit::text("", [s, move]() { return s->texts.moveName(move); });
                    fields << edit::check("Lernbar", [w, index, no]() { return w->canLearnTM(index, no) ? 1 : 0; },
                                          [w, index, no](int v) { w->setTM(index, no, v != 0); },
                                          [o, index, no]() { return o->canLearnTM(index, no) ? 1 : 0; });
                    return fields;
                };
                c.tables << tms;
                return c;
            };
            section.entries.append(entry);
        }
    }
    return section;
}

// -------------------------------------------------------------- Kampfturm

edit::Section towerSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Kampfturm";
    section.subtitle = "Alle Pokémon, aus denen die Gegner im Kampfturm ihre Teams zusammenstellen.";
    if (!s->original.masterBundle.has("TowerTrainerTable")) return section;
    const std::vector<UValue>& pokes = masterTable(s->original, "TowerTrainerTable")["TrainerPoke"].list();
    for (size_t k = 0; k < pokes.size(); k++) {
        if (pokes[k]["MonsNo"].i <= 0) continue;
        const int row = static_cast<int>(k);
        RowFn rf = [s, k]() -> UValue& { return s->work.masterBundle.table("TowerTrainerTable")["TrainerPoke"].list()[k]; };
        ConstRowFn of = [s, k]() -> const UValue& { return s->original.masterBundle.table("TowerTrainerTable")["TrainerPoke"].list()[k]; };
        edit::Entry entry;
        const int block = row / 100;
        entry.group = QString("Pokémon %1–%2").arg(block * 100 + 1).arg(qMin(static_cast<int>(pokes.size()), block * 100 + 100));
        entry.title = QString("#%1 ").arg(row + 1) + pokemonText(*s, static_cast<int>(pokes[k]["MonsNo"].i), static_cast<int>(pokes[k]["FormNo"].i));
        entry.search = [s, rf]() { return s->texts.pokemonName(static_cast<int>(rf()["MonsNo"].i), static_cast<int>(rf()["FormNo"].i)); };
        entry.changed = [rf, of]() { return !sameValue(rf(), of()); };
        entry.build = [s, rf, of, row]() {
            edit::Content c;
            c.fieldsPerLine = 4;
            c.hint = QString("Kampfturm-Pokémon (ID %1). Kein Team sollte eine Art doppelt enthalten.").arg(rf()["ID"].i);
            edit::Field poke = valuePokemon("Pokémon", rf, of, "MonsNo", "FormNo");
            poke.span = 2;
            c.fields << poke;
            c.fields << valueNumber("Level", 1, 100, rf, of, "Level");
            c.fields << valueCheck("Schillernd", rf, of, "IsRare");
            for (int m = 0; m < 4; m++) {
                const QByteArray name = "Waza" + QByteArray::number(m + 1);
                c.fields << valuePicker(QString("Attacke %1").arg(m + 1), edit::Kind::Move, rf, of, name.constData());
            }
            c.fields << valuePicker("Item", edit::Kind::Item, rf, of, "Item");
            c.fields << valuePicker("Fähigkeit", edit::Kind::Ability, rf, of, "Tokusei");
            c.fields << valuePicker("Wesen", edit::Kind::Nature, rf, of, "Seikaku");
            c.fields << valueChoice("Geschlecht", genderChoices(), rf, of, "Sex");
            static const char* const ivs[6] = {"TalentHp", "TalentAtk", "TalentDef", "TalentSpAtk", "TalentSpDef", "TalentAgi"};
            static const char* const evs[6] = {"EffortHp", "EffortAtk", "EffortDef", "EffortSpAtk", "EffortSpDef", "EffortAgi"};
            for (int k = 0; k < 6; k++) c.fields << valueNumber("DV " + kStatNames[k], 0, 31, rf, of, ivs[k]);
            for (int k = 0; k < 6; k++) c.fields << valueNumber("FP " + kStatNames[k], 0, 252, rf, of, evs[k]);
            c.warnings = [s, row]() {
                QStringList w;
                if (row >= s->towerNeighbours.size()) return w;
                const std::vector<UValue>& list = s->work.masterBundle.table("TowerTrainerTable")["TrainerPoke"].list();
                const qint64 species = list[static_cast<size_t>(row)]["MonsNo"].i;
                for (int other : s->towerNeighbours[row]) {
                    if (list[static_cast<size_t>(other)]["MonsNo"].i == species) {
                        w << QString("Steht mit #%1 (gleiche Art) in einem Team.").arg(other + 1);
                    }
                }
                return w;
            };
            return c;
        };
        section.entries.append(entry);
    }
    return section;
}

// ------------------------------------------------------ Spoiler nach Bearbeitung

QList<EncounterChange> encounterLog(EditSession& s) {
    QList<EncounterChange> out;
    for (int i = 0; i < 3; i++) {
        if (s.starters[i] != kOriginalStarters[i]) out.append({0, i, kOriginalStarters[i], s.starters[i], 5, QString(), 0});
    }
    if (s.eevee != 133) out.append({1, 0, 133, s.eevee, 5, "Bebe in Herzhofen", 0});
    for (const ScriptPos& p : s.eggPos) {
        const int before = readInt(s.scriptsOriginal, p), now = readInt(s.scripts, p);
        if (before != now) out.append({1, 1, before, now, 1, "Ei · " + placeForZone(p.zone), 0});
    }
    // Statische Begegnungen in der Reihenfolge der Kaempfe, je Art und Ort einmal
    struct Hit { QString zone; int script; int command; int index; };
    QList<Hit> hits;
    for (int i = 0; i < s.statics.size(); i++)
        for (const ScriptPos& p : s.statics[i].battle) hits.append({p.zone, p.script, p.command, i});
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.zone != b.zone) return a.zone < b.zone;
        if (a.script != b.script) return a.script < b.script;
        return a.command < b.command;
    });
    QSet<QString> listed;
    for (const Hit& h : hits) {
        const EditSession::Static& st = s.statics[h.index];
        const int now = readInt(s.scripts, st.battle.first());
        const ScriptPos levelPos{h.zone, h.script, h.command, 2};
        const int level = readInt(s.scripts, levelPos);
        if (now == st.species && level == readInt(s.scriptsOriginal, levelPos)) continue;
        const QString note = placeForZone(h.zone);
        const QString key = QString::number(st.species) + "|" + note;
        if (listed.contains(key)) continue;
        listed.insert(key);
        out.append({2, 0, st.species, now, level, note, 0});
    }
    // Tausch
    const std::vector<UValue>& a = s.work.masterBundle.table("LocalKoukanData")["data"].list();
    const std::vector<UValue>& b = s.original.masterBundle.table("LocalKoukanData")["data"].list();
    QSet<QString> seen;
    int index = 0;
    for (size_t k = 0; k < a.size() && k < b.size(); k++) {
        const QString key = b[k]["name_label"].toString();
        if (b[k]["monsno"].i <= 0 || seen.contains(key)) continue;
        seen.insert(key);
        if (sameValue(a[k], b[k])) continue;
        out.append({4, index++, static_cast<int>(b[k]["monsno"].i), static_cast<int>(a[k]["monsno"].i),
                    static_cast<int>(a[k]["level"].i), QString(), static_cast<int>(a[k]["target"].i)});
    }
    return out;
}

bool itemsEditedSinceStart(EditSession& s) {
    for (const EditSession::ItemSpot& spot : s.itemSpots) {
        if (readInt(s.scripts, spot.item) != readInt(s.scriptsStart, spot.item)) return true;
        if (!spot.count.zone.isEmpty() && readInt(s.scripts, spot.count) != readInt(s.scriptsStart, spot.count)) return true;
    }
    return false;
}

ItemResult itemLog(EditSession& s) {
    const bool spotsEdited = s.reopened || itemsEditedSinceStart(s);
    const bool shopsEdited = s.reopened || !sameTable(s.work.masterBundle, s.start.masterBundle, "ShopTable");
    ItemResult out = s.items;
    if (spotsEdited) {
        const bool randomized = !s.items.placedItems.isEmpty();
        out.fieldItems = out.hiddenItems = 0;
        out.placedItems.clear();
        for (const EditSession::ItemSpot& spot : s.itemSpots) {
            const int now = readInt(s.scripts, spot.item);
            const bool changed = now != readInt(s.scriptsOriginal, spot.item) ||
                                 (!spot.count.zone.isEmpty() && readInt(s.scripts, spot.count) != readInt(s.scriptsOriginal, spot.count));
            if (changed) (spot.hidden ? out.hiddenItems : out.fieldItems)++;
            if (changed || randomized) out.placedItems[now] += 1;
        }
    }
    if (shopsEdited) {
        out.shopItems = 0;
        out.shops.clear();
        const UValue& now = s.work.masterBundle.table("ShopTable");
        const UValue& before = s.original.masterBundle.table("ShopTable");
        QMap<QPair<int, int>, QList<int>> shops;
        const std::vector<UValue>& fs = now["FS"].list();
        const std::vector<UValue>& fsBefore = before["FS"].list();
        const std::vector<UValue>& fixed = now["FixedShop"].list();
        const std::vector<UValue>& fixedBefore = before["FixedShop"].list();
        QMap<QPair<int, int>, bool> changed;
        for (size_t k = 0; k < fs.size() && k < fsBefore.size(); k++) {
            const auto key = qMakePair(0, fs[k]["ZoneID"].toInt());
            shops[key].append(fs[k]["ItemNo"].toInt());
            if (fs[k]["ItemNo"].i != fsBefore[k]["ItemNo"].i) { changed[key] = true; out.shopItems++; }
        }
        for (size_t k = 0; k < fixed.size() && k < fixedBefore.size(); k++) {
            const auto key = qMakePair(1, fixed[k]["ShopID"].toInt());
            shops[key].append(fixed[k]["ItemNo"].toInt());
            if (fixed[k]["ItemNo"].i != fixedBefore[k]["ItemNo"].i) { changed[key] = true; out.shopItems++; }
        }
        for (auto it = shops.constBegin(); it != shops.constEnd(); ++it) {
            if (!changed.value(it.key())) continue;
            ItemResult::ShopLog log;
            if (it.key().first == 0) {
                log.name = it.key().second < 0 ? QString("Pokémon-Center (Grundsortiment)")
                                               : "Pokémon-Center " + s.zoneNames.value(it.key().second, QString("Zone %1").arg(it.key().second));
            } else {
                log.name = fixedShopName(it.key().second);
            }
            log.items = it.value();
            out.shops.append(log);
        }
    }
    return out;
}

QList<EvolutionChange> evolutionLog(EditSession& s) {
    QList<EvolutionChange> out = s.dataResult.evolutions;
    const QStringList methods = evolutionMethods();
    auto describe = [&](const Evolution& e) {
        QString text = methods.value(e.method, QString::number(e.method)).section(": ", 1);
        if (e.method == 4) text = QString("ab Level %1").arg(e.level);
        else if (e.level > 0) text += QString(", ab Level %1").arg(e.level);
        if (e.argument > 0) {
            if (e.method == 6 || e.method == 8 || e.method == 17 || e.method == 18 || e.method == 19 || e.method == 20) {
                text += " (" + s.texts.itemName(e.argument) + ")";
            } else {
                text += QString(" (Wert %1)").arg(e.argument);
            }
        }
        return text;
    };
    for (int index = 0; index < s.work.evolutions.count(); index++) {
        const QList<Evolution> now = s.work.evolutions.get(index);
        if (!s.reopened && sameEvolutions(now, s.start.evolutions.get(index))) continue;
        const QList<Evolution> before = s.original.evolutions.get(index);
        if (sameEvolutions(now, before)) continue;
        const QPair<int, int> self = s.work.personal.speciesForm(index);
        out.erase(std::remove_if(out.begin(), out.end(), [&self](const EvolutionChange& c) {
            return c.fromSpecies == self.first && c.fromForm == self.second;
        }), out.end());
        // Je Ziel-Pokemon alle Wege vorher/jetzt (die Reihenfolge der Eintraege kann sich verschieben)
        QList<QPair<int, int>> targets;
        for (const QList<Evolution>* list : {&before, &now}) {
            for (const Evolution& e : *list) {
                const QPair<int, int> t(e.species, e.form);
                if (!targets.contains(t)) targets.append(t);
            }
        }
        for (const QPair<int, int>& t : targets) {
            QStringList was, is;
            for (const Evolution& e : before) if (e.species == t.first && e.form == t.second) was << describe(e);
            for (const Evolution& e : now) if (e.species == t.first && e.form == t.second) is << describe(e);
            if (was == is) continue;
            EvolutionChange c;
            c.fromSpecies = self.first;
            c.fromForm = self.second;
            c.toSpecies = t.first;
            c.toForm = t.second;
            c.before = was.isEmpty() ? QString("—") : was.join(" / ");
            c.after = (is.isEmpty() ? QString("entfernt") : is.join(" / ")) + (s.reopened ? QString() : QString(" (von Hand)"));
            out.append(c);
        }
    }
    return out;
}

TowerResult towerLog(EditSession& s) {
    if (!s.reopened && sameTable(s.work.masterBundle, s.start.masterBundle, "TowerTrainerTable")) return s.tower;
    TowerResult out = s.tower;
    out.pokemon.clear();
    const std::vector<UValue>& a = s.work.masterBundle.table("TowerTrainerTable")["TrainerPoke"].list();
    const std::vector<UValue>& b = s.original.masterBundle.table("TowerTrainerTable")["TrainerPoke"].list();
    for (size_t k = 0; k < a.size() && k < b.size(); k++) {
        if (a[k]["MonsNo"].i > 0 && !sameValue(a[k], b[k])) out.pokemon.append(static_cast<int>(a[k]["MonsNo"].i));
    }
    return out;
}

void computeTowerNeighbours(EditSession& s) {
    s.towerNeighbours.clear();
    if (!s.original.masterBundle.has("TowerTrainerTable")) return;
    const std::vector<UValue>& pokes = masterTable(s.original, "TowerTrainerTable")["TrainerPoke"].list();
    QHash<qint64, int> byId;
    for (int k = 0; k < static_cast<int>(pokes.size()); k++) byId.insert(pokes[static_cast<size_t>(k)]["ID"].i, k);
    s.towerNeighbours.resize(static_cast<int>(pokes.size()));
    for (const auto& table : {qMakePair(QString("TowerSingleStockTable"), QByteArray("TowerSingleStock")),
                              qMakePair(QString("TowerDoubleStockTable"), QByteArray("TowerDoubleStock"))}) {
        if (!s.original.masterBundle.has(table.first)) continue;
        for (const UValue& team : s.original.masterBundle.table(table.first)[table.second.constData()].list()) {
            QList<int> rows;
            for (const UValue& id : team["PokeID"].list()) if (byId.contains(id.i)) rows.append(byId.value(id.i));
            for (int x : rows)
                for (int y : rows)
                    if (x != y) s.towerNeighbours[x].insert(y);
        }
    }
}

} // namespace

// ================================================================ Oeffentlich

bool openSession(EditSession& s, QString* error) {
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };
    QString err;
    s.texts.load(s.romfs);
    if (!s.fieldOriginal.load(s.romfs, path::GameSettings, {"FieldEncountTable_d", "FieldEncountTable_p", "MapInfo"}, &err)) {
        return fail("Die wilden Pokémon (gamesettings) konnten nicht gelesen werden:\n" + err);
    }
    s.zoneNames = zoneNames(s.romfs, &s.fieldOriginal.table("MapInfo"));
    if (!s.field.has("FieldEncountTable_d")) s.field = s.fieldOriginal;
    if (!s.ugOriginal.loadWhere(s.romfs, path::Assets + "UnderGround/data/ugdata",
                                [](const UValue& v) { return v.has("table") || v.has("Sheet1"); }, &err)) {
        return fail("Der Untergrund (ugdata) konnte nicht gelesen werden:\n" + err);
    }
    if (!s.ug.has("UgSpecialPokemon")) s.ug = s.ugOriginal;
    if (!s.scriptsOriginal.ensureLoaded(s.romfs, &err)) return fail(err);
    if (!s.scripts.loaded) s.scripts = s.scriptsOriginal;
    s.scripts.changed.clear();
    s.smart.build(s.work.toGameData());

    findScriptPositions(s);
    computeTowerNeighbours(s);

    // Starter und Evoli nach dem Randomisieren
    if (s.randomized) {
        for (const EncounterChange& c : s.encounters.changes) {
            if (c.category == 0 && c.index >= 0 && c.index < 3) s.starters[c.index] = c.newSpecies;
            if (c.category == 1 && c.index == 0) s.eevee = c.newSpecies;
        }
    }
    for (int k = 0; k < 3; k++) s.startersStart[k] = s.starters[k];
    s.eeveeStart = s.eevee;
    s.start = s.work;
    s.scriptsStart = s.scripts;
    s.fieldStart = s.field;
    s.ugStart = s.ug;
    return true;
}

bool loadModInto(EditSession& s, const QString& modFolder, QString* error) {
    auto fail = [error](const QString& message) {
        setReadOverlay(QString(), QString());
        if (error) *error = message;
        return false;
    };
    QString modRomfs = modFolder + "/romfs";
    if (!QFileInfo::exists(modRomfs + "/Data") && QFileInfo::exists(modFolder + "/Data")) modRomfs = modFolder;
    if (!QFileInfo::exists(modRomfs + "/Data")) {
        return fail("In diesem Ordner wurde keine Mod gefunden. Wähle den Ordner, in dem „romfs“ liegt "
                    "(z. B. Randomizers-Output/Diamant-Perle/Randomizer-1).");
    }
    setReadOverlay(s.romfs, modRomfs);
    QStringList errors;
    GameFiles mod;
    if (!mod.load(s.romfs, &errors)) return fail("Die Mod konnte nicht gelesen werden:\n" + errors.join("\n"));
    s.work = mod;
    QString err;
    if (!s.scripts.ensureLoaded(s.romfs, &err)) return fail(err);
    if (!s.field.load(s.romfs, path::GameSettings, {"FieldEncountTable_d", "FieldEncountTable_p", "MapInfo"}, &err)) return fail(err);
    if (!s.ug.loadWhere(s.romfs, path::Assets + "UnderGround/data/ugdata",
                        [](const UValue& v) { return v.has("table") || v.has("Sheet1"); }, &err)) return fail(err);
    setReadOverlay(QString(), QString());

    // Starter aus global-metadata.dat der Mod (gleiche Stelle wie im Dump)
    const QString metaRel = "/Data/Managed/Metadata/global-metadata.dat";
    const QByteArray dumpMeta = readFile(s.romfs + metaRel);
    const QByteArray modMeta = QFileInfo::exists(modRomfs + metaRel) ? readFile(modRomfs + metaRel) : QByteArray();
    if (!dumpMeta.isEmpty() && !modMeta.isEmpty() && modMeta.size() == dumpMeta.size()) {
        QByteArray triple(12, '\0');
        qToLittleEndian<qint32>(387, triple.data());
        qToLittleEndian<qint32>(390, triple.data() + 4);
        qToLittleEndian<qint32>(393, triple.data() + 8);
        const int pos = dumpMeta.indexOf(triple);
        if (pos >= 0) {
            for (int k = 0; k < 3; k++) {
                s.starters[k] = qFromLittleEndian<qint32>(reinterpret_cast<const uchar*>(modMeta.constData() + pos + 4 * k));
            }
        }
    }
    // Evoli aus dem IPS-Patch (falls vorhanden)
    QDir exefsDir(QFileInfo(modRomfs).absolutePath() + "/exefs");
    const QStringList patches = exefsDir.entryList({"*.ips"}, QDir::Files);
    if (!patches.isEmpty() && !s.exefs.isEmpty()) {
        ExeFsPatcher exe;
        if (exe.load(s.exefs, &err)) {
            const int eevee = exe.readEevee(readFile(exefsDir.filePath(patches.first())));
            if (eevee > 0) s.eevee = eevee;
        }
    }
    s.reopened = true;
    return true;
}

edit::Catalog buildCatalog(const EditSession& s) {
    edit::Catalog c;
    const PersonalTable& p = s.work.personal;
    for (int species = 1; species <= kMaxSpecies; species++) {
        const int forms = p.formCount(p.indexOf(species, 0));
        for (int form = 0; form < forms; form++) {
            const int index = p.indexOf(species, form);
            if (index <= 0 || !p.isPresent(index)) continue;
            c.pokemon.append({edit::pokemonValue(species, form), s.texts.pokemonName(species, form)});
        }
    }
    auto sorted = [](QList<QPair<int, QString>> list, const QString& none) {
        std::sort(list.begin(), list.end(), [](const QPair<int, QString>& a, const QPair<int, QString>& b) {
            return QString::localeAwareCompare(a.second, b.second) < 0;
        });
        list.prepend({0, none});
        return list;
    };
    QList<QPair<int, QString>> moves;
    for (const Move& m : s.work.moves.all()) {
        if (m.id <= 0 || !m.canUse || m.id >= s.texts.moves.size() || s.texts.moves[m.id].trimmed().isEmpty()) continue;
        moves.append({m.id, s.texts.moves[m.id]});
    }
    c.moves = sorted(moves, "(keine)");
    QList<QPair<int, QString>> items;
    for (int i = 1; i < s.texts.items.size(); i++) {
        const QString name = s.texts.items[i].trimmed();
        if (name.isEmpty() || name.startsWith('?') || name == "-" || name == "―") continue;
        items.append({i, name});
    }
    c.items = sorted(items, "(kein Item)");
    QList<QPair<int, QString>> abilities;
    for (int i = 1; i < s.texts.abilities.size(); i++) {
        const QString name = s.texts.abilities[i].trimmed();
        if (!name.isEmpty() && name != "-" && name != "―") abilities.append({i, name});
    }
    c.abilities = sorted(abilities, "(keine)");
    for (int t = 0; t < 18; t++) c.types.append({t, s.texts.typeName(t)});
    const QStringList natures = edit::natureNames();
    for (int n = 0; n < natures.size(); n++) c.natures.append({n, natures[n]});
    return c;
}

QList<edit::Section> buildSections(EditSession& s) {
    return {trainerSection(s), wildSection(s), encounterSection(s), itemSection(s), dataSection(s), towerSection(s)};
}

int changedTrainerCount(const EditSession& s) {
    int count = 0;
    for (const Trainer& t : s.work.trainers.all()) {
        const Trainer* o = trainerAt(s.original, t.index);
        if (o && !sameTrainer(t, *o)) count++;
    }
    return count;
}

bool sessionHasChanges(EditSession& s) {
    if (changedTrainerCount(s) > 0) return true;
    for (int k = 0; k < 3; k++) if (s.starters[k] != kOriginalStarters[k]) return true;
    if (s.eevee != 133) return true;
    for (const QString& zone : s.scripts.bundle.tableNames()) {
        if (!sameTable(s.scripts.bundle, s.scriptsOriginal.bundle, zone)) return true;
    }
    for (const QString& t : {QString("FieldEncountTable_d"), QString("FieldEncountTable_p")}) {
        if (!sameTable(s.field, s.fieldOriginal, t)) return true;
    }
    for (const QString& t : s.ug.tableNames()) if (!sameTable(s.ug, s.ugOriginal, t)) return true;
    if (!sameValue(personalRoot(s.work), personalRoot(s.original))) return true;
    if (evolutionsDiffer(s.work, s.original) || learnsetsDiffer(s.work, s.original)) return true;
    for (const char* t : {"LocalKoukanData", "ShopTable", "TowerTrainerTable"}) {
        if (!sameTable(s.work.masterBundle, s.original.masterBundle, t)) return true;
    }
    return false;
}

bool exportSession(EditSession& s, const QString& outFolder, const QString& spoilerPath,
                   const TrainerSettings& trainerSettings, const QString& seedText, bool* hasExefs, QString* error) {
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };
    if (hasExefs) *hasExefs = false;
    const QString romfsOut = outFolder + "/romfs";
    QString err;

    // Spieldaten (Personal, Entwicklungen, Lernsets, Trainer, Tausch, Laeden, Kampfturm)
    GameFiles out = s.work;
    out.personalChanged = !sameValue(personalRoot(s.work), personalRoot(s.original));
    out.evolutionsChanged = evolutionsDiffer(s.work, s.original);
    out.learnsetsChanged = learnsetsDiffer(s.work, s.original);
    out.tradesChanged = !sameTable(s.work.masterBundle, s.original.masterBundle, "LocalKoukanData");
    out.shopsChanged = !sameTable(s.work.masterBundle, s.original.masterBundle, "ShopTable");
    out.towerChanged = !sameTable(s.work.masterBundle, s.original.masterBundle, "TowerTrainerTable");
    QSet<int> editedTrainers;
    for (Trainer& t : out.trainers.all()) {
        const Trainer* o = trainerAt(s.original, t.index);
        t.changed = o && !sameTrainer(t, *o);
        const Trainer* st = trainerAt(s.start, t.index);
        if (t.changed && (s.reopened || !st || !sameTrainer(t, *st) || !s.randomized)) editedTrainers.insert(t.index);
    }
    if (!out.save(romfsOut, &err)) return fail("Die Dateien konnten nicht geschrieben werden:\n" + err);

    // Skripte
    ScriptData scripts = s.scripts;
    scripts.loaded = true;
    scripts.changed.clear();
    for (const QString& zone : s.scripts.bundle.tableNames()) {
        if (!sameTable(s.scripts.bundle, s.scriptsOriginal.bundle, zone)) scripts.changed.insert(zone);
    }
    if (!scripts.save(romfsOut, &err)) return fail(err);

    // Wilde Pokemon und Untergrund
    QStringList fieldTables;
    for (const QString& t : {QString("FieldEncountTable_d"), QString("FieldEncountTable_p")}) {
        if (!sameTable(s.field, s.fieldOriginal, t)) fieldTables << t;
    }
    if (!fieldTables.isEmpty() && !s.field.save(romfsOut, fieldTables, &err)) return fail(err);
    QStringList ugTables;
    for (const QString& t : s.ug.tableNames()) if (!sameTable(s.ug, s.ugOriginal, t)) ugTables << t;
    if (!ugTables.isEmpty() && !s.ug.save(romfsOut, ugTables, &err)) return fail(err);

    // Starter und Evoli: Programmcode (IPS) und global-metadata.dat
    const bool startersChanged = s.starters[0] != 387 || s.starters[1] != 390 || s.starters[2] != 393;
    if (startersChanged || s.eevee != 133) {
        if (s.exefs.isEmpty()) return fail("Für Starter und Evoli wird der ExeFS-Ordner deines Dumps benötigt.");
        ExeFsPatcher exe;
        if (!exe.load(s.exefs, &err)) return fail(err);
        if (startersChanged && !exe.patchStarters(s.starters[0], s.starters[1], s.starters[2], &err)) return fail(err);
        if (s.eevee != 133 && !exe.patchEevee(s.eevee, &err)) return fail(err);
        EncounterResult patch;
        patch.ips = exe.ips();
        patch.ipsName = exe.buildIdName() + ".ips";
        if (startersChanged) {
            bool ok = false;
            const QByteArray metadata = readFile(s.romfs + "/Data/Managed/Metadata/global-metadata.dat", &ok);
            if (!ok) return fail("Im RomFS fehlt die Datei „Data/Managed/Metadata/global-metadata.dat“.");
            patch.metadata = patchMetadataStarters(metadata, s.starters[0], s.starters[1], s.starters[2]);
            if (patch.metadata.isEmpty()) return fail("Die Starter-Liste wurde in global-metadata.dat nicht gefunden.");
        }
        if (!writeEncounters(outFolder, patch, &err)) return fail(err);
        if (hasExefs) *hasExefs = true;
    }

    if (!spoilerPath.isEmpty()) {
        EncounterResult encounterResult;
        encounterResult.changes = encounterLog(s);
        // setStarter tauscht die Starter-Linie auch in den Teams von Rivale und Lucius/Lucia
        encounterResult.rivalPokemon = startersChanged ? qMax(1, s.encounters.rivalPokemon) : 0;
        QList<SpoilerSection> sections = encounterSpoiler(encounterResult, s.work, s.texts);
        WildResult wild;
        wildLogs(s.field, s.fieldOriginal, &s.ug, &s.ugOriginal, s.version, s.zoneNames, s.wildRandomized,
                 s.undergroundRandomized, wild);
        sections += wildSpoiler(wild, s.work, s.texts);
        sections += itemSpoiler(itemLog(s), s.texts);
        PokemonDataResult data = s.dataResult;
        data.evolutions = evolutionLog(s);
        if (out.personalChanged && data.types + data.stats + data.abilities == 0) data.types = 1;
        if (!out.personalChanged) data.types = data.stats = data.abilities = 0;
        sections += pokemonDataSpoiler(data, towerLog(s), s.work, s.texts);
        bool evolutionsByHand = false;
        for (const EvolutionChange& c : data.evolutions) evolutionsByHand |= c.after.endsWith("(von Hand)");
        for (SpoilerSection& section : sections) {
            if ((evolutionsByHand || s.reopened) && section.title == "Tausch-Entwicklungen") section.title = "Entwicklungen";
        }
        writeSpoiler(spoilerPath, s.work, s.texts, trainerSettings, sections, seedText, s.version, editedTrainers,
                     s.reopened ? QString("geändert") : QString("von Hand bearbeitet"));
    }
    return true;
}

} // namespace bdsp
