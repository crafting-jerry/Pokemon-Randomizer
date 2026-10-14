// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: wilde Pokemon
// ---------------------------------------------------------------------------

#include "headers/bdsp/bdsp_wild.h"
#include "headers/common/trainer_smart.h"

#include <QHash>
#include <QMap>
#include <QRandomGenerator>
#include <QSet>
#include <vector>

namespace bdsp {

namespace {

// Listen in FieldEncountTable (jeweils {maxlv, minlv, monsNo})
const char* const kSlotLists[] = {"ground_mons", "tairyo", "day", "night", "swayGrass", "gbaRuby", "gbaSapp",
                                  "gbaEme", "gbaFire", "gbaLeaf", "water_mons", "boro_mons", "ii_mons", "sugoi_mons"};

// Ungefaehre Level der Sonder-Listen (fuer "level-passende Entwicklungen")
constexpr int kHoneyLevel = 15;
constexpr int kMarshLevel = 25;
constexpr int kGardenLevel = 30;
constexpr int kUndergroundLevel = 100; // Level steigen mit den Orden: alle Stufen erlaubt

struct Slot {
    UValue* value = nullptr; // Feld mit der Art
    int level = 1;
};

struct Area {
    QString name;
    int levelMin = 100, levelMax = 0;
    std::vector<Slot> entries;
    UValue* annoon = nullptr; // Icognito-Raum (AnnoonTable)
    bool log = true;
};

class WildPicker {
public:
    WildPicker(const GameFiles& f, const WildSettings& s) : files(f), settings(s) {
        smart.build(files.toGameData());
        for (int species = 1; species <= kMaxSpecies; species++) {
            if (!s.legendaries && isLegendary(species)) continue;
            const int index = files.personal.indexOf(species, 0);
            if (index > 0 && files.personal.isPresent(index)) all.append(species);
        }
    }
    int bst(int species) const {
        const int index = files.personal.indexOf(species, 0);
        int sum = 0;
        for (int i = 0; i < 6; i++) sum += files.personal.stat(index, i);
        return sum;
    }
    bool sharesType(int a, int b) const {
        const int ia = files.personal.indexOf(a, 0), ib = files.personal.indexOf(b, 0);
        const QSet<int> ta = {files.personal.type1(ia), files.personal.type2(ia)};
        return ta.contains(files.personal.type1(ib)) || ta.contains(files.personal.type2(ib));
    }
    // Regeln werden schrittweise gelockert: erst Staerke, dann Typ, dann Level, zuletzt Doppelte
    int pick(QRandomGenerator& rng, int original, int level, const QSet<int>& avoid) const {
        const int reference = bst(original);
        struct Stage { bool strength, type, levelRule, unique; };
        const Stage stages[] = {
            {settings.similarStrength, settings.sameType, settings.levelAppropriate, true},
            {false, settings.sameType, settings.levelAppropriate, true},
            {false, false, settings.levelAppropriate, true},
            {false, false, false, true},
            {false, false, false, false},
        };
        for (const Stage& st : stages) {
            QList<int> candidates;
            for (int m : all) {
                if (st.unique && avoid.contains(m)) continue;
                if (st.type && !sharesType(original, m)) continue;
                if (st.levelRule && level < 100 && !smart.isAllowedAtLevel(m, 0, level, 0, true)) continue;
                if (st.strength && qAbs(bst(m) - reference) * 100 > reference * 15) continue;
                candidates.append(m);
            }
            if (!candidates.isEmpty()) return candidates[static_cast<int>(rng.bounded(static_cast<int>(candidates.size())))];
        }
        return original;
    }

private:
    const GameFiles& files;
    const WildSettings& settings;
    TrainerSmartData smart;
    QList<int> all;
};

void addSlot(Area& area, UValue* value, int level) {
    if (!value || !value->isValid() || value->i <= 0) return;
    area.entries.push_back({value, level});
    area.levelMin = qMin(area.levelMin, level);
    area.levelMax = qMax(area.levelMax, level);
}

// Liest alle Gebiete einer FieldEncountTable
QList<Area> fieldAreas(UValue& table, const QHash<int, QString>& zoneNames) {
    QList<Area> areas;
    for (UValue& zone : table["table"].list()) {
        const int zoneId = zone["zoneID"].toInt();
        if (zoneId < 0) continue; // ungenutzte Eintraege
        Area area;
        area.name = zoneNames.value(zoneId, QString("Gebiet %1").arg(zoneId));
        for (const char* list : kSlotLists) {
            for (UValue& slot : zone[list].list()) {
                const int maxLevel = slot["maxlv"].toInt();
                addSlot(area, &slot["monsNo"], qMax(1, maxLevel));
                if (slot["monsNo"].i > 0) area.levelMin = qMin(area.levelMin, slot["minlv"].toInt());
            }
        }
        std::vector<UValue>& annoon = zone["AnnoonTable"].list();
        if (annoon.size() >= 2 && annoon[1].i != 0) area.annoon = &annoon[1];
        if (!area.entries.empty()) areas.append(area);
    }
    Area honey;
    honey.name = "Honigbäume";
    for (UValue& row : table["mistu"].list()) {
        for (const char* field : {"Normal", "Rare", "SuperRare"}) addSlot(honey, &row[field], kHoneyLevel);
    }
    if (!honey.entries.empty()) areas.append(honey);
    Area marsh;
    marsh.name = "Großes Moor (Safari)";
    for (UValue& row : table["safari"].list()) addSlot(marsh, &row["MonsNo"], kMarshLevel);
    if (!marsh.entries.empty()) areas.append(marsh);
    Area garden;
    garden.name = "Trophäengarten";
    for (UValue& row : table["urayama"].list()) addSlot(garden, &row["monsNo"], kGardenLevel);
    if (!garden.entries.empty()) areas.append(garden);
    return areas;
}

void randomizeAreas(QList<Area>& areas, const WildPicker& picker, int mode, QRandomGenerator& rng) {
    // Global 1:1: niedrigstes Level jeder Art ueber alle Gebiete
    QHash<int, int> globalMap;
    if (mode == 1) {
        QHash<int, int> minLevel;
        QList<int> order;
        for (const Area& a : areas)
            for (const Slot& s : a.entries) {
                const int species = static_cast<int>(s.value->i);
                if (!minLevel.contains(species)) { order.append(species); minLevel[species] = s.level; }
                else minLevel[species] = qMin(minLevel[species], s.level);
            }
        QSet<int> used;
        for (int species : order) {
            const int n = picker.pick(rng, species, minLevel[species], used);
            used.insert(n);
            globalMap[species] = n;
        }
    }
    for (Area& a : areas) {
        QHash<int, int> areaMap;
        if (mode == 0) {
            QHash<int, int> minLevel;
            QList<int> order;
            for (const Slot& s : a.entries) {
                const int species = static_cast<int>(s.value->i);
                if (!minLevel.contains(species)) { order.append(species); minLevel[species] = s.level; }
                else minLevel[species] = qMin(minLevel[species], s.level);
            }
            QSet<int> used;
            for (int species : order) {
                const int n = picker.pick(rng, species, minLevel[species], used);
                used.insert(n);
                areaMap[species] = n;
            }
        }
        QSet<int> usedInArea;
        for (Slot& s : a.entries) {
            const int old = static_cast<int>(s.value->i);
            int n = old;
            if (mode == 0) n = areaMap.value(old, old);
            else if (mode == 1) n = globalMap.value(old, old);
            else n = picker.pick(rng, old, s.level, usedInArea);
            usedInArea.insert(n);
            s.value->i = n;
        }
    }
}

// Spoiler-Eintraege (alt -> neu) aus den Original- und neuen Werten
WildResult::AreaLog makeLog(const Area& area, const QList<int>& before) {
    WildResult::AreaLog log;
    log.place = area.name;
    log.levelMin = area.levelMin;
    log.levelMax = area.levelMax;
    QSet<qint64> seen;
    for (size_t k = 0; k < area.entries.size(); k++) {
        const int old = before.value(static_cast<int>(k));
        const int now = static_cast<int>(area.entries[k].value->i);
        const qint64 key = static_cast<qint64>(old) * 10000 + now;
        if (seen.contains(key)) continue;
        seen.insert(key);
        log.pairs.append(qMakePair(old, now));
    }
    return log;
}

QList<int> snapshot(const Area& area) {
    QList<int> values;
    for (const Slot& s : area.entries) values.append(static_cast<int>(s.value->i));
    return values;
}

} // namespace

QJsonObject wildSettingsToJson(const WildSettings& s) {
    QJsonObject j;
    j["enabled"] = s.enabled;
    j["mode"] = s.mode;
    j["levelAppropriate"] = s.levelAppropriate;
    j["sameType"] = s.sameType;
    j["similarStrength"] = s.similarStrength;
    j["legendaries"] = s.legendaries;
    j["underground"] = s.underground;
    return j;
}

void wildSettingsFromJson(const QJsonObject& j, WildSettings& s) {
    s.enabled = j["enabled"].toBool(s.enabled);
    s.mode = qBound(0, j["mode"].toInt(s.mode), 2);
    s.levelAppropriate = j["levelAppropriate"].toBool(s.levelAppropriate);
    s.sameType = j["sameType"].toBool(s.sameType);
    s.similarStrength = j["similarStrength"].toBool(s.similarStrength);
    s.legendaries = j["legendaries"].toBool(s.legendaries);
    s.underground = j["underground"].toBool(s.underground);
}

bool randomizeWild(const QString& romfs, const GameFiles& files, const WildSettings& s, Version version, quint64 seed,
                   WildResult& result, QString* error) {
    result = WildResult();
    if (!s.enabled) return true;
    QString err;
    if (!result.gameSettings.load(romfs, path::GameSettings, {"FieldEncountTable_d", "FieldEncountTable_p", "MapInfo"}, &err)) {
        if (error) *error = "Die wilden Pokémon (gamesettings) konnten nicht gelesen werden:\n" + err;
        return false;
    }

    // Ortsnamen: MapInfo.ZoneData[zone].PokePlaceName -> Spieltext
    const QHash<int, QString> zoneNames = bdsp::zoneNames(romfs, &result.gameSettings.table("MapInfo"));

    WildPicker picker(files, s);
    QRandomGenerator rng(seed ^ 0x77117711ULL);
    const bool pearl = version == Version::Pearl;
    for (const QString& name : {QString("FieldEncountTable_d"), QString("FieldEncountTable_p")}) {
        QList<Area> areas = fieldAreas(result.gameSettings.table(name), zoneNames);
        QList<QList<int>> before;
        for (const Area& a : areas) before.append(snapshot(a));
        randomizeAreas(areas, picker, s.mode, rng);
        const bool own = (name.endsWith("_p") == pearl);
        for (int k = 0; k < areas.size(); k++) {
            Area& a = areas[k];
            // Icognito-Raum ohne Icognito: Buchstaben-Tabelle abschalten
            if (a.annoon) {
                bool unown = false;
                for (const Slot& slot : a.entries) unown |= slot.value->i == 201;
                if (!unown) a.annoon->i = 0;
            }
            if (own) result.areas.append(makeLog(a, before[k]));
        }
    }
    result.fieldChanged = true;

    // ---------------------------------------------------------- Untergrund
    if (s.underground) {
        if (!result.underground.loadWhere(romfs, path::Assets + "UnderGround/data/ugdata", [](const UValue& v) {
                return v.has("table") || v.has("Sheet1");
            }, &err)) {
            if (error) *error = "Der Untergrund (ugdata) konnte nicht gelesen werden:\n" + err;
            return false;
        }
        QList<Area> areas;
        QStringList tables;
        for (const QString& name : result.underground.tableNames()) {
            if (!name.startsWith("UgEncount_")) continue;
            if (name == "UgEncountLevel") continue;
            Area a;
            a.name = "Untergrund · Höhlen-Tabelle " + name.mid(10);
            for (UValue& row : result.underground.table(name)["table"].list()) addSlot(a, &row["monsno"], kUndergroundLevel);
            if (!a.entries.empty()) {
                areas.append(a);
                tables << name;
            }
        }
        {
            // Seltene Pokemon je Hoehlen-Art (Gruppe = id)
            QMap<int, Area> special;
            for (UValue& row : result.underground.table("UgSpecialPokemon")["Sheet1"].list()) {
                Area& a = special[row["id"].toInt()];
                a.name = QString("Untergrund · seltene Pokémon %1").arg(row["id"].toInt());
                addSlot(a, &row["monsno"], kUndergroundLevel);
            }
            for (const Area& a : special) {
                if (!a.entries.empty()) areas.append(a);
            }
            tables << "UgSpecialPokemon";
        }
        QList<QList<int>> before;
        for (const Area& a : areas) before.append(snapshot(a));
        randomizeAreas(areas, picker, s.mode, rng);
        for (int k = 0; k < areas.size(); k++) {
            WildResult::AreaLog log = makeLog(areas[k], before[k]);
            log.levelMin = 0;
            log.levelMax = 0;
            result.undergroundLog.append(log);
        }
        result.undergroundTables = tables;
    }
    return true;
}

bool writeWild(const QString& outRomfs, const WildResult& result, QString* error) {
    if (result.fieldChanged &&
        !result.gameSettings.save(outRomfs, {"FieldEncountTable_d", "FieldEncountTable_p"}, error)) return false;
    if (!result.undergroundTables.isEmpty() && !result.underground.save(outRomfs, result.undergroundTables, error)) return false;
    return true;
}

// ------------------------------------------------------------ Spoiler-Log

QList<SpoilerSection> wildSpoiler(const WildResult& result, const GameFiles& files, const GameTexts& texts) {
    QList<SpoilerSection> sections;
    auto esc = [](const QString& t) { return t.toHtmlEscaped(); };
    auto rowsFor = [&](const QList<WildResult::AreaLog>& logs, int& count) {
        QString rows;
        for (const WildResult::AreaLog& a : logs) {
            count++;
            QString search = a.place.toLower();
            QStringList items;
            for (const auto& pair : a.pairs) {
                const QString newName = texts.pokemonName(pair.second, 0);
                const QString oldName = texts.pokemonName(pair.first, 0);
                const int t1 = files.personal.type1(files.personal.indexOf(pair.second, 0));
                items << "<span class=\"wild\" title=\"statt " + esc(oldName) + "\"><span class=\"dot t" + QString::number(t1) +
                             "\"></span>" + esc(newName) + "</span>";
                search += " " + newName.toLower() + " " + oldName.toLower();
            }
            const QString level = a.levelMax > 0 ? "Lv. " + QString::number(a.levelMin) + "–" + QString::number(a.levelMax)
                                                 : QString("nach Orden");
            rows += "<tr data-search=\"" + esc(search) + "\"><td class=\"place\">" + esc(a.place) + "</td><td class=\"num\">" +
                    level + "</td><td>" + items.join(" ") + "</td></tr>";
        }
        return rows;
    };
    const QString head = "<p class=\"hint\">Fahre mit der Maus über ein Pokémon, um zu sehen, welches es ersetzt. Gezeigt "
                         "wird deine Edition; der Mod enthält beide.</p><table class=\"enc\"><thead><tr><th>Ort</th>"
                         "<th>Level</th><th>Pokémon</th></tr></thead><tbody>";
    if (!result.areas.isEmpty()) {
        SpoilerSection section;
        section.title = "Wilde Pokémon";
        section.html = head + rowsFor(result.areas, section.count) + "</tbody></table>";
        sections.append(section);
    }
    if (!result.undergroundLog.isEmpty()) {
        SpoilerSection section;
        section.title = "Untergrund";
        section.html = head + rowsFor(result.undergroundLog, section.count) + "</tbody></table>";
        sections.append(section);
    }
    return sections;
}

} // namespace bdsp
