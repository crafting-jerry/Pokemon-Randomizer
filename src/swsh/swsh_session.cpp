#include "headers/swsh/swsh_session.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QtEndian>
#include <algorithm>

namespace swsh {

namespace {

const QString kDataTable = "bin/archive/field/resident/data_table.gfpak";
const char* kWildFiles[4] = {"encount_k.bin", "encount_symbol_k.bin", "encount_t.bin", "encount_symbol_t.bin"};
const QString kTradeText = "bin/message/German/script/field_trade.dat";

// ------------------------------------------------------------ Vergleiche

bool sameRecord(const FlatRecord& a, const FlatRecord& b) {
    const int n = static_cast<int>(qMax(a.values.size(), b.values.size()));
    for (int f = 0; f < n; f++) {
        if (a.get(f) != b.get(f)) return false;
    }
    return true;
}

bool sameRecords(const QList<FlatRecord>& a, const QList<FlatRecord>& b) {
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); i++) {
        if (!sameRecord(a[i], b[i])) return false;
    }
    return true;
}

bool sameZone(const WildZone& a, const WildZone& b) {
    if (a.zoneId != b.zoneId || a.subTables.size() != b.subTables.size()) return false;
    for (int s = 0; s < a.subTables.size(); s++) {
        const WildSubTable& x = a.subTables[s];
        const WildSubTable& y = b.subTables[s];
        if (x.levelMin != y.levelMin || x.levelMax != y.levelMax || x.entries.size() != y.entries.size()) return false;
        for (int k = 0; k < x.entries.size(); k++) {
            if (x.entries[k].probability != y.entries[k].probability || x.entries[k].species != y.entries[k].species ||
                x.entries[k].form != y.entries[k].form) {
                return false;
            }
        }
    }
    return true;
}

bool sameArchive(const WildArchive& a, const WildArchive& b) {
    if (a.field0 != b.field0 || a.zones.size() != b.zones.size()) return false;
    for (int z = 0; z < a.zones.size(); z++) {
        if (!sameZone(a.zones[z], b.zones[z])) return false;
    }
    return true;
}

bool sameTrainer(const Trainer& a, const Trainer& b) {
    return a.data == b.data && a.pokeFile() == b.pokeFile();
}

const Trainer* originalTrainer(const EditSession& s, int index) {
    const QList<Trainer>& list = s.original.trainers.all();
    return index >= 0 && index < list.size() ? &list[index] : nullptr;
}

// ------------------------------------------------- Rohwerte der Trainer-Pokemon

int u8(const QByteArray& raw, int pos) { return static_cast<quint8>(raw[pos]); }
void setU8(QByteArray& raw, int pos, int value) { raw[pos] = static_cast<char>(qBound(0, value, 255)); }
quint32 ivWord(const QByteArray& raw) { return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(raw.constData() + 0x1C)); }
void setIvWord(QByteArray& raw, quint32 value) { qToLittleEndian<quint32>(value, reinterpret_cast<uchar*>(raw.data() + 0x1C)); }
// Reihenfolge der DVs im 32-Bit-Wert: KP, Angriff, Verteidigung, Initiative, Sp.-Angriff, Sp.-Verteidigung
int ivAt(const QByteArray& raw, int bit) { return static_cast<int>((ivWord(raw) >> (5 * bit)) & 0x1F); }
void setIvAt(QByteArray& raw, int bit, int value) {
    quint32 word = ivWord(raw);
    word = (word & ~(0x1Fu << (5 * bit))) | (static_cast<quint32>(qBound(0, value, 31)) << (5 * bit));
    setIvWord(raw, word);
}

// Anzeige-Reihenfolge KP, Ang, Vert, SpAng, SpVert, Init
const QStringList kStatNames = {"KP", "Angriff", "Verteidigung", "Sp.-Angriff", "Sp.-Verteidigung", "Initiative"};
const int kIvBit[6] = {0, 1, 2, 4, 5, 3};       // Bit-Position im DV-Wert
const int kEvByte[6] = {2, 3, 4, 5, 6, 7};      // Byte der FP in trainer_poke

QStringList genderChoices() { return {"Zufällig", "Männlich", "Weiblich"}; }
// Laut Originaldaten: 0 = zufaellig (Fossilien), 2 = gesperrt (Starter, Legendaere). 1 ist im Spiel nicht belegt.
QStringList shinyChoices() { return {"Zufällig", "Wert 1 (unbekannt)", "Nie (gesperrt)"}; }
QStringList natureChoicesWithRandom() {
    QStringList list = edit::natureNames();
    list << "Zufällig";
    return list;
}

const QStringList kWeatherNames = {"Normal", "Bewölkt", "Regen", "Gewitter", "Starke Sonne", "Schnee",
                                   "Schneesturm", "Sandsturm", "Dichter Nebel", "Bäume schütteln", "Angeln"};
const QStringList kWildGroups = {"Versteckt (Gras, Angeln …) – Schwert", "Sichtbar in der Spielwelt – Schwert",
                                 "Versteckt (Gras, Angeln …) – Schild", "Sichtbar in der Spielwelt – Schild"};

// ------------------------------------------------------------- Encounter-Log

QList<EncounterChange> encounterChanges(const EditSession& s) {
    QList<EncounterChange> changes;
    auto starterSlot = [](int i) {
        for (int k = 0; k < 3; k++) if (kStarterGiftIndexes[k] == i) return k;
        return -1;
    };
    for (int i = 0; i < s.gifts.size() && i < s.giftsOriginal.size(); i++) {
        const FlatRecord& g = s.gifts[i];
        const FlatRecord& o = s.giftsOriginal[i];
        if (sameRecord(g, o)) continue;
        const int slot = starterSlot(i);
        EncounterChange c;
        c.category = slot >= 0 ? 0 : 1;
        c.index = slot >= 0 ? slot : i;
        c.oldSpecies = static_cast<int>(o.get(gift::Species));
        c.oldForm = static_cast<int>(o.get(gift::Form));
        c.newSpecies = static_cast<int>(g.get(gift::Species));
        c.newForm = static_cast<int>(g.get(gift::Form));
        c.level = static_cast<int>(g.get(gift::Level));
        if (slot < 0 && g.get(gift::CanGigantamax) != 0) c.note = "Gigadynamax";
        changes.append(c);
    }
    for (int i = 0; i < s.statics.size() && i < s.staticsOriginal.size(); i++) {
        const FlatRecord& e = s.statics[i];
        const FlatRecord& o = s.staticsOriginal[i];
        if (sameRecord(e, o)) continue;
        EncounterChange c;
        c.category = staticCategory(o, i);
        c.index = i;
        c.oldSpecies = static_cast<int>(o.get(encounter::Species));
        c.oldForm = static_cast<int>(o.get(encounter::Form));
        c.newSpecies = static_cast<int>(e.get(encounter::Species));
        c.newForm = static_cast<int>(e.get(encounter::Form));
        c.level = static_cast<int>(e.get(encounter::Level));
        if (isDynamaxEncounter(o)) c.note = "Dynamax-Kampf";
        changes.append(c);
    }
    for (int i = 0; i < s.trades.size() && i < s.tradesOriginal.size(); i++) {
        const FlatRecord& t = s.trades[i];
        const FlatRecord& o = s.tradesOriginal[i];
        if (sameRecord(t, o)) continue;
        EncounterChange c;
        c.category = 4;
        c.index = i;
        c.oldSpecies = static_cast<int>(o.get(trade::Species));
        c.oldForm = static_cast<int>(o.get(trade::Form));
        c.newSpecies = static_cast<int>(t.get(trade::Species));
        c.newForm = static_cast<int>(t.get(trade::Form));
        c.level = static_cast<int>(t.get(trade::Level));
        c.requiredSpecies = static_cast<int>(t.get(trade::RequiredSpecies));
        changes.append(c);
    }
    return changes;
}

// Log der wilden Pokemon aus dem Vergleich mit dem Dump (enthaelt auch Aenderungen von Hand)
WildResult wildLog(const EditSession& s) {
    WildResult result;
    for (int f = 0; f < 4; f++) {
        const WildArchive& now = s.wild[f];
        const WildArchive& before = s.wildOriginal[f];
        for (int z = 0; z < now.zones.size() && z < before.zones.size(); z++) {
            const WildZone& zone = now.zones[z];
            const WildZone& old = before.zones[z];
            if (sameZone(zone, old)) continue;
            WildResult::ZoneLog log;
            log.symbol = (f % 2) == 1;
            log.shield = f >= 2;
            log.zoneId = zone.zoneId;
            log.levelMin = 100;
            QSet<qint64> logged;
            for (int t = 0; t < zone.subTables.size() && t < old.subTables.size(); t++) {
                const WildSubTable& sub = zone.subTables[t];
                const WildSubTable& subOld = old.subTables[t];
                for (int k = 0; k < sub.entries.size(); k++) {
                    const WildSlot& slot = sub.entries[k];
                    if (slot.species <= 0) continue;
                    const WildSlot from = k < subOld.entries.size() ? subOld.entries[k] : slot;
                    log.levelMin = qMin(log.levelMin, sub.levelMin);
                    log.levelMax = qMax(log.levelMax, sub.levelMax);
                    const qint64 key = (static_cast<qint64>(from.species) * 100 + from.form) * 100000 + slot.species * 100 + slot.form;
                    if (logged.contains(key)) continue;
                    logged.insert(key);
                    log.pairs.append({StarterChoice{from.species, from.form}, StarterChoice{slot.species, slot.form}});
                }
            }
            if (!log.pairs.isEmpty()) result.zones.append(log);
        }
    }
    return result;
}

// ----------------------------------------------------------- Feld-Helfer

// Feld eines FlatRecord (Zahl)
edit::Field recordNumber(const QString& label, QList<FlatRecord>* list, const QList<FlatRecord>* original, int index,
                         int field, int min, int max) {
    return edit::number(label, min, max,
                        [list, index, field]() { return static_cast<int>((*list)[index].get(field)); },
                        [list, index, field](int v) { (*list)[index].set(field, v); },
                        [original, index, field]() { return static_cast<int>((*original)[index].get(field)); });
}

edit::Field recordPicker(const QString& label, edit::Kind kind, QList<FlatRecord>* list, const QList<FlatRecord>* original,
                         int index, int field) {
    return edit::picker(label, kind,
                        [list, index, field]() { return static_cast<int>((*list)[index].get(field)); },
                        [list, index, field](int v) { (*list)[index].set(field, v); },
                        [original, index, field]() { return static_cast<int>((*original)[index].get(field)); });
}

edit::Field recordChoice(const QString& label, const QStringList& choices, QList<FlatRecord>* list,
                         const QList<FlatRecord>* original, int index, int field) {
    return edit::choice(label, choices,
                        [list, index, field]() { return static_cast<int>((*list)[index].get(field)); },
                        [list, index, field](int v) { (*list)[index].set(field, v); },
                        [original, index, field]() { return static_cast<int>((*original)[index].get(field)); });
}

edit::Field recordCheck(const QString& label, QList<FlatRecord>* list, const QList<FlatRecord>* original, int index,
                        int field) {
    return edit::check(label,
                       [list, index, field]() { return (*list)[index].get(field) != 0 ? 1 : 0; },
                       [list, index, field](int v) { (*list)[index].set(field, v ? 1 : 0); },
                       [original, index, field]() { return (*original)[index].get(field) != 0 ? 1 : 0; });
}

// Art + Form eines FlatRecord als ein Auswahlfeld
edit::Field recordPokemon(const QString& label, QList<FlatRecord>* list, const QList<FlatRecord>* original, int index,
                          int speciesField, int formField) {
    edit::Field f = edit::picker(label, edit::Kind::Pokemon,
        [list, index, speciesField, formField]() {
            return edit::pokemonValue(static_cast<int>((*list)[index].get(speciesField)),
                                      static_cast<int>((*list)[index].get(formField)));
        },
        [list, index, speciesField, formField](int v) {
            (*list)[index].set(speciesField, edit::speciesOf(v));
            (*list)[index].set(formField, edit::formOf(v));
        },
        [original, index, speciesField, formField]() {
            return edit::pokemonValue(static_cast<int>((*original)[index].get(speciesField)),
                                      static_cast<int>((*original)[index].get(formField)));
        });
    f.span = 2;
    return f;
}

const QString kIvInfo = "0–31. Negative Werte überlassen die DVs dem Spiel (−1 = zufällig).";

QList<edit::Field> recordIvs(QList<FlatRecord>* list, const QList<FlatRecord>* original, int index, const int fields[6]) {
    QList<edit::Field> out;
    for (int i = 0; i < 6; i++) {
        edit::Field f = recordNumber("DV " + kStatNames[i], list, original, index, fields[i], -4, 31);
        f.info = kIvInfo;
        out.append(f);
    }
    return out;
}

QString pokemonText(const EditSession& s, int species, int form) { return s.texts.pokemonName(species, form); }

// ------------------------------------------------------------ Trainer-Seite

edit::Section trainerSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Trainer";
    section.subtitle = "Alle Trainer mit ihren Teams. Fahre mit der Maus über ein Feld, um den Vorher-Wert zu sehen.";

    struct Item { int group; int index; };
    QList<Item> order;
    const QList<Trainer>& trainers = s->work.trainers.all();
    for (const Trainer& t : trainers) {
        if (t.isPlaceholder() || t.team.isEmpty()) continue;
        order.append({trainerGroup(t), t.index});
    }
    std::stable_sort(order.begin(), order.end(), [](const Item& a, const Item& b) { return a.group < b.group; });

    for (const Item& it : order) {
        const int ti = it.index;
        const Trainer& t = trainers[ti];
        int highest = 0;
        const Trainer* orig = originalTrainer(*s, ti);
        for (const TrainerPoke& p : (orig ? orig->team : t.team)) highest = qMax(highest, p.level());

        edit::Entry entry;
        entry.group = groupInfo(it.group).title;
        entry.title = s->texts.trainerLabel(t) + "  ·  Lv. " + QString::number(highest);
        entry.search = [s, ti]() {
            QString text = "#" + QString::number(ti);
            for (const TrainerPoke& p : s->work.trainers.all()[ti].team) {
                text += " " + s->texts.pokemonName(p.species(), p.form());
            }
            return text;
        };
        entry.changed = [s, ti]() {
            const Trainer* o = originalTrainer(*s, ti);
            return o && !sameTrainer(s->work.trainers.all()[ti], *o);
        };
        entry.build = [s, ti]() {
            edit::Content c;
            Trainer& trainer = s->work.trainers.all()[ti];
            const Trainer* o = originalTrainer(*s, ti);
            const int theme = trainerTheme(trainer);
            QStringList facts;
            facts << (trainer.battleMode() == 1 ? "Doppelkampf" : "Einzelkampf");
            if (theme >= 0) facts << "Typ-Trainer: " + s->texts.typeName(theme);
            facts << "#" + QString::number(ti);
            c.hint = facts.join(" · ");
            c.warnings = [s, ti]() {
                QStringList w;
                QHash<int, int> count;
                for (const TrainerPoke& p : s->work.trainers.all()[ti].team) count[p.species()]++;
                for (auto it = count.constBegin(); it != count.constEnd(); ++it)
                    if (it.value() > 1) w << s->texts.pokemonName(it.key(), 0) + " ist mehrfach im Team.";
                w.sort();
                return w;
            };

            edit::Table team;
            team.title = "Team";
            team.cards = true;
            team.columnsPerLine = 4;
            team.rowCount = [s, ti]() { return static_cast<int>(s->work.trainers.all()[ti].team.size()); };
            team.rowTitle = [s, ti](int r) {
                const TrainerPoke& p = s->work.trainers.all()[ti].team[r];
                return QString("Pokémon %1 – %2").arg(r + 1).arg(s->texts.pokemonName(p.species(), p.form()));
            };
            team.canAdd = [s, ti]() { return s->work.trainers.all()[ti].team.size() < 6; };
            team.add = [s, ti]() {
                QList<TrainerPoke>& list = s->work.trainers.all()[ti].team;
                if (list.size() < 6 && !list.isEmpty()) list.append(list.last());
            };
            team.canRemove = [s, ti]() { return s->work.trainers.all()[ti].team.size() > 1; };
            team.remove = [s, ti](int r) {
                QList<TrainerPoke>& list = s->work.trainers.all()[ti].team;
                if (list.size() > 1 && r >= 0 && r < list.size()) list.removeAt(r);
            };
            team.row = [s, ti, o](int r) {
                QList<edit::Field> fields;
                auto poke = [s, ti, r]() -> TrainerPoke& { return s->work.trainers.all()[ti].team[r]; };
                const bool hasOrig = o && r < o->team.size();
                auto origPoke = [o, r]() -> const TrainerPoke& { return o->team[r]; };
                auto orig = [hasOrig, origPoke](std::function<int(const TrainerPoke&)> fn) -> std::function<int()> {
                    if (!hasOrig) return {};
                    return [origPoke, fn]() { return fn(origPoke()); };
                };

                edit::Field species = edit::picker("Pokémon", edit::Kind::Pokemon,
                    [poke]() { return edit::pokemonValue(poke().species(), poke().form()); },
                    [poke](int v) {
                        TrainerPoke& p = poke();
                        p.setSpecies(edit::speciesOf(v));
                        p.setForm(edit::formOf(v));
                        if (!canGigantamax(edit::speciesOf(v), edit::formOf(v))) p.setCanGigantamax(false);
                    },
                    orig([](const TrainerPoke& p) { return edit::pokemonValue(p.species(), p.form()); }));
                species.span = 2;
                species.rebuild = true;
                fields << species;
                fields << edit::number("Level", 1, 100, [poke]() { return poke().level(); },
                                       [poke](int v) { poke().setLevel(v); },
                                       orig([](const TrainerPoke& p) { return p.level(); }));
                fields << edit::picker("Item", edit::Kind::Item, [poke]() { return poke().heldItem(); },
                                       [poke](int v) { poke().setHeldItem(v); },
                                       orig([](const TrainerPoke& p) { return p.heldItem(); }));
                for (int m = 0; m < 4; m++) {
                    fields << edit::picker(QString("Attacke %1").arg(m + 1), edit::Kind::Move,
                                           [poke, m]() { return poke().move(m); },
                                           [poke, m](int v) { poke().setMove(m, v); },
                                           orig([m](const TrainerPoke& p) { return p.move(m); }));
                }

                // Faehigkeit: Auswahl ueber die Faehigkeiten der aktuellen Art
                const TrainerPoke& current = poke();
                const int pi = s->work.personal.indexOf(current.species(), current.form());
                auto abilityName = [s, pi](int slot) {
                    const int id = pi > 0 ? s->work.personal.ability(pi, slot) : 0;
                    return id > 0 && id < s->texts.abilities.size() ? s->texts.abilities[id] : QString("?");
                };
                edit::Field ability = edit::choice("Fähigkeit",
                    {"Spiel-Standard", "1: " + abilityName(0), "2: " + abilityName(1), "Versteckt: " + abilityName(2)},
                    [poke]() { return poke().ability(); }, [poke](int v) { poke().setAbility(v); },
                    orig([](const TrainerPoke& p) { return p.ability(); }));
                ability.info = "Welche Fähigkeit das Pokémon hat. Die Namen gehören zur aktuell gewählten Art.";
                fields << ability;
                fields << edit::choice("Wesen", natureChoicesWithRandom(),
                                       [poke]() { return u8(poke().raw, 0x01); },
                                       [poke](int v) { setU8(poke().raw, 0x01, v); },
                                       orig([](const TrainerPoke& p) { return u8(p.raw, 0x01); }));
                fields << edit::choice("Geschlecht", genderChoices(), [poke]() { return poke().gender(); },
                                       [poke](int v) { poke().setGender(v); },
                                       orig([](const TrainerPoke& p) { return p.gender(); }));
                fields << edit::check("Schillernd", [poke]() { return poke().shiny() ? 1 : 0; },
                                      [poke](int v) { poke().setShiny(v != 0); },
                                      orig([](const TrainerPoke& p) { return p.shiny() ? 1 : 0; }));
                fields << edit::number("Dyna-Level", 0, 10, [poke]() { return poke().dynamaxLevel(); },
                                       [poke](int v) { poke().setDynamaxLevel(v); },
                                       orig([](const TrainerPoke& p) { return p.dynamaxLevel(); }));
                fields << edit::check("Gigadynamax", [poke]() { return poke().canGigantamax() ? 1 : 0; },
                                      [poke](int v) { poke().setCanGigantamax(v != 0); },
                                      orig([](const TrainerPoke& p) { return p.canGigantamax() ? 1 : 0; }));
                edit::Field dyn = edit::check("Darf dynamaximieren", [poke]() { return poke().dynamaxAllowed() ? 1 : 0; },
                                              [poke](int v) { poke().setDynamaxAllowed(v != 0); },
                                              orig([](const TrainerPoke& p) { return p.dynamaxAllowed() ? 1 : 0; }));
                dyn.info = "Der Trainer darf dieses Pokémon im Stadion dynamaximieren.";
                fields << dyn;
                fields << edit::text("", []() { return QString(); });
                for (int i = 0; i < 6; i++) {
                    const int bit = kIvBit[i];
                    fields << edit::number("DV " + kStatNames[i], 0, 31, [poke, bit]() { return ivAt(poke().raw, bit); },
                                           [poke, bit](int v) { setIvAt(poke().raw, bit, v); },
                                           orig([bit](const TrainerPoke& p) { return ivAt(p.raw, bit); }));
                }
                for (int i = 0; i < 6; i++) {
                    const int pos = kEvByte[i];
                    fields << edit::number("FP " + kStatNames[i], 0, 252, [poke, pos]() { return u8(poke().raw, pos); },
                                           [poke, pos](int v) { setU8(poke().raw, pos, v); },
                                           orig([pos](const TrainerPoke& p) { return u8(p.raw, pos); }));
                }
                return fields;
            };
            c.tables << team;
            return c;
        };
        section.entries.append(entry);
    }
    return section;
}

// ---------------------------------------------------------- Wilde Pokemon

edit::Section wildSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Wilde Pokémon";
    section.subtitle = "Pokémon pro Gebiet und Wetter. Schwert und Schild haben eigene Tabellen, der Mod enthält beide.";

    for (int f = 0; f < 4; f++) {
        QHash<QString, int> seen;
        for (int z = 0; z < s->wild[f].zones.size(); z++) {
            const WildZone& zone = s->wild[f].zones[z];
            bool any = false;
            for (const WildSubTable& t : zone.subTables)
                for (const WildSlot& slot : t.entries) any = any || slot.species > 0;
            if (!any) continue;

            QString place = zoneName(zone.zoneId, s->placeNames);
            const int n = ++seen[place];
            if (n > 1) place += QString(" (%1)").arg(n);

            edit::Entry entry;
            entry.group = kWildGroups[f];
            entry.title = place;
            entry.search = [s, f, z]() {
                QString text;
                for (const WildSubTable& t : s->wild[f].zones[z].subTables)
                    for (const WildSlot& slot : t.entries)
                        if (slot.species > 0) text += " " + s->texts.pokemonName(slot.species, slot.form);
                return text;
            };
            entry.changed = [s, f, z]() {
                return z < s->wildOriginal[f].zones.size() && !sameZone(s->wild[f].zones[z], s->wildOriginal[f].zones[z]);
            };
            entry.build = [s, f, z]() {
                edit::Content c;
                c.hint = "Chance in Prozent pro Platz. Leere Plätze (ohne Pokémon) werden nicht angezeigt.";
                WildZone& zone = s->wild[f].zones[z];
                const bool hasOrig = z < s->wildOriginal[f].zones.size();
                for (int t = 0; t < zone.subTables.size(); t++) {
                    bool any = false;
                    for (const WildSlot& slot : zone.subTables[t].entries) any = any || slot.species > 0;
                    if (!any) continue;
                    auto sub = [s, f, z, t]() -> WildSubTable& { return s->wild[f].zones[z].subTables[t]; };
                    auto subOrig = [s, f, z, t]() -> const WildSubTable& { return s->wildOriginal[f].zones[z].subTables[t]; };
                    const bool tableOrig = hasOrig && t < s->wildOriginal[f].zones[z].subTables.size();

                    edit::Table table;
                    table.title = t < kWeatherNames.size() ? kWeatherNames[t] : QString("Tabelle %1").arg(t + 1);
                    table.columns = {"Level von", "Level bis", "Pokémon", "Chance (%)"};
                    QList<int> visible;
                    for (int k = 0; k < zone.subTables[t].entries.size(); k++) {
                        if (zone.subTables[t].entries[k].species > 0) visible.append(k);
                    }
                    table.rowCount = [visible]() { return static_cast<int>(visible.size()); };
                    table.row = [sub, subOrig, tableOrig, visible](int r) {
                        const int k = visible[r];
                        const bool slotOrig = tableOrig && k < subOrig().entries.size();
                        QList<edit::Field> fields;
                        // Level gilt fuer die ganze Tabelle: nur in der ersten Zeile aenderbar
                        if (r == 0) {
                            fields << edit::number("Level von", 1, 100, [sub]() { return sub().levelMin; },
                                                   [sub](int v) { sub().levelMin = v; },
                                                   tableOrig ? std::function<int()>([subOrig]() { return subOrig().levelMin; })
                                                             : std::function<int()>());
                            fields << edit::number("Level bis", 1, 100, [sub]() { return sub().levelMax; },
                                                   [sub](int v) { sub().levelMax = v; },
                                                   tableOrig ? std::function<int()>([subOrig]() { return subOrig().levelMax; })
                                                             : std::function<int()>());
                        } else {
                            fields << edit::text("", []() { return QString(); }) << edit::text("", []() { return QString(); });
                        }
                        fields << edit::picker("Pokémon", edit::Kind::Pokemon,
                            [sub, k]() { return edit::pokemonValue(sub().entries[k].species, sub().entries[k].form); },
                            [sub, k](int v) {
                                if (edit::speciesOf(v) <= 0) return;
                                sub().entries[k].species = edit::speciesOf(v);
                                sub().entries[k].form = edit::formOf(v);
                            },
                            slotOrig ? std::function<int()>([subOrig, k]() {
                                return edit::pokemonValue(subOrig().entries[k].species, subOrig().entries[k].form);
                            }) : std::function<int()>());
                        fields << edit::number("Chance", 0, 100, [sub, k]() { return sub().entries[k].probability; },
                                               [sub, k](int v) { sub().entries[k].probability = v; },
                                               slotOrig ? std::function<int()>([subOrig, k]() { return subOrig().entries[k].probability; })
                                                        : std::function<int()>());
                        return fields;
                    };
                    c.tables << table;
                }
                return c;
            };
            section.entries.append(entry);
        }
    }
    return section;
}

// ------------------------------------------- Starter, Geschenke, Begegnungen, Tausch

edit::Section encounterSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Starter & Geschenke";
    section.subtitle = "Starter, geschenkte Pokémon, Legendäre und feste Begegnungen sowie Tauschpartner.";

    // Starter und Geschenke
    for (int i = 0; i < s->gifts.size() && i < s->giftsOriginal.size(); i++) {
        const FlatRecord& o = s->giftsOriginal[i];
        if (o.get(gift::Species) <= 0) continue;
        int starter = -1;
        for (int k = 0; k < 3; k++) if (kStarterGiftIndexes[k] == i) starter = k;

        edit::Entry entry;
        entry.group = starter >= 0 ? "Starter" : "Geschenkte Pokémon";
        entry.title = pokemonText(*s, static_cast<int>(o.get(gift::Species)), static_cast<int>(o.get(gift::Form))) +
                      "  ·  Lv. " + QString::number(o.get(gift::Level));
        entry.search = [s, i]() {
            return s->texts.pokemonName(static_cast<int>(s->gifts[i].get(gift::Species)),
                                        static_cast<int>(s->gifts[i].get(gift::Form)));
        };
        entry.changed = [s, i]() { return !sameRecord(s->gifts[i], s->giftsOriginal[i]); };
        entry.build = [s, i, starter]() {
            edit::Content c;
            QList<FlatRecord>* l = &s->gifts;
            const QList<FlatRecord>* o = &s->giftsOriginal;
            c.fieldsPerLine = 4;
            c.warnings = [s, i, starter]() {
                QStringList w;
                const int sp = static_cast<int>(s->gifts[i].get(gift::Species));
                if (starter >= 0) {
                    for (int k = 0; k < 3; k++) {
                        const int other = kStarterGiftIndexes[k];
                        if (k != starter && other < s->gifts.size() && s->gifts[other].get(gift::Species) == sp) {
                            w << "Ein anderer Starter ist schon " + s->texts.pokemonName(sp, 0) + ".";
                        }
                    }
                } else if ((i == 13 || i == 16 || i == 17) && !sameRecord(s->gifts[i], s->giftsOriginal[i])) {
                    w << "Dieses Geschenk gehört zur Geschichte. Ein anderes Pokémon kann Zwischensequenzen stören.";
                }
                return w;
            };
            if (starter >= 0) {
                c.hint = "Starter bei Delion. Das Modell auf dem Tisch zeigt beim Export automatisch das neue Pokémon.";
            } else if (i == 13 || i == 16 || i == 17) {
                c.hint = "Achtung: Dieses Geschenk gehört zur Geschichte (wird vom Randomizer nie geändert).";
            }
            c.fields << recordPokemon("Pokémon", l, o, i, gift::Species, gift::Form);
            c.fields << recordNumber("Level", l, o, i, gift::Level, 1, 100);
            c.fields << recordPicker("Item", edit::Kind::Item, l, o, i, gift::HeldItem);
            c.fields << recordChoice("Wesen", natureChoicesWithRandom(), l, o, i, gift::Nature);
            c.fields << recordChoice("Geschlecht", genderChoices(), l, o, i, gift::Gender);
            c.fields << recordChoice("Schillernd", shinyChoices(), l, o, i, gift::ShinyLock);
            edit::Field ability = recordNumber("Fähigkeit (Nr.)", l, o, i, gift::Ability, -1, 4);
            ability.info = "Fähigkeit als Zahl, wie im Spiel gespeichert. Im Original kommen 0 bis 3 vor (0 = Fähigkeit 1 oder 2 zufällig).";
            c.fields << ability;
            c.fields << recordPicker("Spezial-Attacke", edit::Kind::Move, l, o, i, gift::SpecialMove);
            c.fields << recordNumber("Dyna-Level", l, o, i, gift::DynamaxLevel, 0, 10);
            c.fields << recordCheck("Gigadynamax", l, o, i, gift::CanGigantamax);
            c.fields << recordCheck("Als Ei", l, o, i, gift::IsEgg);
            c.fields << recordNumber("Ball (Nr.)", l, o, i, gift::Ball, 0, 30);
            const int ivs[6] = {gift::IvHp, gift::IvAtk, gift::IvDef, gift::IvSpa, gift::IvSpd, gift::IvSpe};
            c.fields << recordIvs(l, o, i, ivs);
            return c;
        };
        section.entries.append(entry);
    }

    // Statische Begegnungen
    for (int i = 0; i < s->statics.size() && i < s->staticsOriginal.size(); i++) {
        const FlatRecord& o = s->staticsOriginal[i];
        if (o.get(encounter::Species) <= 0) continue;
        edit::Entry entry;
        entry.group = staticCategory(o, i) == 2 ? "Legendäre und Story-Begegnungen" : "Feste Pokémon in der Spielwelt";
        entry.title = pokemonText(*s, static_cast<int>(o.get(encounter::Species)), static_cast<int>(o.get(encounter::Form))) +
                      "  ·  Lv. " + QString::number(o.get(encounter::Level));
        entry.search = [s, i]() {
            return s->texts.pokemonName(static_cast<int>(s->statics[i].get(encounter::Species)),
                                        static_cast<int>(s->statics[i].get(encounter::Form))) + " #" + QString::number(i);
        };
        entry.changed = [s, i]() { return !sameRecord(s->statics[i], s->staticsOriginal[i]); };
        entry.build = [s, i]() {
            edit::Content c;
            QList<FlatRecord>* l = &s->statics;
            const QList<FlatRecord>* o = &s->staticsOriginal;
            c.fieldsPerLine = 4;
            QStringList hints;
            hints << "#" + QString::number(i);
            if (isDynamaxEncounter((*o)[i])) hints << "Dynamax-Kampf: das Pokémon muss dynamaximieren können";
            c.hint = hints.join(" · ");
            c.warnings = [s, i]() {
                QStringList w;
                const FlatRecord& o = s->staticsOriginal[i];
                const FlatRecord& r = s->statics[i];
                const int origSpecies = static_cast<int>(o.get(encounter::Species));
                if (origSpecies >= 888 && origSpecies <= 892 && r.get(encounter::Species) != o.get(encounter::Species)) {
                    w << "Story-kritisches Pokémon: ein Wechsel kann Zwischensequenzen stören (der Randomizer ändert es nie).";
                }
                if (isDynamaxEncounter(o)) {
                    const int index = s->work.personal.indexOf(static_cast<int>(r.get(encounter::Species)),
                                                               static_cast<int>(r.get(encounter::Form)));
                    if (index > 0 && !s->work.personal.canDynamax(index)) {
                        w << s->texts.pokemonName(static_cast<int>(r.get(encounter::Species)), static_cast<int>(r.get(encounter::Form))) +
                                 " kann nicht dynamaximieren, der Kampf funktioniert so nicht richtig.";
                    }
                }
                return w;
            };
            c.fields << recordPokemon("Pokémon", l, o, i, encounter::Species, encounter::Form);
            c.fields << recordNumber("Level", l, o, i, encounter::Level, 1, 100);
            c.fields << recordPicker("Item", edit::Kind::Item, l, o, i, encounter::HeldItem);
            c.fields << recordPicker("Attacke 1", edit::Kind::Move, l, o, i, encounter::Move0);
            c.fields << recordPicker("Attacke 2", edit::Kind::Move, l, o, i, encounter::Move1);
            c.fields << recordPicker("Attacke 3", edit::Kind::Move, l, o, i, encounter::Move2);
            c.fields << recordPicker("Attacke 4", edit::Kind::Move, l, o, i, encounter::Move3);
            c.fields << recordChoice("Wesen", natureChoicesWithRandom(), l, o, i, encounter::Nature);
            c.fields << recordChoice("Geschlecht", genderChoices(), l, o, i, encounter::Gender);
            c.fields << recordChoice("Schillernd", shinyChoices(), l, o, i, encounter::ShinyLock);
            edit::Field ability = recordNumber("Fähigkeit (Nr.)", l, o, i, encounter::Ability, -1, 4);
            ability.info = "Fähigkeit als Zahl, wie im Spiel gespeichert. Im Original kommen 0 bis 3 vor (0 = Fähigkeit 1 oder 2 zufällig).";
            c.fields << ability;
            c.fields << recordNumber("Dyna-Level", l, o, i, encounter::DynamaxLevel, 0, 10);
            c.fields << recordCheck("Gigadynamax", l, o, i, encounter::CanGigantamax);
            const int ivs[6] = {encounter::IvHp, encounter::IvAtk, encounter::IvDef, encounter::IvSpa, encounter::IvSpd,
                                encounter::IvSpe};
            c.fields << recordIvs(l, o, i, ivs);
            const int evs[6] = {encounter::EvHp, encounter::EvAtk, encounter::EvDef, encounter::EvSpa, encounter::EvSpd,
                                encounter::EvSpe};
            for (int k = 0; k < 6; k++) c.fields << recordNumber("FP " + kStatNames[k], l, o, i, evs[k], 0, 252);
            return c;
        };
        section.entries.append(entry);
    }

    // Tausch
    for (int i = 0; i < s->trades.size() && i < s->tradesOriginal.size(); i++) {
        const FlatRecord& o = s->tradesOriginal[i];
        if (o.get(trade::Species) <= 0) continue;
        edit::Entry entry;
        entry.group = "Tausch";
        entry.title = pokemonText(*s, static_cast<int>(o.get(trade::Species)), static_cast<int>(o.get(trade::Form))) +
                      " für " + pokemonText(*s, static_cast<int>(o.get(trade::RequiredSpecies)),
                                            static_cast<int>(o.get(trade::RequiredForm)));
        entry.search = [s, i]() {
            return s->texts.pokemonName(static_cast<int>(s->trades[i].get(trade::Species)),
                                        static_cast<int>(s->trades[i].get(trade::Form)));
        };
        entry.changed = [s, i]() { return !sameRecord(s->trades[i], s->tradesOriginal[i]); };
        entry.build = [s, i]() {
            edit::Content c;
            QList<FlatRecord>* l = &s->trades;
            const QList<FlatRecord>* o = &s->tradesOriginal;
            c.fieldsPerLine = 4;
            c.hint = "Der Tauschpartner nennt im Dialog automatisch sein neues Pokémon.";
            if (i == 8 || i == 9) c.hint += " Tausch 8 und 9 teilen sich einen Dialog – am besten beide gleich setzen.";
            c.fields << recordPokemon("Pokémon (bekommst du)", l, o, i, trade::Species, trade::Form);
            c.fields << recordPokemon("Gewünschtes Pokémon", l, o, i, trade::RequiredSpecies, trade::RequiredForm);
            c.fields << recordNumber("Level", l, o, i, trade::Level, 1, 100);
            c.fields << recordPicker("Item", edit::Kind::Item, l, o, i, trade::HeldItem);
            c.fields << recordChoice("Wesen", natureChoicesWithRandom(), l, o, i, trade::Nature);
            c.fields << recordChoice("Geschlecht", genderChoices(), l, o, i, trade::Gender);
            c.fields << recordChoice("Schillernd", shinyChoices(), l, o, i, trade::ShinyLock);
            edit::Field ability = recordNumber("Fähigkeit (Nr.)", l, o, i, trade::AbilityNumber, -1, 4);
            ability.info = "Fähigkeit als Zahl, wie im Spiel gespeichert. Im Original kommen 0 bis 3 vor (0 = Fähigkeit 1 oder 2 zufällig).";
            c.fields << ability;
            c.fields << recordNumber("Dyna-Level", l, o, i, trade::DynamaxLevel, 0, 10);
            c.fields << recordCheck("Gigadynamax", l, o, i, trade::CanGigantamax);
            for (int k = 0; k < 4; k++) {
                c.fields << recordPicker(QString("Attacke %1").arg(k + 1), edit::Kind::Move, l, o, i, trade::Relearn1 + k);
            }
            const int ivs[6] = {trade::IvHp, trade::IvAtk, trade::IvDef, trade::IvSpa, trade::IvSpd, trade::IvSpe};
            c.fields << recordIvs(l, o, i, ivs);
            return c;
        };
        section.entries.append(entry);
    }
    return section;
}

// ------------------------------------------------------------- Dyna-Raids

bool sameNests(const QList<NestTable>& a, const QList<NestTable>& b) {
    if (a.size() != b.size()) return false;
    for (int t = 0; t < a.size(); t++) {
        if (a[t].entries.size() != b[t].entries.size()) return false;
        for (int e = 0; e < a[t].entries.size(); e++) {
            const NestEntry& x = a[t].entries[e];
            const NestEntry& y = b[t].entries[e];
            if (x.species != y.species || x.form != y.form || x.gigantamax != y.gigantamax || x.ability != y.ability ||
                x.gender != y.gender || x.flawlessIVs != y.flawlessIVs || x.probabilities != y.probabilities) {
                return false;
            }
        }
    }
    return true;
}

bool sameNestTable(const NestTable& a, const NestTable& b) { return sameNests({a}, {b}); }

edit::Section raidSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Dyna-Raids";
    section.subtitle = "Pokémon in den Dyna-Nestern. Die Chancen gelten je Sternzahl (1–5 ★).";
    int perVersion[3] = {0, 0, 0};
    for (int t = 0; t < s->nests.size(); t++) {
        const int version = qBound(0, static_cast<int>(s->nests[t].version), 2);
        const int number = ++perVersion[version];
        edit::Entry entry;
        entry.group = version == 2 ? "Schild" : (version == 1 ? "Schwert" : "Beide Editionen");
        entry.title = QString("Nest-Tabelle %1").arg(number);
        entry.search = [s, t]() {
            QString text;
            for (const NestEntry& n : s->nests[t].entries)
                if (n.species > 0) text += " " + s->texts.pokemonName(static_cast<int>(n.species), static_cast<int>(n.form));
            return text;
        };
        entry.changed = [s, t]() { return t < s->nestsOriginal.size() && !sameNestTable(s->nests[t], s->nestsOriginal[t]); };
        entry.build = [s, t]() {
            edit::Content c;
            c.hint = "G-Max = Gigadynamax-Raid. Perfekte DVs = Anzahl der DVs mit 31.";
            c.warnings = [s, t]() {
                QStringList w;
                QHash<int, int> count;
                for (const NestEntry& n : s->nests[t].entries) {
                    if (n.species <= 0) continue;
                    count[static_cast<int>(n.species)]++;
                    if (n.gigantamax && !canGigantamax(static_cast<int>(n.species), static_cast<int>(n.form))) {
                        w << s->texts.pokemonName(static_cast<int>(n.species), static_cast<int>(n.form)) +
                                 " hat keine Gigadynamax-Form.";
                    }
                }
                for (auto it = count.constBegin(); it != count.constEnd(); ++it)
                    if (it.value() > 1) w << s->texts.pokemonName(it.key(), 0) + " kommt mehrfach in dieser Tabelle vor.";
                w.sort();
                return w;
            };
            edit::Table table;
            table.title = "Pokémon";
            table.columns = {"Pokémon", "G-Max", "1★", "2★", "3★", "4★", "5★", "Perfekte DVs"};
            QList<int> visible;
            for (int e = 0; e < s->nests[t].entries.size(); e++)
                if (s->nests[t].entries[e].species > 0) visible.append(e);
            table.rowCount = [visible]() { return static_cast<int>(visible.size()); };
            table.row = [s, t, visible](int r) {
                const int e = visible[r];
                auto n = [s, t, e]() -> NestEntry& { return s->nests[t].entries[e]; };
                const bool hasOrig = t < s->nestsOriginal.size() && e < s->nestsOriginal[t].entries.size();
                auto o = [s, t, e]() -> const NestEntry& { return s->nestsOriginal[t].entries[e]; };
                auto orig = [hasOrig, o](std::function<int(const NestEntry&)> fn) -> std::function<int()> {
                    if (!hasOrig) return {};
                    return [o, fn]() { return fn(o()); };
                };
                QList<edit::Field> fields;
                fields << edit::picker("Pokémon", edit::Kind::Pokemon,
                    [n]() { return edit::pokemonValue(static_cast<int>(n().species), static_cast<int>(n().form)); },
                    [n](int v) {
                        if (edit::speciesOf(v) <= 0) return;
                        n().species = edit::speciesOf(v);
                        n().form = edit::formOf(v);
                        // Psiaugon/Servol: Geschlecht haengt an der Form (wie beim Randomizer)
                        const int sp = edit::speciesOf(v), fm = edit::formOf(v);
                        n().gender = (sp == 678 || sp == 876) ? (fm == 1 ? 2 : 1) : 0;
                    },
                    orig([](const NestEntry& x) { return edit::pokemonValue(static_cast<int>(x.species), static_cast<int>(x.form)); }));
                fields << edit::check("G-Max", [n]() { return n().gigantamax ? 1 : 0; }, [n](int v) { n().gigantamax = v ? 1 : 0; },
                                      orig([](const NestEntry& x) { return x.gigantamax ? 1 : 0; }));
                for (int k = 0; k < 5; k++) {
                    fields << edit::number(QString("%1★").arg(k + 1), 0, 100,
                        [n, k]() { return k < n().probabilities.size() ? static_cast<int>(n().probabilities[k]) : 0; },
                        [n, k](int v) { if (k < n().probabilities.size()) n().probabilities[k] = v; },
                        orig([k](const NestEntry& x) { return k < x.probabilities.size() ? static_cast<int>(x.probabilities[k]) : 0; }));
                }
                fields << edit::number("Perfekte DVs", 0, 6, [n]() { return static_cast<int>(n().flawlessIVs); },
                                       [n](int v) { n().flawlessIVs = v; },
                                       orig([](const NestEntry& x) { return static_cast<int>(x.flawlessIVs); }));
                return fields;
            };
            c.tables << table;
            return c;
        };
        section.entries.append(entry);
    }
    return section;
}

// ------------------------------------------------------------------ Items

bool sameSpots(const QList<ItemSpot>& a, const QList<ItemSpot>& b) {
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); i++) {
        if (a[i].item != b[i].item || a[i].quantity != b[i].quantity) return false;
    }
    return true;
}

bool sameShop(const QList<ShopSlot>& a, const QList<ShopSlot>& b) {
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); i++) {
        if (a[i].item != b[i].item) return false;
    }
    return true;
}

edit::Section itemSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Items";
    section.subtitle = "Items auf dem Boden, versteckte Items und die Angebote der Läden.";
    if (!s->itemInfoLoaded) return section;

    // Boden und versteckt, pro Gebiet
    for (int hidden = 0; hidden <= 1; hidden++) {
        QList<quint64> zones;
        QHash<quint64, QList<int>> byZone;
        for (int i = 0; i < s->spots.size(); i++) {
            if (s->spots[i].hidden != (hidden == 1)) continue;
            const quint64 z = s->spots[i].zoneId;
            if (!byZone.contains(z)) zones.append(z);
            byZone[z].append(i);
        }
        QHash<QString, int> seen;
        for (quint64 z : zones) {
            const QList<int> indexes = byZone.value(z);
            QString place = zoneName(z, s->placeNames);
            if (place.startsWith("GEBIET ", Qt::CaseInsensitive)) {
                // Gebiete ohne Ortsnamen im Spiel (z. B. Gebaeude): kurze Kennung statt der langen Zahl
                place = "Ort ohne Namen " + QString("%1").arg(z, 16, 16, QChar('0')).toUpper().left(4);
            }
            const int n = ++seen[place];
            if (n > 1) place += QString(" (%1)").arg(n);
            edit::Entry entry;
            entry.group = hidden ? "Versteckte Items" : "Items auf dem Boden";
            entry.title = place;
            entry.search = [s, indexes]() {
                QString text;
                for (int i : indexes) text += " " + s->texts.itemName(s->spots[i].item);
                return text;
            };
            entry.changed = [s, indexes]() {
                for (int i : indexes) {
                    if (i < s->spotsOriginal.size() &&
                        (s->spots[i].item != s->spotsOriginal[i].item || s->spots[i].quantity != s->spotsOriginal[i].quantity)) {
                        return true;
                    }
                }
                return false;
            };
            entry.build = [s, indexes, hidden]() {
                edit::Content c;
                c.hint = hidden ? "Versteckte Items (glitzernde Stellen). Ein Fundort kann mehrere mögliche Items haben."
                                : "Items in Poké-Bällen auf dem Boden.";
                c.warnings = [s, indexes]() {
                    QStringList w;
                    for (int i : indexes) {
                        if (i < s->spotsOriginal.size() && s->spots[i].item != s->spotsOriginal[i].item &&
                            s->itemInfo.isProtected(s->spotsOriginal[i].item)) {
                            w << "Hier lag im Original " + s->texts.itemName(s->spotsOriginal[i].item) +
                                     " (TM oder Basis-Item). Ohne es fehlt es dir eventuell im Spiel.";
                        }
                    }
                    return w;
                };
                edit::Table table;
                table.title = "Items";
                table.columns = {"Item", "Anzahl", "Hinweis"};
                table.rowCount = [indexes]() { return static_cast<int>(indexes.size()); };
                table.row = [s, indexes](int r) {
                    const int i = indexes[r];
                    const bool hasOrig = i < s->spotsOriginal.size();
                    QList<edit::Field> fields;
                    fields << edit::picker("Item", edit::Kind::Item, [s, i]() { return s->spots[i].item; },
                        [s, i](int v) { if (v > 0 && s->itemInfo.hashById.contains(v)) s->spots[i].item = v; },
                        hasOrig ? std::function<int()>([s, i]() { return s->spotsOriginal[i].item; }) : std::function<int()>());
                    edit::Field q = edit::number("Anzahl", 1, 99, [s, i]() { return s->spots[i].quantity; },
                        [s, i](int v) { s->spots[i].quantity = v; },
                        hasOrig ? std::function<int()>([s, i]() { return s->spotsOriginal[i].quantity; }) : std::function<int()>());
                    if (s->spots[i].quantityPos <= 0) q.set = {};
                    fields << q;
                    const int originalItem = hasOrig ? s->spotsOriginal[i].item : 0;
                    const bool protectedItem = s->itemInfo.isProtected(originalItem);
                    fields << edit::text("", [protectedItem]() {
                        return protectedItem ? QString("TM oder Basis-Item im Original") : QString();
                    });
                    return fields;
                };
                c.tables << table;
                return c;
            };
            section.entries.append(entry);
        }
    }

    // Laeden
    QList<QPair<int, int>> shops; // (Laden, Stufe)
    QHash<qint64, QList<int>> slotsOf;
    for (int i = 0; i < s->shopSlots.size(); i++) {
        const qint64 key = static_cast<qint64>(s->shopSlots[i].shop) * 100 + (s->shopSlots[i].stage + 1);
        if (!slotsOf.contains(key)) shops.append({s->shopSlots[i].shop, s->shopSlots[i].stage});
        slotsOf[key].append(i);
    }
    for (const auto& shop : shops) {
        const QList<int> indexes = slotsOf.value(static_cast<qint64>(shop.first) * 100 + (shop.second + 1));
        edit::Entry entry;
        entry.group = "Läden";
        entry.title = QString("Laden %1").arg(shop.first + 1) +
                      (shop.second >= 0 ? QString(" – Stufe %1").arg(shop.second + 1) : QString());
        entry.search = [s, indexes]() {
            QString text;
            for (int i : indexes) text += " " + s->texts.itemName(s->shopSlots[i].item);
            return text;
        };
        entry.changed = [s, indexes]() {
            for (int i : indexes)
                if (i < s->shopSlotsOriginal.size() && s->shopSlots[i].item != s->shopSlotsOriginal[i].item) return true;
            return false;
        };
        entry.build = [s, indexes, shop]() {
            edit::Content c;
            c.hint = shop.second >= 0 ? "Pokémon-Center-Laden: das Angebot wächst mit der Anzahl der Orden (Stufe)."
                                      : "Fester Laden. Die Anzahl der Angebote ist fest, die Items kannst du tauschen.";
            QStringList first;
            for (int i : indexes.mid(0, 4)) first << s->texts.itemName(s->shopSlotsOriginal.value(i).item);
            c.hint += "\nIm Original: " + first.join(", ") + (indexes.size() > 4 ? " …" : "");
            edit::Table table;
            table.title = "Angebote";
            table.columns = {"Item"};
            table.rowCount = [indexes]() { return static_cast<int>(indexes.size()); };
            table.row = [s, indexes](int r) {
                const int i = indexes[r];
                const bool hasOrig = i < s->shopSlotsOriginal.size();
                return QList<edit::Field>{edit::picker("Item", edit::Kind::Item, [s, i]() { return s->shopSlots[i].item; },
                    [s, i](int v) { if (v > 0) s->shopSlots[i].item = v; },
                    hasOrig ? std::function<int()>([s, i]() { return s->shopSlotsOriginal[i].item; }) : std::function<int()>())};
            };
            c.tables << table;
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
        "Level (Eisfels)", "Level (Konsole kopfüber)", "Zuneigung + Attackentyp", "Level mit Unlicht-Pokémon",
        "Level bei Regen", "Level (Tag)", "Level (Nacht)", "Level (weiblich, Form 1)", "(unbenutzt)", "Level (Edition)",
        "Level (Edition, Tag)", "Level (Edition, Nacht)", "Level (Gipfel)", "Level (Abend)", "Level (Ultrapforte)",
        "Item (Ultrapforte)", "Volltreffer im Kampf", "KP im Kampf verloren", "Drehen", "Level (Wesen Hoch)",
        "Level (Wesen Tief)", "Turm des Unlichts", "Turm des Wassers"};
    QStringList out;
    for (int i = 0; i < names.size(); i++) out << QString("%1: %2").arg(i).arg(names[i]);
    return out;
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

bool samePersonalEntry(const EditSession& s, int index) {
    const QByteArray a = s.work.personal.save().mid(index * kPersonalSize, kPersonalSize);
    const QByteArray b = s.original.personal.save().mid(index * kPersonalSize, kPersonalSize);
    return a == b && s.work.learnsets.get(index) == s.original.learnsets.get(index) &&
           sameEvolutions(s.work.evolutions.get(index), s.original.evolutions.get(index));
}

edit::Section dataSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Pokémon-Daten";
    section.subtitle = "Typen, Basiswerte, Fähigkeiten, Attacken, TMs/TPs und Entwicklungen jedes Pokémon.";
    static const QList<QPair<int, QString>> generations = {
        {151, "Kanto (1–151)"}, {251, "Johto (152–251)"}, {386, "Hoenn (252–386)"}, {493, "Sinnoh (387–493)"},
        {649, "Einall (494–649)"}, {721, "Kalos (650–721)"}, {809, "Alola (722–809)"}, {898, "Galar (810–898)"}};
    const PersonalTable& p = s->work.personal;
    for (int species = 1; species <= kMaxSpecies; species++) {
        const int forms = qMax(1, p.formCount(p.indexOf(species, 0)));
        for (int form = 0; form < forms; form++) {
            const int index = p.indexOf(species, form);
            if (index <= 0 || !p.isPresent(index)) continue;
            edit::Entry entry;
            for (const auto& g : generations) {
                if (species <= g.first) {
                    entry.group = g.second;
                    break;
                }
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
                                         [w, index](int v) { w->setTypes(index, v, w->type2(index)); },
                                         [o, index]() { return o->type1(index); });
                c.fields << edit::picker("Typ 2", edit::Kind::Type, [w, index]() { return w->type2(index); },
                                         [w, index](int v) { w->setTypes(index, w->type1(index), v); },
                                         [o, index]() { return o->type2(index); });
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
                static const int order[6] = {0, 1, 2, 4, 5, 3};  // KP, Ang, Vert, SpAng, SpVert, Init
                for (int k = 0; k < 6; k++) {
                    const int st = order[k];
                    edit::Field f = edit::number(kStatNames[k], 1, 255, [w, index, st]() { return w->stat(index, st); },
                                                 [w, index, st](int v) { w->setStat(index, st, v); },
                                                 [o, index, st]() { return o->stat(index, st); });
                    f.rebuild = true;   // Summe aktualisieren
                    c.fields << f;
                }

                // Level-Attacken
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
                moves.canAdd = [lw, index]() { return lw->get(index).size() < kLearnsetSize / 4; };
                moves.add = [lw, index]() {
                    auto l = lw->get(index);
                    const int level = l.isEmpty() ? 1 : l.last().second;
                    l.append({1, level});
                    lw->set(index, l);
                };
                moves.canRemove = [lw, index]() { return lw->get(index).size() > 1; };
                moves.remove = [lw, index](int r) {
                    auto l = lw->get(index);
                    if (r >= 0 && r < l.size()) l.removeAt(r);
                    lw->set(index, l);
                };
                c.tables << moves;

                // Entwicklungen
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
                                           [update](int v) { update([v](Evolution& e) { e.method = v; }); },
                                           orig([](const Evolution& e) { return e.method; }));
                    edit::Field arg = edit::number("Wert", 0, 65535, [ew, index, r]() { return ew->get(index).value(r).argument; },
                                                   [update](int v) { update([v](Evolution& e) { e.argument = v; }); },
                                                   orig([](const Evolution& e) { return e.argument; }));
                    arg.info = "Zusatzwert der Methode: Item-Nr. (Item benutzen, mit Item), Attacken-Nr., Pokémon-Nr. oder Typ.";
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
                evos.canAdd = [ew, index]() { return ew->get(index).size() < ew->rawFile(index).size() / 8; };
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

                // TMs und TPs
                for (int kind = 0; kind < 2; kind++) {
                    edit::Table tms;
                    tms.title = kind == 0 ? "TMs" : "TPs";
                    tms.columns = {"Nr.", "Attacke", "Lernbar"};
                    const QList<int>* list = kind == 0 ? &kTMMoves : &kTRMoves;
                    tms.rowCount = [list]() { return static_cast<int>(list->size()); };
                    tms.row = [s, w, o, index, kind, list](int r) {
                        QList<edit::Field> fields;
                        fields << edit::text("", [kind, r]() { return QString("%1%2").arg(kind == 0 ? "TM" : "TP").arg(r, 2, 10, QChar('0')); });
                        const int move = list->value(r);
                        fields << edit::text("", [s, move]() { return s->texts.moveName(move); });
                        if (kind == 0) {
                            fields << edit::check("Lernbar", [w, index, r]() { return w->canLearnTM(index, r) ? 1 : 0; },
                                                  [w, index, r](int v) { w->setTM(index, r, v != 0); },
                                                  [o, index, r]() { return o->canLearnTM(index, r) ? 1 : 0; });
                        } else {
                            fields << edit::check("Lernbar", [w, index, r]() { return w->canLearnTR(index, r) ? 1 : 0; },
                                                  [w, index, r](int v) { w->setTR(index, r, v != 0); },
                                                  [o, index, r]() { return o->canLearnTR(index, r) ? 1 : 0; });
                        }
                        return fields;
                    };
                    c.tables << tms;
                }
                return c;
            };
            section.entries.append(entry);
        }
    }
    return section;
}

// ------------------------------------------------- Dyna-Hoehle und Kampfturm

edit::Section facilitySection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Dyna-Höhle & Kampfturm";
    section.subtitle = "Leih- und Gegner-Pokémon der Dynamax-Abenteuer und die Pokémon im Kampfturm.";

    for (int i = 0; i < s->lair.size() && i < s->lairOriginal.size(); i++) {
        const FlatRecord& o = s->lairOriginal[i];
        if (o.get(lair::Species) <= 0) continue;
        edit::Entry entry;
        entry.group = o.get(lair::IsSingleCapture) != 0 ? "Dyna-Höhle: Legendäre" : "Dyna-Höhle: Leih- und Gegner-Pokémon";
        entry.title = pokemonText(*s, static_cast<int>(o.get(lair::Species)), static_cast<int>(o.get(lair::Form))) +
                      "  ·  Lv. " + QString::number(o.get(lair::Level));
        entry.search = [s, i]() {
            return s->texts.pokemonName(static_cast<int>(s->lair[i].get(lair::Species)), static_cast<int>(s->lair[i].get(lair::Form)));
        };
        entry.changed = [s, i]() { return !sameRecord(s->lair[i], s->lairOriginal[i]); };
        entry.build = [s, i]() {
            edit::Content c;
            QList<FlatRecord>* l = &s->lair;
            const QList<FlatRecord>* o = &s->lairOriginal;
            c.fieldsPerLine = 4;
            c.hint = "Gigadynamax: 2 = Gigadynamax-Form, 1 = normal (wie im Spiel gespeichert).";
            c.warnings = [s, i]() {
                QStringList w;
                const FlatRecord& r = s->lair[i];
                const int sp = static_cast<int>(r.get(lair::Species)), fm = static_cast<int>(r.get(lair::Form));
                if (r.get(lair::GigantamaxState) == 2 && !canGigantamax(sp, fm)) {
                    w << s->texts.pokemonName(sp, fm) + " hat keine Gigadynamax-Form.";
                }
                const int index = s->work.personal.indexOf(sp, fm);
                if (index > 0 && !s->work.personal.canDynamax(index)) {
                    w << s->texts.pokemonName(sp, fm) + " kann nicht dynamaximieren.";
                }
                return w;
            };
            c.fields << recordPokemon("Pokémon", l, o, i, lair::Species, lair::Form);
            c.fields << recordNumber("Level", l, o, i, lair::Level, 1, 100);
            c.fields << recordNumber("Gigadynamax", l, o, i, lair::GigantamaxState, 0, 2);
            for (int k = 0; k < 4; k++) {
                c.fields << recordPicker(QString("Attacke %1").arg(k + 1), edit::Kind::Move, l, o, i, lair::Move0 + k);
            }
            edit::Field ability = recordNumber("Fähigkeit (Nr.)", l, o, i, lair::Ability, -1, 4);
            ability.info = "Fähigkeit als Zahl, wie im Spiel gespeichert.";
            c.fields << ability;
            const int ivs[6] = {lair::IvHp, lair::IvAtk, lair::IvDef, lair::IvSpa, lair::IvSpd, lair::IvSpe};
            c.fields << recordIvs(l, o, i, ivs);
            return c;
        };
        section.entries.append(entry);
    }

    for (int i = 0; i < s->tower.size() && i < s->towerOriginal.size(); i++) {
        const FlatRecord& o = s->towerOriginal[i];
        if (o.get(tower::Species) <= 0) continue;
        edit::Entry entry;
        entry.group = "Kampfturm";
        entry.title = QString("#%1 ").arg(i + 1) +
                      pokemonText(*s, static_cast<int>(o.get(tower::Species)), static_cast<int>(o.get(tower::Form)));
        entry.search = [s, i]() {
            return s->texts.pokemonName(static_cast<int>(s->tower[i].get(tower::Species)), static_cast<int>(s->tower[i].get(tower::Form)));
        };
        entry.changed = [s, i]() { return !sameRecord(s->tower[i], s->towerOriginal[i]); };
        entry.build = [s, i]() {
            edit::Content c;
            QList<FlatRecord>* l = &s->tower;
            const QList<FlatRecord>* o = &s->towerOriginal;
            c.fieldsPerLine = 4;
            c.hint = "Aus diesen Pokémon stellt der Kampfturm die Teams zusammen (Level 50).";
            c.fields << recordPokemon("Pokémon", l, o, i, tower::Species, tower::Form);
            c.fields << recordPicker("Item", edit::Kind::Item, l, o, i, tower::HeldItem);
            c.fields << recordChoice("Wesen", natureChoicesWithRandom(), l, o, i, tower::Nature);
            for (int k = 0; k < 4; k++) {
                c.fields << recordPicker(QString("Attacke %1").arg(k + 1), edit::Kind::Move, l, o, i, tower::Move0 + k);
            }
            const int ivs[6] = {tower::IvHp, tower::IvAtk, tower::IvDef, tower::IvSpa, tower::IvSpd, tower::IvSpe};
            c.fields << recordIvs(l, o, i, ivs);
            return c;
        };
        section.entries.append(entry);
    }
    return section;
}

const QString kShopFile = "bin/appli/shop/bin/shop_data.bin";

// ------------------------------------------------ Spoiler-Log nach Bearbeitung

// Dyna-Raids: unveraendert = Ergebnis des Randomizers, sonst alle Nester, die vom Dump abweichen
RaidResult raidLog(const EditSession& s) {
    if (!s.reopened && sameNests(s.nests, s.nestsStart)) return s.raids;
    RaidResult out;
    int perVersion[3] = {0, 0, 0};
    for (int t = 0; t < s.nests.size(); t++) {
        const NestTable& table = s.nests[t];
        RaidResult::Den den;
        den.shield = table.version == 2;
        den.index = perVersion[qBound(0, static_cast<int>(table.version), 2)]++;
        if (t < s.nestsOriginal.size() && sameNestTable(table, s.nestsOriginal[t])) continue;
        for (const NestEntry& n : table.entries) {
            if (n.species <= 0) continue;
            int star = 4;
            for (int k = 0; k < n.probabilities.size() && k < 5; k++) {
                if (n.probabilities[k] > 0) { star = k; break; }
            }
            den.entries.append({StarterChoice{static_cast<int>(n.species), static_cast<int>(n.form) + (n.gigantamax ? 1000 : 0)},
                                star + 1});
        }
        out.dens.append(den);
    }
    return out;
}

// Items: unveraendert = Ergebnis des Randomizers, sonst Zaehlung gegenueber dem Dump
ItemResult itemLog(const EditSession& s) {
    ItemResult out = s.items;
    if (s.itemInfoLoaded && (s.reopened || !sameSpots(s.spots, s.spotsStart))) {
        const bool randomized = !s.items.placedItems.isEmpty();
        out.fieldItems = out.hiddenItems = 0;
        out.placedItems.clear();
        for (int i = 0; i < s.spots.size(); i++) {
            const bool changed = i >= s.spotsOriginal.size() || s.spots[i].item != s.spotsOriginal[i].item ||
                                 s.spots[i].quantity != s.spotsOriginal[i].quantity;
            if (changed) (s.spots[i].hidden ? out.hiddenItems : out.fieldItems)++;
            if (changed || randomized) out.placedItems[s.spots[i].item] += 1;
        }
    }
    if (s.reopened || !sameShop(s.shopSlots, s.shopSlotsStart)) {
        out.shopItems = 0;
        for (int i = 0; i < s.shopSlots.size(); i++) {
            if (i >= s.shopSlotsOriginal.size() || s.shopSlots[i].item != s.shopSlotsOriginal[i].item) out.shopItems++;
        }
    }
    return out;
}

// Dyna-Hoehle: unveraendert = Ergebnis des Randomizers, sonst alle Eintraege, die vom Dump abweichen
FacilityResult facilityLog(const EditSession& s) {
    FacilityResult out = s.facilities;
    if (s.reopened || !sameRecords(s.lair, s.lairStart)) {
        out.lairPokemon.clear();
        out.lairLegends.clear();
        for (int i = 0; i < s.lair.size() && i < s.lairOriginal.size(); i++) {
            const FlatRecord& r = s.lair[i];
            if (r.get(lair::Species) <= 0 || sameRecord(r, s.lairOriginal[i])) continue;
            const StarterChoice c{static_cast<int>(r.get(lair::Species)),
                                  static_cast<int>(r.get(lair::Form)) + (r.get(lair::GigantamaxState) == 2 ? 1000 : 0)};
            (r.get(lair::IsSingleCapture) != 0 ? out.lairLegends : out.lairPokemon).append(c);
        }
    }
    if (s.reopened || !sameRecords(s.tower, s.towerStart)) {
        out.towerPokemon.clear();
        for (int i = 0; i < s.tower.size() && i < s.towerOriginal.size(); i++) {
            const FlatRecord& r = s.tower[i];
            if (r.get(tower::Species) <= 0 || sameRecord(r, s.towerOriginal[i])) continue;
            out.towerPokemon.append({static_cast<int>(r.get(tower::Species)), static_cast<int>(r.get(tower::Form))});
        }
    }
    return out;
}

// Entwicklungen: Aenderungen des Randomizers + von Hand geaenderte Eintraege
QList<EvolutionChange> evolutionLog(const EditSession& s) {
    QList<EvolutionChange> out = s.evolutionChanges;
    auto describe = [&s](const Evolution& e) {
        const QStringList methods = evolutionMethods();
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
    for (int index = 0; index < s.work.personal.count(); index++) {
        const QList<Evolution> now = s.work.evolutions.get(index);
        const QList<Evolution> start = s.evolutionsStart.get(index);
        if (!s.reopened && sameEvolutions(now, start)) continue;
        const QList<Evolution> before = s.original.evolutions.get(index);
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


bool loadWildFrom(const QByteArray& dataTable, WildArchive out[4], QString* error) {
    GfPak pack;
    if (!pack.load(dataTable)) {
        if (error) *error = "Die Datei data_table.gfpak (wilde Pokémon) konnte nicht gelesen werden.";
        return false;
    }
    for (int i = 0; i < 4; i++) {
        const int index = pack.indexOf(kWildFiles[i]);
        if (index < 0 || !out[i].load(pack.file(index))) {
            if (error) *error = QString("Die Tabelle %1 konnte nicht gelesen werden.").arg(kWildFiles[i]);
            return false;
        }
    }
    return true;
}

} // namespace

// ================================================================ Oeffentlich

bool openSession(EditSession& s, const EncounterResult* encounters, QString* error) {
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };
    s.texts.load(s.romfs);
    s.placeNames = readMessage(s.romfs, "place_name_indirect");

    // Wilde Pokemon
    const QByteArray originalTable = readFile(s.romfs + "/" + kDataTable);
    if (!loadWildFrom(originalTable, s.wildOriginal, error)) return false;
    s.dataTableChanged = !s.dataTable.isEmpty();
    if (s.dataTable.isEmpty()) s.dataTable = originalTable;
    if (!loadWildFrom(s.dataTable, s.wild, error)) return false;
    for (int i = 0; i < 4; i++) s.wildStart[i] = s.wild[i];

    // Geschenke, Begegnungen, Tausch
    bool ok = false;
    s.giftsOriginal = readFlatArchive(readFile(s.romfs + "/" + path::Gifts), gift::kSizes, &ok);
    if (!ok) return fail("Die Geschenk-Daten (add_poke.bin) konnten nicht gelesen werden.");
    s.staticsOriginal = readFlatArchive(readFile(s.romfs + "/" + path::Statics), encounter::kSizes, &ok);
    if (!ok) return fail("Die Begegnungs-Daten (event_encount_data.bin) konnten nicht gelesen werden.");
    s.tradesOriginal = readFlatArchive(readFile(s.romfs + "/" + path::Trades), trade::kSizes, &ok);
    if (!ok) return fail("Die Tausch-Daten (field_trade.bin) konnten nicht gelesen werden.");

    s.gifts = s.giftsOriginal;
    s.statics = s.staticsOriginal;
    s.trades = s.tradesOriginal;
    if (encounters) {
        if (!encounters->gifts.isEmpty()) s.gifts = readFlatArchive(encounters->gifts, gift::kSizes, &ok);
        if (!encounters->statics.isEmpty()) s.statics = readFlatArchive(encounters->statics, encounter::kSizes, &ok);
        if (!encounters->trades.isEmpty()) s.trades = readFlatArchive(encounters->trades, trade::kSizes, &ok);
        if (s.gifts.size() != s.giftsOriginal.size() || s.statics.size() != s.staticsOriginal.size() ||
            s.trades.size() != s.tradesOriginal.size()) {
            return fail("Die randomisierten Geschenke/Begegnungen passen nicht zu den Originaldaten.");
        }
    }

    // Dyna-Raids (fehlt die Tabelle, bleibt der Reiter leer)
    auto readNests = [](const QByteArray& table, QList<NestTable>& out) {
        GfPak pack;
        if (!pack.load(table)) return;
        const int index = pack.indexOf("nest_hole_encount.bin");
        if (index >= 0) readNestTables(pack.file(index), out);
    };
    readNests(originalTable, s.nestsOriginal);
    readNests(s.dataTable, s.nests);
    if (s.nests.size() != s.nestsOriginal.size()) s.nests = s.nestsOriginal;
    s.nestsStart = s.nests;

    // Items auf dem Boden / versteckt und Laeden
    s.itemInfoLoaded = s.itemInfo.load(s.romfs);
    if (s.itemInfoLoaded) {
        const QByteArray placementOriginal = readFile(s.romfs + "/" + path::Placement);
        s.spotsOriginal = readItemSpots(placementOriginal, s.itemInfo);
        s.spots = s.placementBase.isEmpty() ? s.spotsOriginal : readItemSpots(s.placementBase, s.itemInfo);
        if (s.spots.size() != s.spotsOriginal.size()) s.spots = s.spotsOriginal;
        s.spotsStart = s.spots;
    }
    s.shopSlotsOriginal = readShopSlots(readFile(s.romfs + "/" + kShopFile));
    s.shopSlots = s.items.shops.isEmpty() ? s.shopSlotsOriginal : readShopSlots(s.items.shops);
    if (s.shopSlots.size() != s.shopSlotsOriginal.size()) s.shopSlots = s.shopSlotsOriginal;
    s.shopSlotsStart = s.shopSlots;

    // Pokemon-Daten
    s.evolutionsStart = s.work.evolutions;

    // Dyna-Hoehle und Kampfturm
    s.lairOriginal = readFlatArchive(readFile(s.romfs + "/" + kLairFile), lair::kSizes, &ok);
    if (!ok) s.lairOriginal.clear();
    s.lair = s.facilities.lair.isEmpty() ? s.lairOriginal : readFlatArchive(s.facilities.lair, lair::kSizes, &ok);
    if (s.lair.size() != s.lairOriginal.size()) s.lair = s.lairOriginal;
    s.lairStart = s.lair;
    s.towerOriginal = readFlatArchive(readFile(s.romfs + "/" + kTowerFile), tower::kSizes, &ok);
    if (!ok) s.towerOriginal.clear();
    s.tower = s.facilities.tower.isEmpty() ? s.towerOriginal : readFlatArchive(s.facilities.tower, tower::kSizes, &ok);
    if (s.tower.size() != s.towerOriginal.size()) s.tower = s.towerOriginal;
    s.towerStart = s.tower;
    return true;
}

edit::Catalog buildCatalog(const EditSession& s) {
    edit::Catalog c;
    for (const StarterChoice& m : availablePokemon(s.work)) {
        c.pokemon.append({edit::pokemonValue(m.species, m.form), s.texts.pokemonName(m.species, m.form)});
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
        if (name.isEmpty() || name.startsWith('?') || name == "-") continue;
        items.append({i, name});
    }
    c.items = sorted(items, "(kein Item)");
    QList<QPair<int, QString>> abilities;
    for (int i = 1; i < s.texts.abilities.size(); i++) {
        const QString name = s.texts.abilities[i].trimmed();
        if (!name.isEmpty() && name != "-") abilities.append({i, name});
    }
    c.abilities = sorted(abilities, "(keine)");
    for (int t = 0; t < 18; t++) c.types.append({t, s.texts.typeName(t)});
    const QStringList natures = edit::natureNames();
    for (int n = 0; n < natures.size(); n++) c.natures.append({n, natures[n]});
    return c;
}

QList<edit::Section> buildSections(EditSession& s) {
    QList<edit::Section> sections = {trainerSection(s), wildSection(s), encounterSection(s)};
    if (!s.nests.isEmpty()) sections << raidSection(s);
    if (s.itemInfoLoaded) sections << itemSection(s);
    sections << dataSection(s);
    if (!s.lair.isEmpty() || !s.tower.isEmpty()) sections << facilitySection(s);
    return sections;
}

int changedTrainerCount(const EditSession& s) {
    int count = 0;
    const QList<Trainer>& list = s.work.trainers.all();
    for (const Trainer& t : list) {
        const Trainer* o = originalTrainer(s, t.index);
        if (o && !sameTrainer(t, *o)) count++;
    }
    return count;
}

int changedEncounterCount(const EditSession& s) { return static_cast<int>(encounterChanges(s).size()); }

bool sessionHasChanges(const EditSession& s) {
    if (s.dataTableChanged || changedTrainerCount(s) > 0 || changedEncounterCount(s) > 0) return true;
    for (int i = 0; i < 4; i++) {
        if (!sameArchive(s.wild[i], s.wildOriginal[i])) return true;
    }
    if (!sameNests(s.nests, s.nestsOriginal) || !sameSpots(s.spots, s.spotsOriginal) ||
        !sameShop(s.shopSlots, s.shopSlotsOriginal) || !sameRecords(s.lair, s.lairOriginal) ||
        !sameRecords(s.tower, s.towerOriginal)) {
        return true;
    }
    return !s.placementBase.isEmpty() || s.work.personal.save() != s.original.personal.save() ||
           s.work.learnsets.save() != s.original.learnsets.save() || !s.work.evolutions.changedIndexes().isEmpty();
}

bool exportSession(const EditSession& s, const QString& romfsOut, const QString& spoilerPath,
                   const TrainerSettings& trainerSettings, const QString& seedText, QString* error) {
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };
    bool written = true;

    // Trainer: alles, was vom Dump abweicht
    QList<int> changedTrainers;
    QSet<int> editedTrainers;
    const QSet<int> randomized(s.trainerResult.changedIndexes.begin(), s.trainerResult.changedIndexes.end());
    for (const Trainer& t : s.work.trainers.all()) {
        const Trainer* o = originalTrainer(s, t.index);
        if (!o || sameTrainer(t, *o)) continue;
        changedTrainers.append(t.index);
        if (!randomized.contains(t.index)) editedTrainers.insert(t.index);
    }
    written &= s.work.trainers.save(romfsOut, changedTrainers);

    // data_table.gfpak: wilde Pokemon (+ Raids aus dem Randomizer)
    bool wildEdited = false;
    for (int i = 0; i < 4; i++) wildEdited = wildEdited || !sameArchive(s.wild[i], s.wildStart[i]);
    const bool raidsEdited = !sameNests(s.nests, s.nestsStart);
    if (wildEdited || raidsEdited || s.dataTableChanged) {
        QByteArray table = s.dataTable;
        if (wildEdited || raidsEdited) {
            GfPak pack;
            if (!pack.load(table)) return fail("Die Datei data_table.gfpak konnte nicht gelesen werden.");
            for (int i = 0; i < 4; i++) {
                if (sameArchive(s.wild[i], s.wildStart[i])) continue;
                pack.setFile(pack.indexOf(kWildFiles[i]), s.wild[i].save());
            }
            if (raidsEdited) pack.setFile(pack.indexOf("nest_hole_encount.bin"), writeNestTables(s.nests));
            table = pack.save();
        }
        written &= writeFile(romfsOut + "/" + kDataTable, table);
    }

    // Geschenke, Begegnungen, Tausch
    if (!sameRecords(s.gifts, s.giftsOriginal)) written &= writeFile(romfsOut + "/" + path::Gifts, writeFlatArchive(s.gifts, gift::kSizes));
    if (!sameRecords(s.statics, s.staticsOriginal)) {
        written &= writeFile(romfsOut + "/" + path::Statics, writeFlatArchive(s.statics, encounter::kSizes));
    }
    if (!sameRecords(s.trades, s.tradesOriginal)) written &= writeFile(romfsOut + "/" + path::Trades, writeFlatArchive(s.trades, trade::kSizes));

    // placement.gfpak: Items (Randomizer und von Hand) + Starter-Modelle
    QByteArray placement = s.placementBase;
    const bool spotsEdited = s.itemInfoLoaded && !sameSpots(s.spots, s.spotsStart);
    if (spotsEdited) {
        const QByteArray base = placement.isEmpty() ? readFile(s.romfs + "/" + path::Placement) : placement;
        placement = writeItemSpots(base, s.spots, s.itemInfo);
        if (placement.isEmpty()) return fail("Die Items konnten nicht in placement.gfpak geschrieben werden.");
    }
    bool startersChanged = false;
    int starter[3] = {0, 0, 0};
    for (int k = 0; k < 3; k++) {
        const int i = kStarterGiftIndexes[k];
        if (i >= s.gifts.size() || i >= s.giftsOriginal.size()) continue;
        starter[k] = static_cast<int>(s.gifts[i].get(gift::Species));
        if (s.gifts[i].get(gift::Species) != s.giftsOriginal[i].get(gift::Species)) startersChanged = true;
    }
    if (startersChanged) {
        const QByteArray base = placement.isEmpty() ? readFile(s.romfs + "/" + path::Placement) : placement;
        const QByteArray withModels = applyStarterModels(base, starter[0], starter[1], starter[2]);
        if (!withModels.isEmpty()) placement = withModels;
    }
    if (!placement.isEmpty()) written &= writeFile(romfsOut + "/" + path::Placement, placement);

    // Pokemon-Daten
    // Entwicklungen: vom Randomizer/Bearbeiten gesetzte und alle, die vom Dump abweichen (geoeffnete Mod)
    {
        QSet<int> evolutionFiles(s.work.evolutions.changedIndexes().begin(), s.work.evolutions.changedIndexes().end());
        for (int index = 0; index < s.work.personal.count(); index++) {
            if (s.work.evolutions.rawFile(index) != s.original.evolutions.rawFile(index)) evolutionFiles.insert(index);
        }
        for (int index : evolutionFiles) {
            const QByteArray raw = s.work.evolutions.rawFile(index);
            if (!raw.isEmpty()) {
                written &= writeFile(romfsOut + "/" + path::Evolutions + "/" + EvolutionTable::fileName(index), raw);
            }
        }
    }
    const bool personalChanged = s.work.personal.save() != s.original.personal.save();
    const bool learnsetsChanged = s.work.learnsets.save() != s.original.learnsets.save();
    if (personalChanged) written &= writeFile(romfsOut + "/" + path::Personal, s.work.personal.save());
    if (learnsetsChanged) written &= writeFile(romfsOut + "/" + path::Learnsets, s.work.learnsets.save());

    // Laeden
    const bool shopsEdited = !sameShop(s.shopSlots, s.shopSlotsStart);
    if (shopsEdited) {
        const QByteArray shop = writeShopSlots(readFile(s.romfs + "/" + kShopFile), s.shopSlots);
        if (shop.isEmpty()) return fail("Die Läden (shop_data.bin) konnten nicht geschrieben werden.");
        written &= writeFile(romfsOut + "/" + kShopFile, shop);
    } else if (!s.items.shops.isEmpty()) {
        written &= writeFile(romfsOut + "/" + kShopFile, s.items.shops);
    }

    // Dyna-Hoehle und Kampfturm
    if (!s.lairOriginal.isEmpty() && !sameRecords(s.lair, s.lairOriginal)) {
        written &= writeFile(romfsOut + "/" + kLairFile, writeFlatArchive(s.lair, lair::kSizes));
    }
    if (!s.towerOriginal.isEmpty() && !sameRecords(s.tower, s.towerOriginal)) {
        written &= writeFile(romfsOut + "/" + kTowerFile, writeFlatArchive(s.tower, tower::kSizes));
    }

    // Tauschpartner nennen im Dialog ihr neues Pokemon
    const QList<EncounterChange> changes = encounterChanges(s);
    bool tradeChanged = false;
    for (const EncounterChange& c : changes) tradeChanged = tradeChanged || c.category == 4;
    if (tradeChanged) {
        const QByteArray text = buildTradeTexts(readFile(s.romfs + "/" + kTradeText), changes, s.texts.pokemon);
        if (!text.isEmpty()) written &= writeFile(romfsOut + "/" + kTradeText, text);
    }
    if (!written) return fail("Die Dateien konnten nicht geschrieben werden:\n" + romfsOut);

    if (!spoilerPath.isEmpty()) {
        EncounterResult encounterLog;
        encounterLog.changes = changes;
        QList<SpoilerSection> sections = encounterSpoiler(encounterLog, s.work, s.texts);
        sections += wildSpoiler(wildLog(s), s.work, s.texts, s.placeNames, s.version);
        PokemonDataResult dataLog = s.dataResult;
        dataLog.personalChanged = personalChanged;
        dataLog.learnsetsChanged = learnsetsChanged;
        const QList<EvolutionChange> evolutions = evolutionLog(s);
        QList<SpoilerSection> extras = extrasSpoiler(evolutions, raidLog(s), itemLog(s), s.texts, s.version);
        bool evolutionsByHand = false;
        for (const EvolutionChange& c : evolutions) evolutionsByHand = evolutionsByHand || c.after.endsWith("(von Hand)");
        for (SpoilerSection& section : extras) {
            if ((evolutionsByHand || s.reopened) && section.title == "Tausch-Entwicklungen") section.title = "Entwicklungen";
        }
        sections += extras;
        sections += dataSpoiler(dataLog, facilityLog(s), s.work, s.texts);
        writeSpoiler(spoilerPath, s.work, s.texts, trainerSettings, sections, seedText, s.version, editedTrainers,
                     s.reopened ? QString("geändert") : QString("von Hand bearbeitet"));
    }
    return true;
}


bool loadModInto(EditSession& s, const QString& modFolder, QString* error) {
    auto fail = [error](const QString& message) {
        setReadOverlay(QString(), QString());
        if (error) *error = message;
        return false;
    };
    QString modRomfs = modFolder + "/romfs";
    if (!QFileInfo::exists(modRomfs + "/bin") && QFileInfo::exists(modFolder + "/bin")) modRomfs = modFolder;
    if (!QFileInfo::exists(modRomfs + "/bin")) {
        return fail("In diesem Ordner wurde keine Mod gefunden. Wähle den Ordner, in dem „romfs“ liegt "
                    "(z. B. Randomizers-Output/Schwert-Schild/Randomizer-1).");
    }
    auto modFile = [&modRomfs](const QString& relative) {
        const QString path = modRomfs + "/" + relative;
        return QFileInfo::exists(path) ? readFile(path) : QByteArray();
    };

    // Trainer, Pokemon-Daten, Entwicklungen, Lernsets, Attacken: Mod-Dateien vor dem Dump
    setReadOverlay(s.romfs, modRomfs);
    QStringList errors;
    GameFiles mod;
    const bool loaded = mod.load(s.romfs, &errors);
    setReadOverlay(QString(), QString());
    if (!loaded) return fail("Die Mod konnte nicht gelesen werden:\n" + errors.join("\n"));
    s.work = mod;

    s.dataTable = modFile(kDataTable);
    EncounterResult encounters;
    encounters.gifts = modFile(path::Gifts);
    encounters.statics = modFile(path::Statics);
    encounters.trades = modFile(path::Trades);

    // placement.gfpak: Starter-Modelle auf das Original zuruecksetzen (werden beim Export neu gesetzt)
    const QByteArray placement = modFile(path::Placement);
    if (!placement.isEmpty()) {
        const QByteArray restored = applyStarterModels(placement, 810, 813, 816, true);
        s.placementBase = restored.isEmpty() ? placement : restored;
        if (s.placementBase == readFile(s.romfs + "/" + path::Placement)) s.placementBase.clear();
    }
    s.items.shops = modFile(kShopFile);
    s.facilities.lair = modFile(kLairFile);
    s.facilities.tower = modFile(kTowerFile);
    s.reopened = true;

    if (!openSession(s, &encounters, error)) return false;
    // Gilt als Dump-Stand fuer "geaendert seit dem Oeffnen" nur intern: Export schreibt alles, was vom Dump abweicht
    s.dataTableChanged = !s.dataTable.isEmpty() && s.dataTable != readFile(s.romfs + "/" + kDataTable);
    return true;
}

} // namespace swsh
