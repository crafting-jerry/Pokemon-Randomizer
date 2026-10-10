#include "headers/swsh/swsh_extras.h"
#include "headers/swsh/swsh_fb.h"
#include "headers/swsh/swsh_trainers.h"
#include "headers/common/trainer_smart.h"

#include <QRandomGenerator>
#include <QSet>
#include <algorithm>

namespace swsh {

// ============================================================ Pokemon-Daten

QJsonObject pokemonDataSettingsToJson(const PokemonDataSettings& s) {
    QJsonObject j;
    j["tradeEvolutions"] = s.tradeEvolutions;
    return j;
}

void pokemonDataSettingsFromJson(const QJsonObject& j, PokemonDataSettings& s) {
    s.tradeEvolutions = j["tradeEvolutions"].toBool(s.tradeEvolutions);
}

// ======================================================== Tausch-Entwicklungen

QList<EvolutionChange> removeTradeEvolutions(GameFiles& files, const GameTexts& texts, int level) {
    enum { LevelUp = 4, Trade = 5, TradeHeldItem = 6, TradeWithSpecies = 7, HeldItemDay = 19, HeldItemNight = 20 };
    QList<EvolutionChange> changes;
    for (int index = 1; index < files.personal.count(); index++) {
        const QList<Evolution> original = files.evolutions.get(index);
        bool hasTrade = false;
        for (const Evolution& e : original) {
            hasTrade |= e.method == Trade || e.method == TradeHeldItem || e.method == TradeWithSpecies;
        }
        if (!hasTrade) continue;

        const QPair<int,int> from = files.personal.speciesForm(index);
        QList<Evolution> first;  // Item-Entwicklungen zuerst (z. B. Flegmon: King-Stein vor Level 37)
        QList<Evolution> rest;
        QList<EvolutionChange> local;
        for (const Evolution& e : original) {
            EvolutionChange c{from.first, from.second, e.species, e.form, QString(), QString()};
            if (e.method == Trade) {
                Evolution n = e;
                n.method = LevelUp;
                n.argument = 0;
                n.level = level;
                rest.append(n);
                c.before = "Tausch";
                c.after = QString("ab Level %1").arg(level);
            } else if (e.method == TradeWithSpecies) {
                Evolution n = e;
                n.method = LevelUp;
                n.argument = 0;
                n.level = level;
                rest.append(n);
                c.before = "Tausch gegen " + texts.pokemonName(e.argument > 0 ? e.argument : (from.first == 588 ? 616 : 588), 0);
                c.after = QString("ab Level %1").arg(level);
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
            local.append(c);
        }
        if (files.evolutions.set(index, first + rest)) {
            changes += local;
        }
    }
    return changes;
}

// ============================================================ Auswahl-Helfer

namespace {

struct Mon {
    int species = 0;
    int form = 0;
};

class Pool {
public:
    Pool(const GameFiles& f, bool legendaries, bool dynamaxOnly) : files(f) {
        smart.build(files.toGameData());
        for (int species = 1; species <= kMaxSpecies; species++) {
            if (!legendaries && isLegendary(species)) continue;
            const int forms = qMax(1, files.personal.formCount(files.personal.indexOf(species, 0)));
            for (int form = 0; form < forms; form++) {
                const int index = files.personal.indexOf(species, form);
                if (index <= 0 || !files.personal.isPresent(index) || isExcludedForm(species, form)) continue;
                if (dynamaxOnly && !files.personal.canDynamax(index)) continue;
                all.append({species, form});
            }
        }
    }
    int bst(const Mon& m) const {
        const int index = files.personal.indexOf(m.species, m.form);
        int sum = 0;
        for (int i = 0; i < 6; i++) sum += files.personal.stat(index, i);
        return sum;
    }
    bool sharesType(const Mon& a, const Mon& b) const {
        const int ia = files.personal.indexOf(a.species, a.form);
        const int ib = files.personal.indexOf(b.species, b.form);
        const QSet<int> ta = {files.personal.type1(ia), files.personal.type2(ia)};
        return ta.contains(files.personal.type1(ib)) || ta.contains(files.personal.type2(ib));
    }

    struct Rules {
        bool strength = false, type = false, level = false, gmax = false;
    };
    // Regeln werden schrittweise gelockert: Staerke, Typ, Level, zuletzt Doppelte
    bool pick(QRandomGenerator& rng, const Mon& original, int level, Rules rules, const QSet<int>& avoid, Mon& out) const {
        const int reference = bst(original);
        const Rules stages[] = {
            rules,
            {false, rules.type, rules.level, rules.gmax},
            {false, false, rules.level, rules.gmax},
            {false, false, false, rules.gmax},
        };
        for (int unique = 1; unique >= 0; unique--) {
            for (const Rules& st : stages) {
                QHash<int, QList<int>> bySpecies;
                QList<int> order;
                for (const Mon& m : all) {
                    if (unique && avoid.contains(m.species)) continue;
                    if (st.gmax && !canGigantamax(m.species, m.form)) continue;
                    if (st.type && !sharesType(original, m)) continue;
                    if (st.level && !smart.isAllowedAtLevel(m.species, m.form, level, 0, true)) continue;
                    if (st.strength && qAbs(bst(m) - reference) * 100 > reference * 15) continue;
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
        }
        return false;
    }

private:
    const GameFiles& files;
    TrainerSmartData smart;
    QList<Mon> all;
};

int genderFor(int species, int form) {
    if (species == 678 || species == 876) return form == 1 ? 2 : 1;
    return 0;
}

} // namespace

// ================================================================ Dyna-Raids

QJsonObject raidSettingsToJson(const RaidSettings& s) {
    QJsonObject j;
    j["enabled"] = s.enabled;
    j["sameType"] = s.sameType;
    j["levelAppropriate"] = s.levelAppropriate;
    j["similarStrength"] = s.similarStrength;
    j["legendaries"] = s.legendaries;
    j["keepGigantamax"] = s.keepGigantamax;
    return j;
}

void raidSettingsFromJson(const QJsonObject& j, RaidSettings& s) {
    s.enabled = j["enabled"].toBool(s.enabled);
    s.sameType = j["sameType"].toBool(s.sameType);
    s.levelAppropriate = j["levelAppropriate"].toBool(s.levelAppropriate);
    s.similarStrength = j["similarStrength"].toBool(s.similarStrength);
    s.legendaries = j["legendaries"].toBool(s.legendaries);
    s.keepGigantamax = j["keepGigantamax"].toBool(s.keepGigantamax);
}

namespace {
struct NestEntry {
    qint64 entryIndex = 0, species = 0, form = 0, levelTable = 0, ability = 0, gigantamax = 0, dropTable = 0,
           bonusTable = 0, gender = 0, flawlessIVs = 0;
    QVector<qint64> probabilities;
};
struct NestTable {
    qint64 tableId = 0, version = 0;
    QList<NestEntry> entries;
};
} // namespace

bool randomizeRaids(QByteArray& dataTable, const GameFiles& files, const RaidSettings& s, quint64 seed,
                    RaidResult& result, QString* error) {
    result = RaidResult();
    if (!s.enabled) return true;

    GfPak pack;
    if (!pack.load(dataTable)) {
        if (error) *error = "Die Datei data_table.gfpak konnte nicht gelesen werden.";
        return false;
    }
    const int fileIndex = pack.indexOf("nest_hole_encount.bin");
    const QByteArray raw = pack.file(fileIndex);
    if (fileIndex < 0 || raw.isEmpty()) {
        if (error) *error = "Die Raid-Tabelle (nest_hole_encount.bin) wurde nicht gefunden.";
        return false;
    }

    // --- Lesen ---
    FbReader r(raw);
    QList<NestTable> tables;
    for (int t : r.tables(r.root(), 0)) {
        NestTable table;
        table.tableId = r.scalar(t, 0, 8);
        table.version = r.scalar(t, 1, 4);
        for (int e : r.tables(t, 2)) {
            NestEntry n;
            n.entryIndex = r.scalar(e, 0, 4);
            n.species = r.scalar(e, 1, 4);
            n.form = r.scalar(e, 2, 4);
            n.levelTable = r.scalar(e, 3, 8);
            n.ability = r.scalar(e, 4, 1);
            n.gigantamax = r.scalar(e, 5, 1);
            n.dropTable = r.scalar(e, 6, 8);
            n.bonusTable = r.scalar(e, 7, 8);
            const int vec = r.vectorPos(e, 8);
            for (int k = 0; k < r.vectorCount(vec); k++) n.probabilities.append(r.u32(vec + 4 + 4 * k));
            n.gender = r.scalar(e, 9, 1);
            n.flawlessIVs = r.scalar(e, 10, 1);
            table.entries.append(n);
        }
        tables.append(table);
    }
    if (tables.isEmpty()) {
        if (error) *error = "Die Raid-Tabelle konnte nicht gelesen werden.";
        return false;
    }

    // --- Randomisieren ---
    Pool pool(files, s.legendaries, true);
    QRandomGenerator rng(seed ^ 0xDA1DDA1DULL);
    static const int kStarLevels[5] = {15, 25, 35, 45, 55};
    int perVersion[3] = {0, 0, 0};
    for (NestTable& table : tables) {
        RaidResult::Den den;
        den.shield = table.version == 2;
        den.index = perVersion[qBound(0, static_cast<int>(table.version), 2)]++;
        QSet<int> used;
        for (NestEntry& n : table.entries) {
            if (n.species <= 0) continue;
            int star = 4;
            for (int k = 0; k < n.probabilities.size() && k < 5; k++) {
                if (n.probabilities[k] > 0) { star = k; break; }
            }
            const bool gmax = n.gigantamax != 0 && s.keepGigantamax;
            Pool::Rules rules{s.similarStrength, s.sameType, s.levelAppropriate, gmax};
            Mon m;
            if (!pool.pick(rng, {static_cast<int>(n.species), static_cast<int>(n.form)}, kStarLevels[star], rules, used, m)) {
                continue;
            }
            used.insert(m.species);
            n.species = m.species;
            n.form = m.form;
            n.gender = genderFor(m.species, m.form);
            n.gigantamax = (gmax && canGigantamax(m.species, m.form)) ? 1 : 0;
            den.entries.append({StarterChoice{m.species, m.form + (n.gigantamax ? 1000 : 0)}, star + 1});
        }
        result.dens.append(den);
    }

    // --- Schreiben ---
    FbWriter w;
    using F = FbWriter::Field;
    auto tableOf = [](const QVector<int>& pos) { return *std::min_element(pos.begin(), pos.end()) - 4; };
    QVector<int> rootPos = w.table({F{4, 0, true}});
    w.patch(0, tableOf(rootPos));
    const int tableSlots = w.vector(rootPos[0], tables.size());
    for (int t = 0; t < tables.size(); t++) {
        const NestTable& table = tables[t];
        QVector<int> tPos = w.table({F{8, table.tableId, false}, F{4, table.version, false}, F{4, 0, true}});
        w.patch(tableSlots + 4 * t, tableOf(tPos));
        const int entrySlots = w.vector(tPos[2], table.entries.size());
        for (int e = 0; e < table.entries.size(); e++) {
            const NestEntry& n = table.entries[e];
            QVector<int> ePos = w.table({F{4, n.entryIndex, false}, F{4, n.species, false}, F{4, n.form, false},
                                         F{8, n.levelTable, false}, F{1, n.ability, false}, F{1, n.gigantamax, false},
                                         F{8, n.dropTable, false}, F{8, n.bonusTable, false}, F{4, 0, true},
                                         F{1, n.gender, false}, F{1, n.flawlessIVs, false}});
            w.patch(entrySlots + 4 * e, tableOf(ePos));
            w.vectorScalars(ePos[8], n.probabilities, 4);
        }
    }
    while (w.buf.size() % 4) w.buf.append('\0');

    pack.setFile(fileIndex, w.buf);
    dataTable = pack.save();
    return true;
}

// ===================================================================== Items

QJsonObject itemSettingsToJson(const ItemSettings& s) {
    QJsonObject j;
    j["fieldItems"] = s.fieldItems;
    j["hiddenItems"] = s.hiddenItems;
    j["mode"] = s.mode;
    j["shops"] = s.shops;
    j["trainerItems"] = s.trainerItems;
    return j;
}

void itemSettingsFromJson(const QJsonObject& j, ItemSettings& s) {
    s.fieldItems = j["fieldItems"].toBool(s.fieldItems);
    s.hiddenItems = j["hiddenItems"].toBool(s.hiddenItems);
    s.mode = qBound(0, j["mode"].toInt(s.mode), 1);
    s.shops = j["shops"].toBool(s.shops);
    s.trainerItems = j["trainerItems"].toBool(s.trainerItems);
}

namespace {

enum Pouch { Medicine = 0, Balls, Battle, Berries, Other, TMs, Treasures, Ingredients, Key };

struct ItemInfo {
    QHash<int, int> pouch;
    QHash<int, quint32> price;
    QHash<quint64, int> idByHash;
    QHash<int, quint64> hashById;

    bool load(const QString& romfs) {
        const QByteArray data = readFile(romfs + "/bin/pml/item/item.dat");
        const QByteArray hashes = readFile(romfs + "/bin/pml/item/item_hash_to_index.dat");
        if (data.size() < 0x48 || hashes.size() < 4) return false;
        auto u16 = [&](const QByteArray& d, int p) { return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(d.constData() + p)); };
        auto u32 = [&](const QByteArray& d, int p) { return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(d.constData() + p)); };
        const int count = u16(data, 0);
        const int maxEntry = u16(data, 4);
        const int start = static_cast<qint32>(u32(data, 0x40));
        for (int i = 0; i < count && 0x44 + 2 * i + 2 <= data.size(); i++) {
            const int entry = u16(data, 0x44 + 2 * i);
            const int pos = start + entry * 0x30;
            if (entry >= maxEntry || pos + 0x30 > data.size()) continue;
            pouch[i] = static_cast<quint8>(data[pos + 0x11]) & 0xF;
            price[i] = u32(data, pos);
        }
        const int hashCount = static_cast<int>(u32(hashes, 0));
        for (int i = 0; i < hashCount && 4 + 16 * i + 12 <= hashes.size(); i++) {
            const quint64 h = qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(hashes.constData() + 4 + 16 * i));
            const int id = static_cast<int>(u32(hashes, 4 + 16 * i + 8));
            idByHash[h] = id;
            hashById[id] = h;
        }
        return !pouch.isEmpty() && !idByHash.isEmpty();
    }
    // TMs, TPs und Basis-Items werden nie veraendert
    bool isProtected(int id) const {
        const int p = pouch.value(id, Key);
        return p == TMs || p == Key;
    }
};

// Kampf-Items fuer Trainer-Pokemon (IDs gegen die Spieltexte geprueft)
const QList<int> kHeldItems = {
    234, 270, 220, 297, 287, 275, 158, 157, 268, 640, 540, 639, 538, 269, 217, 253, 266, 267, 232, 213, 221, 214,
    219, 541, 542, 1120, 1118, 1119, 1121, 1123, 155, 230, 276,
    237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 248, 249, 250, 251 // Typ-verstaerkende Items
};

// Items, die in Shops immer bleiben (Baelle, Traenke, Heiler, Schutz)
const QSet<int> kKeepInShops = {2, 3, 4, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 76, 77, 79};

} // namespace

bool randomizeItems(const QString& romfs, const QByteArray& placement, GameFiles& work, QList<int>* changedTrainers,
                    const ItemSettings& s, quint64 seed, ItemResult& result, QString* error) {
    result = ItemResult();
    if (!s.anyEnabled()) return true;

    ItemInfo info;
    if (!info.load(romfs)) {
        if (error) *error = "Die Item-Daten (bin/pml/item) konnten nicht gelesen werden.";
        return false;
    }
    // Pool fuer "komplett zufaellig": alle Items, die im Spiel wirklich vorkommen
    // (Boden, versteckt, Shops) plus Kampf-Items. So landen keine Platzhalter-Items im Spiel.
    QSet<int> obtainable(kHeldItems.begin(), kHeldItems.end());
    {
        GfPak scan;
        if (scan.load(placement)) {
            for (int f = 0; f < scan.count(); f++) {
                const QByteArray data = scan.file(f);
                if (data.isEmpty()) continue;
                FbReader r(data);
                const int root = r.root();
                if (!r.valid(root, 4) || root < 4) continue;
                for (int zone : r.tables(root, 0)) {
                    for (int holder : r.tables(zone, 6)) {
                        const int flags = r.vectorPos(r.child(holder, 0), 6);
                        if (r.vectorCount(flags) > 0) obtainable.insert(info.idByHash.value(r.u64(flags + 4), 0));
                    }
                    for (int holder : r.tables(zone, 19)) {
                        for (int chance : r.tables(r.child(holder, 0), 2)) {
                            obtainable.insert(info.idByHash.value(r.u64(r.field(chance, 0)), 0));
                        }
                    }
                }
            }
        }
        const QByteArray shop = readFile(romfs + "/bin/appli/shop/bin/shop_data.bin");
        FbReader r(shop);
        if (shop.size() > 8) {
            QList<int> inventories;
            for (int single : r.tables(r.root(), 0)) inventories.append(r.child(single, 1));
            for (int multi : r.tables(r.root(), 1)) inventories += r.tables(multi, 1);
            for (int inv : inventories) {
                const int vec = r.vectorPos(inv, 0);
                for (int k = 0; k < r.vectorCount(vec); k++) obtainable.insert(static_cast<qint32>(r.u32(vec + 4 + 4 * k)));
            }
        }
    }
    QList<int> pool;
    for (int id : obtainable) {
        if (id > 0 && !info.isProtected(id) && info.hashById.contains(id)) pool.append(id);
    }
    std::sort(pool.begin(), pool.end());
    if (pool.isEmpty()) {
        if (error) *error = "Es wurden keine Items gefunden.";
        return false;
    }
    QRandomGenerator rng(seed ^ 0x17E3517E35ULL);
    auto randomItem = [&]() { return pool[static_cast<int>(rng.bounded(static_cast<int>(pool.size())))]; };

    // ----------------------------------------- Boden und versteckte Items
    if (s.fieldItems || s.hiddenItems) {
        GfPak pack;
        if (!pack.load(placement)) {
            if (error) *error = "Die Datei placement.gfpak konnte nicht gelesen werden.";
            return false;
        }
        struct Spot { int file; int hashPos; int quantityPos; int quantitySize; bool hidden; };
        QList<Spot> spots;
        QHash<int, QByteArray> fileData;
        for (int f = 0; f < pack.count(); f++) {
            const QByteArray data = pack.file(f);
            if (data.isEmpty()) continue;
            FbReader r(data);
            const int root = r.root();
            if (!r.valid(root, 4) || root < 4) continue;
            const int vtable = root - static_cast<qint32>(r.u32(root));
            if (!r.valid(vtable, 4)) continue;
            for (int zone : r.tables(root, 0)) {
                if (s.fieldItems) {
                    for (int holder : r.tables(zone, 6)) {
                        const int item = r.child(holder, 0);
                        const int flags = r.vectorPos(item, 6);
                        const int amounts = r.vectorPos(item, 7);
                        if (r.vectorCount(flags) < 1) continue;
                        const int hashPos = flags + 4;
                        const quint64 hash = r.u64(hashPos);
                        const int id = info.idByHash.value(hash, -1);
                        if (id <= 0 || info.isProtected(id)) continue;
                        spots.append({f, hashPos, r.vectorCount(amounts) > 0 ? amounts + 4 : -1, 4, false});
                    }
                }
                if (s.hiddenItems) {
                    for (int holder : r.tables(zone, 19)) {
                        const int item = r.child(holder, 0);
                        for (int chance : r.tables(item, 2)) {
                            const int hashPos = r.field(chance, 0);
                            if (!hashPos) continue;
                            const int id = info.idByHash.value(r.u64(hashPos), -1);
                            if (id <= 0 || info.isProtected(id)) continue;
                            spots.append({f, hashPos, r.field(chance, 2), 4, true});
                        }
                    }
                }
            }
            fileData[f] = data;
        }

        // Neue Items bestimmen
        QList<int> newItems;
        if (s.mode == 0) {
            // Mischen: jede Gruppe (Boden / versteckt) behaelt ihre Items, nur die Orte wechseln
            for (int hidden = 0; hidden <= 1; hidden++) {
                QList<int> group;
                QList<int> positions;
                for (int i = 0; i < spots.size(); i++) {
                    if (spots[i].hidden != (hidden == 1)) continue;
                    const QByteArray& d = fileData[spots[i].file];
                    group.append(info.idByHash.value(qFromLittleEndian<quint64>(
                        reinterpret_cast<const uchar*>(d.constData() + spots[i].hashPos))));
                    positions.append(i);
                }
                for (int i = group.size() - 1; i > 0; i--) {
                    const int j = static_cast<int>(rng.bounded(i + 1));
                    std::swap(group[i], group[j]);
                }
                newItems.resize(spots.size());
                for (int k = 0; k < positions.size(); k++) newItems[positions[k]] = group[k];
            }
        } else {
            for (int i = 0; i < spots.size(); i++) newItems.append(randomItem());
        }

        // Schreiben (Werte sind vorhanden und werden direkt ueberschrieben)
        QSet<int> touched;
        for (int i = 0; i < spots.size(); i++) {
            const Spot& spot = spots[i];
            QByteArray& d = fileData[spot.file];
            qToLittleEndian<quint64>(info.hashById.value(newItems[i]), reinterpret_cast<uchar*>(d.data() + spot.hashPos));
            if (s.mode == 1 && spot.quantityPos > 0) {
                qToLittleEndian<quint32>(1, reinterpret_cast<uchar*>(d.data() + spot.quantityPos));
            }
            touched.insert(spot.file);
            result.placedItems[newItems[i]] += 1;
            if (spot.hidden) result.hiddenItems++; else result.fieldItems++;
        }
        for (int f : touched) pack.setFile(f, fileData[f]);
        if (!touched.isEmpty()) result.placement = pack.save();
    }

    // -------------------------------------------------------------- Shops
    if (s.shops) {
        QByteArray shop = readFile(romfs + "/bin/appli/shop/bin/shop_data.bin");
        if (shop.isEmpty()) {
            if (error) *error = "Die Shop-Daten (shop_data.bin) konnten nicht gelesen werden.";
            return false;
        }
        FbReader r(shop);
        QList<int> inventories;
        for (int single : r.tables(r.root(), 0)) {
            const int inv = r.child(single, 1);
            if (inv) inventories.append(inv);
        }
        for (int multi : r.tables(r.root(), 1)) {
            for (int inv : r.tables(multi, 1)) inventories.append(inv);
        }
        QByteArray out = shop;
        for (int inv : inventories) {
            const int vec = r.vectorPos(inv, 0);
            QSet<int> present;
            for (int k = 0; k < r.vectorCount(vec); k++) present.insert(static_cast<qint32>(r.u32(vec + 4 + 4 * k)));
            for (int k = 0; k < r.vectorCount(vec); k++) {
                const int pos = vec + 4 + 4 * k;
                const int id = static_cast<qint32>(r.u32(pos));
                if (id <= 0 || info.isProtected(id) || kKeepInShops.contains(id)) continue;
                int item = randomItem();
                for (int tries = 0; tries < 20 && present.contains(item); tries++) item = randomItem();
                present.insert(item);
                qToLittleEndian<qint32>(item, reinterpret_cast<uchar*>(out.data() + pos));
                result.shopItems++;
            }
        }
        result.shops = out;
    }

    // ------------------------------------------------------ Trainer-Items
    if (s.trainerItems) {
        for (Trainer& t : work.trainers.all()) {
            bool changed = false;
            for (TrainerPoke& p : t.team) {
                if (p.heldItem() <= 0) continue;
                p.setHeldItem(kHeldItems[static_cast<int>(rng.bounded(static_cast<int>(kHeldItems.size())))]);
                result.trainerItems++;
                changed = true;
            }
            if (changed && changedTrainers && !changedTrainers->contains(t.index)) changedTrainers->append(t.index);
        }
    }
    return true;
}

// ============================================================== Spoiler-Log

QList<SpoilerSection> extrasSpoiler(const QList<EvolutionChange>& evolutions, const RaidResult& raids,
                                    const ItemResult& items, const GameTexts& texts, Version version) {
    auto esc = [](const QString& t) { return t.toHtmlEscaped(); };
    QList<SpoilerSection> sections;

    if (!evolutions.isEmpty()) {
        SpoilerSection s;
        s.title = "Tausch-Entwicklungen";
        QString rows;
        for (const EvolutionChange& c : evolutions) {
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

    if (!raids.dens.isEmpty()) {
        SpoilerSection s;
        s.title = "Dyna-Raids";
        const bool shield = version == Version::Shield;
        QString rows;
        for (const RaidResult::Den& den : raids.dens) {
            if (den.shield != shield) continue;
            QStringList chips;
            QString search;
            for (const auto& e : den.entries) {
                const bool gmax = e.first.form >= 1000;
                const QString name = texts.pokemonName(e.first.species, e.first.form % 1000);
                chips << "<span class=\"wild\">" + QString::number(e.second) + "★ " + esc(name) +
                             (gmax ? QString(" <span class=\"gmax\">G</span>") : QString()) + "</span>";
                search += " " + name.toLower();
            }
            rows += "<tr data-search=\"" + esc(search) + "\"><td class=\"place\">Nest-Tabelle " + QString::number(den.index + 1) +
                    "</td><td>" + chips.join(" ") + "</td></tr>";
            s.count++;
        }
        s.html = "<p class=\"hint\">Die Zahl vor dem Namen ist die niedrigste Sternzahl, ab der das Pokémon im Nest "
                 "erscheint. G = Gigadynamax. Gezeigt wird " + QString(shield ? "Schild" : "Schwert") + ".</p>"
                 "<table class=\"enc\"><thead><tr><th>Nest</th><th>Pokémon</th></tr></thead><tbody>" + rows + "</tbody></table>";
        sections.append(s);
    }

    if (items.fieldItems + items.hiddenItems + items.shopItems + items.trainerItems > 0) {
        SpoilerSection s;
        s.title = "Items";
        QString html = "<p>";
        QStringList parts;
        if (items.fieldItems) parts << QString::number(items.fieldItems) + " Items auf dem Boden";
        if (items.hiddenItems) parts << QString::number(items.hiddenItems) + " versteckte Items";
        if (items.shopItems) parts << QString::number(items.shopItems) + " Shop-Angebote";
        if (items.trainerItems) parts << QString::number(items.trainerItems) + " Items von Trainer-Pokémon";
        html += esc(parts.join(" · ")) + " geändert.</p>";
        if (!items.placedItems.isEmpty()) {
            QList<QPair<int,int>> sorted;
            for (auto it = items.placedItems.constBegin(); it != items.placedItems.constEnd(); ++it) {
                sorted.append({it.value(), it.key()});
            }
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
            QString rows;
            for (const auto& entry : sorted) {
                const QString name = texts.itemName(entry.second);
                rows += "<tr data-search=\"" + esc(name.toLower()) + "\"><td>" + esc(name) + "</td><td class=\"num\">" +
                        QString::number(entry.first) + "×</td></tr>";
            }
            html += "<p class=\"hint\">So oft liegt jedes Item jetzt in der Spielwelt (Boden und versteckt):</p>"
                    "<table class=\"enc\"><thead><tr><th>Item</th><th>Anzahl</th></tr></thead><tbody>" + rows + "</tbody></table>";
        }
        s.html = html;
        s.count = items.fieldItems + items.hiddenItems + items.shopItems + items.trainerItems;
        sections.append(s);
    }
    return sections;
}

} // namespace swsh
