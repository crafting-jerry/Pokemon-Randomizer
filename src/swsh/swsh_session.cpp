#include "headers/swsh/swsh_session.h"

#include <QFile>
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
            const int sp = static_cast<int>((*o)[i].get(encounter::Species));
            if (sp >= 888 && sp <= 892) hints << "Achtung: story-kritisch, ein Wechsel kann Zwischensequenzen stören";
            c.hint = hints.join(" · ");
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
    return {trainerSection(s), wildSection(s), encounterSection(s)};
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
    return !s.placementBase.isEmpty() || !s.items.shops.isEmpty() || s.dataResult.personalChanged ||
           s.dataResult.learnsetsChanged || !s.work.evolutions.changedIndexes().isEmpty() || !s.evolutionChanges.isEmpty();
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
    if (wildEdited || s.dataTableChanged) {
        QByteArray table = s.dataTable;
        if (wildEdited) {
            GfPak pack;
            if (!pack.load(table)) return fail("Die Datei data_table.gfpak konnte nicht gelesen werden.");
            for (int i = 0; i < 4; i++) {
                if (sameArchive(s.wild[i], s.wildStart[i])) continue;
                pack.setFile(pack.indexOf(kWildFiles[i]), s.wild[i].save());
            }
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

    // placement.gfpak: Items (Randomizer) + Starter-Modelle
    QByteArray placement = s.placementBase;
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

    // Uebrige Bereiche aus dem Randomizer
    written &= s.work.evolutions.save(romfsOut);
    written &= writeFacilities(romfsOut, s.facilities);
    if (s.dataResult.personalChanged) written &= writeFile(romfsOut + "/" + path::Personal, s.work.personal.save());
    if (s.dataResult.learnsetsChanged) written &= writeFile(romfsOut + "/" + path::Learnsets, s.work.learnsets.save());
    if (!s.items.shops.isEmpty()) written &= writeFile(romfsOut + "/bin/appli/shop/bin/shop_data.bin", s.items.shops);

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
        sections += extrasSpoiler(s.evolutionChanges, s.raids, s.items, s.texts, s.version);
        sections += dataSpoiler(s.dataResult, s.facilities, s.work, s.texts);
        writeSpoiler(spoilerPath, s.work, s.texts, trainerSettings, sections, seedText, s.version, editedTrainers);
    }
    return true;
}

} // namespace swsh
