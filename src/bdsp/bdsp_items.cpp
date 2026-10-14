// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Items und Laeden
// ---------------------------------------------------------------------------

#include "headers/bdsp/bdsp_items.h"

#include <QMap>
#include <QRandomGenerator>
#include <QSet>
#include <algorithm>
#include <cstring>

namespace bdsp {

namespace {

enum Cmd { CmdLdval = 60, CmdFldItemEvent = 187 };
enum ArgType { ArgFloat = 1, ArgWork = 2 };
enum Pocket { PocketTMs = 5, PocketKey = 8, PocketUnused = 9 };

// Arbeitswerte der versteckten Items (ev_hide_item_N)
constexpr int kWorkHiddenItem = 234;
constexpr int kWorkHiddenCount = 235;

// Kampf-Items fuer Trainer-Pokemon (wie bei Schwert/Schild, nur Items aus der 4. Generation,
// die es in Strahlender Diamant/Leuchtende Perle gibt)
const QList<int> kHeldItems = {
    234, 270, 220, 297, 287, 275, 158, 157, 268, 269, 217, 253, 266, 267, 232, 213, 221, 214, 219, 155, 230, 276,
    265,
    237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 248, 249, 250, 251 // Typ-verstaerkende Items
};

// Items, die in Laeden immer bleiben (Baelle, Traenke, Heiler, Schutz) und Honig (fuer die Honigbaeume)
const QSet<int> kKeepInShops = {2, 3, 4, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 76, 77, 79, 94};
// Feste Laeden, die unveraendert bleiben (Getraenkeautomaten)
const QSet<int> kKeepFixedShops = {11};

float argFloat(const UValue& arg) {
    const qint32 bits = static_cast<qint32>(arg["data"].i);
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
void setArgFloat(UValue& arg, float value) {
    qint32 bits;
    std::memcpy(&bits, &value, 4);
    arg["data"].i = bits;
}
int cmdId(const UValue& cmd) {
    const std::vector<UValue>& args = cmd["Arg"].list();
    return args.empty() ? -1 : args[0]["data"].toInt();
}

struct ItemInfo {
    QHash<int, int> pocket;
    QHash<int, int> price;
    void load(const UValue& table) {
        for (const UValue& item : table["Item"].list()) {
            pocket.insert(item["no"].toInt(), item["fld_pocket"].toInt());
            price.insert(item["no"].toInt(), item["price"].toInt());
        }
    }
    bool exists(int id) const { return id > 0 && pocket.contains(id); }
    bool isProtected(int id) const {
        const int p = pocket.value(id, PocketUnused);
        return p == PocketTMs || p == PocketKey || p == PocketUnused;
    }
};

// Ein Item-Platz in einem Skript
struct Spot {
    UValue* item = nullptr;  // Argument mit der Item-Nummer (Float)
    UValue* count = nullptr; // Argument mit der Anzahl (Float), kann fehlen
    QString script;          // Skript-Objekt (fuer ScriptData::changed)
    bool hidden = false;
};

QList<Spot> findSpots(ScriptData& scripts, const ItemInfo& info, bool field, bool hidden) {
    QList<Spot> spots;
    if (field && scripts.bundle.has("fld_item")) {
        for (UValue& script : scripts.bundle.table("fld_item")["Scripts"].list()) {
            if (!script["Label"].toString().startsWith("ev_fld_item_")) continue;
            for (UValue& cmd : script["Commands"].list()) {
                if (cmdId(cmd) != CmdFldItemEvent) continue;
                std::vector<UValue>& args = cmd["Arg"].list();
                if (args.size() < 2 || args[1]["argType"].i != ArgFloat) continue;
                const int id = static_cast<int>(argFloat(args[1]));
                if (!info.exists(id) || info.isProtected(id)) continue;
                Spot spot;
                spot.item = &args[1];
                if (args.size() >= 3 && args[2]["argType"].i == ArgFloat) spot.count = &args[2];
                spot.script = "fld_item";
                spots.append(spot);
            }
        }
    }
    if (hidden && scripts.bundle.has("hide_item")) {
        for (UValue& script : scripts.bundle.table("hide_item")["Scripts"].list()) {
            if (!script["Label"].toString().startsWith("ev_hide_item_")) continue;
            Spot spot;
            spot.script = "hide_item";
            spot.hidden = true;
            for (UValue& cmd : script["Commands"].list()) {
                if (cmdId(cmd) != CmdLdval) continue;
                std::vector<UValue>& args = cmd["Arg"].list();
                if (args.size() < 3 || args[1]["argType"].i != ArgWork || args[2]["argType"].i != ArgFloat) continue;
                if (args[1]["data"].i == kWorkHiddenItem) spot.item = &args[2];
                else if (args[1]["data"].i == kWorkHiddenCount) spot.count = &args[2];
            }
            if (!spot.item) continue;
            const int id = static_cast<int>(argFloat(*spot.item));
            if (!info.exists(id) || info.isProtected(id)) continue;
            spots.append(spot);
        }
    }
    return spots;
}

QString fixedShopName(int id) {
    switch (id) {
    case 0: return "Honig-Verkäufer";
    case 1: return "Ball-Laden";
    case 2: return "Kräuterladen (Ewigenau)";
    case 3: case 4: case 5: case 6: return "Kaufhaus Schleiede";
    case 7: case 8: return "Kaufhaus Schleiede (TMs)";
    case 12: return "Café (Kuhmuh-Milch)";
    default: return QString("Sonderladen %1").arg(id);
    }
}

} // namespace

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

bool randomizeItems(const QString& romfs, GameFiles& files, ScriptData& scripts, const ItemSettings& s, quint64 seed,
                    ItemResult& result, QString* error) {
    result = ItemResult();
    if (!s.anyEnabled()) return true;

    ItemInfo info;
    info.load(files.personalBundle.table("ItemTable"));
    QList<int> heldItems;
    for (int id : kHeldItems) {
        if (info.exists(id) && !info.isProtected(id)) heldItems.append(id);
    }

    QString err;
    if ((s.fieldItems || s.hiddenItems) && !scripts.ensureLoaded(romfs, &err)) {
        if (error) *error = err;
        return false;
    }
    UValue& shopTable = files.masterBundle.table("ShopTable");

    // Pool fuer "komplett zufaellig": alle Items, die im Spiel wirklich vorkommen
    // (Boden, versteckt, Laeden) plus Kampf-Items. So landen keine Platzhalter-Items im Spiel.
    QSet<int> obtainable(heldItems.begin(), heldItems.end());
    if (scripts.loaded || scripts.ensureLoaded(romfs, &err)) {
        for (const Spot& spot : findSpots(scripts, info, true, true)) obtainable.insert(static_cast<int>(argFloat(*spot.item)));
    }
    for (const char* list : {"FS", "FixedShop"}) {
        for (const UValue& entry : shopTable[list].list()) obtainable.insert(entry["ItemNo"].toInt());
    }
    QList<int> pool, shopPool;
    for (int id : obtainable) {
        if (!info.exists(id) || info.isProtected(id)) continue;
        pool.append(id);
        if (info.price.value(id) > 0) shopPool.append(id);
    }
    std::sort(pool.begin(), pool.end());
    std::sort(shopPool.begin(), shopPool.end());
    if (pool.isEmpty() || shopPool.isEmpty()) {
        if (error) *error = "Es wurden keine Items gefunden.";
        return false;
    }
    QRandomGenerator rng(seed ^ 0x17E3B05DULL);
    auto pickFrom = [&](const QList<int>& list) { return list[static_cast<int>(rng.bounded(static_cast<int>(list.size())))]; };

    // ----------------------------------------- Boden und versteckte Items
    if (s.fieldItems || s.hiddenItems) {
        QList<Spot> spots = findSpots(scripts, info, s.fieldItems, s.hiddenItems);
        QList<int> newItems;
        newItems.resize(spots.size());
        if (s.mode == 0) {
            // Mischen: jede Gruppe (Boden / versteckt) behaelt ihre Items, nur die Orte wechseln
            for (int hidden = 0; hidden <= 1; hidden++) {
                QList<int> group, positions;
                for (int i = 0; i < spots.size(); i++) {
                    if (spots[i].hidden != (hidden == 1)) continue;
                    group.append(static_cast<int>(argFloat(*spots[i].item)));
                    positions.append(i);
                }
                for (int i = static_cast<int>(group.size()) - 1; i > 0; i--) {
                    const int j = static_cast<int>(rng.bounded(i + 1));
                    std::swap(group[i], group[j]);
                }
                for (int k = 0; k < positions.size(); k++) newItems[positions[k]] = group[k];
            }
        } else {
            for (int i = 0; i < spots.size(); i++) newItems[i] = pickFrom(pool);
        }
        for (int i = 0; i < spots.size(); i++) {
            Spot& spot = spots[i];
            setArgFloat(*spot.item, static_cast<float>(newItems[i]));
            if (s.mode == 1 && spot.count) setArgFloat(*spot.count, 1.0f);
            scripts.changed.insert(spot.script);
            result.placedItems[newItems[i]] += 1;
            if (spot.hidden) result.hiddenItems++; else result.fieldItems++;
        }
    }

    // ------------------------------------------------------------- Laeden
    if (s.shops) {
        const QHash<int, QString> zones = zoneNames(romfs);
        // Laden-Schluessel: Pokemon-Center je Zone (FS) bzw. fester Laden (FixedShop)
        QMap<QPair<int, int>, QList<UValue*>> shops;
        for (UValue& entry : shopTable["FS"].list()) shops[qMakePair(0, entry["ZoneID"].toInt())].append(&entry);
        for (UValue& entry : shopTable["FixedShop"].list()) {
            const int id = entry["ShopID"].toInt();
            if (!kKeepFixedShops.contains(id)) shops[qMakePair(1, id)].append(&entry);
        }
        for (auto it = shops.begin(); it != shops.end(); ++it) {
            QSet<int> present;
            for (const UValue* entry : it.value()) present.insert((*entry)["ItemNo"].toInt());
            ItemResult::ShopLog log;
            if (it.key().first == 0) {
                log.name = it.key().second < 0 ? QString("Pokémon-Center (Grundsortiment)")
                                               : "Pokémon-Center " + zones.value(it.key().second,
                                                                                 QString("Zone %1").arg(it.key().second));
            } else {
                log.name = fixedShopName(it.key().second);
            }
            bool changed = false;
            for (UValue* entry : it.value()) {
                UValue& no = (*entry)["ItemNo"];
                const int id = no.toInt();
                if (id > 0 && !info.isProtected(id) && !kKeepInShops.contains(id)) {
                    int item = pickFrom(shopPool);
                    for (int tries = 0; tries < 30 && present.contains(item); tries++) item = pickFrom(shopPool);
                    present.insert(item);
                    no.i = item;
                    result.shopItems++;
                    changed = true;
                }
                log.items.append(static_cast<int>(no.i));
            }
            if (changed) result.shops.append(log);
        }
        files.shopsChanged = result.shopItems > 0;
    }

    // ------------------------------------------------------ Trainer-Items
    if (s.trainerItems && !heldItems.isEmpty()) {
        for (Trainer& t : files.trainers.all()) {
            for (TrainerMon& m : t.team) {
                if (m.item <= 0) continue;
                m.item = pickFrom(heldItems);
                result.trainerItems++;
                t.changed = true;
            }
        }
    }
    return true;
}

// ============================================================== Spoiler-Log

QList<SpoilerSection> itemSpoiler(const ItemResult& items, const GameTexts& texts) {
    auto esc = [](const QString& t) { return t.toHtmlEscaped(); };
    QList<SpoilerSection> sections;
    if (items.fieldItems + items.hiddenItems + items.shopItems + items.trainerItems == 0) return sections;

    SpoilerSection s;
    s.title = "Items";
    QStringList parts;
    if (items.fieldItems) parts << QString::number(items.fieldItems) + " Items auf dem Boden";
    if (items.hiddenItems) parts << QString::number(items.hiddenItems) + " versteckte Items";
    if (items.shopItems) parts << QString::number(items.shopItems) + " Laden-Angebote";
    if (items.trainerItems) parts << QString::number(items.trainerItems) + " Items von Trainer-Pokémon";
    QString html = "<p>" + esc(parts.join(" · ")) + " geändert.</p>";

    if (!items.shops.isEmpty()) {
        QString rows;
        for (const ItemResult::ShopLog& shop : items.shops) {
            QStringList names;
            for (int id : shop.items) names << texts.itemName(id);
            rows += "<tr data-search=\"" + esc((shop.name + " " + names.join(" ")).toLower()) + "\"><td class=\"place\">" +
                    esc(shop.name) + "</td><td>" + esc(names.join(", ")) + "</td></tr>";
        }
        html += "<p class=\"hint\">Neues Sortiment der Läden (TMs und Grundartikel wie Bälle, Tränke und Schutz bleiben):</p>"
                "<table class=\"enc\"><thead><tr><th>Laden</th><th>Angebot</th></tr></thead><tbody>" + rows +
                "</tbody></table>";
    }
    if (!items.placedItems.isEmpty()) {
        QList<QPair<int, int>> sorted;
        for (auto it = items.placedItems.constBegin(); it != items.placedItems.constEnd(); ++it) {
            sorted.append({it.value(), it.key()});
        }
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            return a.first != b.first ? a.first > b.first : a.second < b.second;
        });
        QString rows;
        for (const auto& entry : sorted) {
            const QString name = texts.itemName(entry.second);
            rows += "<tr data-search=\"" + esc(name.toLower()) + "\"><td>" + esc(name) + "</td><td class=\"num\">" +
                    QString::number(entry.first) + "×</td></tr>";
        }
        html += "<p class=\"hint\">So oft liegt jedes Item jetzt in der Spielwelt (Boden und versteckt):</p>"
                "<table class=\"enc\"><thead><tr><th>Item</th><th>Anzahl</th></tr></thead><tbody>" + rows +
                "</tbody></table>";
    }
    s.html = html;
    s.count = items.fieldItems + items.hiddenItems + items.shopItems + items.trainerItems;
    sections.append(s);
    return sections;
}

} // namespace bdsp
