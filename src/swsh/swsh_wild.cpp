#include "headers/swsh/swsh_wild.h"
#include "headers/swsh/swsh_trainers.h"
#include "headers/common/trainer_smart.h"
#include "headers/swsh/swsh_fb.h"

#include <QHash>
#include <QRandomGenerator>
#include <QSet>
#include <QtEndian>
#include <algorithm>
#include <functional>

namespace swsh {

namespace {

const QString kDataTable = "bin/archive/field/resident/data_table.gfpak";
const char* kWildFiles[4] = {"encount_k.bin", "encount_symbol_k.bin", "encount_t.bin", "encount_symbol_t.bin"};

// Zone -> Ortsnummer (Zeile in place_name_indirect)
const QHash<quint64, int>& zoneLocations() {
    static const QHash<quint64, int> map = {
    {0x078BC1FF1A657844ULL, 12}, {0x10355EFF1F4DB0B5ULL, 18}, {0x776776717EA4483EULL, 122}, {0x776777717EA449F1ULL, 124},
    {0x776778717EA44BA4ULL, 126}, {0x776779717EA44D57ULL, 128}, {0x77677A717EA44F0AULL, 130}, {0x77677B717EA450BDULL, 132},
    {0x77676C717EA43740ULL, 134}, {0x77676D717EA438F3ULL, 136}, {0x776AFA717EA75E61ULL, 138}, {0x194B97FF2492111AULL, 28},
    {0x776E81717EAA799DULL, 140}, {0x776E7E717EAA7484ULL, 142}, {0xDBCF5CFF0180B073ULL, 32}, {0x8F67CD45F405D66EULL, 8},
    {0xE0D6E5E78C91F4A7ULL, 20}, {0xE4E595FF06C510D8ULL, 40}, {0x1C7150C0594994E5ULL, 44}, {0x7D3B7A45E97D4A51ULL, 54},
    {0x75D83E45E5AA7953ULL, 30}, {0x7D3B7745E97D4538ULL, 52}, {0xA88AC04602050B95ULL, 76}, {0xEDFC32FF0C0A1B29ULL, 68},
    {0xF55F6BFF0FDCE70EULL, 84}, {0x449AE0FF3D19D777ULL, 86}, {0x4BFDF9FF40EC6CFCULL, 88}, {0x4BFDFCFF40EC7215ULL, 90},
    {0x4BFDF6FF40EC67E3ULL, 92}, {0x4BFDFBFF40EC7062ULL, 94}, {0xB332930807F9D48AULL, 106}, {0x7771E5717EAD5960ULL, 144},
    {0x7771E8717EAD5E79ULL, 146}, {0x7771E7717EAD5CC6ULL, 148}, {0x7771EA717EAD61DFULL, 150}, {0x7771E9717EAD602CULL, 152},
    {0x7771EC717EAD6545ULL, 154}, {0x10355BFF1F4DAB9CULL, 18}, {0xB332920807F9D2D7ULL, 106}, {0x8F67CB45F405D308ULL, 8},
    {0xCD6E4FBCE1466F32ULL, 12}, {0xDF686EC613544BD1ULL, 18}, {0xD602B2A66C268F7CULL, 122}, {0x458C9CA2C0087385ULL, 124},
    {0xE20E6AE30AAA57D2ULL, 126}, {0xEEEEAC06BAC8D0B3ULL, 128}, {0xF8D1E527F7B21FA0ULL, 130}, {0xB6CFE90E0378FD79ULL, 132},
    {0x520D8DD522E9A4C6ULL, 134}, {0xBC7237A0392D8837ULL, 136}, {0xB67C706F5BAE9E35ULL, 138}, {0xDA910F69A1B92FEDULL, 130},
    {0x7C17DB1B430F9543ULL, 134}, {0xCC0F8A437312B8ACULL, 128}, {0x8BE2F6160986FB8EULL, 138}, {0x0E8392C0A57D5830ULL, 28},
    {0x82A7A328A26B9057ULL, 30}, {0x5B2BC38E044EC2B7ULL, 32}, {0x8D68276C03A332BEULL, 40}, {0x16D2FC4840A658A5ULL, 54},
    {0x3D6D58A96894575EULL, 52}, {0x6AA652641154B119ULL, 140}, {0x36A5DC94335E1E72ULL, 142}, {0xE503416A1C05765DULL, 68},
    {0x201EF8E9D2A32D71ULL, 76}, {0x42312695C904658CULL, 84}, {0x1B95A78295F6F213ULL, 86}, {0xAADAC3CB6A1DFE8AULL, 88},
    {0x9116B224702CDCF1ULL, 90}, {0xCDD3B5660D2E5E67ULL, 92}, {0x5A3B8F8147272058ULL, 94}, {0xA93101EA38598995ULL, 90},
    {0x0181225223DE5420ULL, 106}, {0x1F0F1AE1818C4326ULL, 144}, {0xAD11B3F3B2AC662DULL, 146}, {0xCD9719B2E64F2AA4ULL, 148},
    {0xCD48625EDC10CBFBULL, 150}, {0x712F3056573E23FAULL, 152}, {0x593196758BA16B61ULL, 154}, {0xF79DE930E6F50533ULL, 106},
    {0xA26A4595F72EDAEAULL, 18}, {0x56580C94EDFCE664ULL, 28}, {0xCB38FEA3F71C3958ULL, 122}, {0x1F174D36062B8C38ULL, 122},
    {0x23017513039A78E7ULL, 122}, {0xF1BA4AAD9AAB2C1AULL, 126}, {0x3D2E746F9D3F5CB5ULL, 128}, {0x6E121A9CE4F58F1EULL, 128},
    {0x3171A0C61793816EULL, 134}, {0x198E4023A1B2DDEFULL, 134}, {0xFAB1C08E70C0F1CAULL, 140}, {0xB9F76CEE459CEC07ULL, 142},
    {0x5F4E0AB29FD3F13AULL, 142}, {0xF603DEA4177200EAULL, 144}, {0x76EE4E28DD28374EULL, 144}, {0x3F264B6FCB5647B4ULL, 148},
    {0x2D887A1CA9B1B99AULL, 146}, {0x2BE7E6A8901ECC20ULL, 148}, {0x39F0170769BF4524ULL, 146}, {0xB2067FBCF8D5C7BAULL, 152},
    {0x48B9525945EE48B5ULL, 144}, {0xB5756B87989661E1ULL, 152}, {0x7AB83D18C831DDEBULL, 152}, {0xDBEF8A8593377AAAULL, 152},
    {0x066F97F8765BC22DULL, 150}, {0x87A97AFF94BC6CF2ULL, 154}, {0x94289204B628522CULL, 8}, {0x5D02F15C043B872EULL, 8},
    {0xA4945486A2B97DFFULL, 18}, {0xAC1187E9EC166853ULL, 92}, {0x908A64718CA374E6ULL, 164}, {0x908A63718CA37333ULL, 166},
    {0x908A62718CA37180ULL, 168}, {0x908A69718CA37D65ULL, 170}, {0x908A68718CA37BB2ULL, 172}, {0x908A67718CA379FFULL, 174},
    {0x908A66718CA3784CULL, 176}, {0x908A6D718CA38431ULL, 178}, {0x908A6C718CA3827EULL, 180}, {0x90875F718CA13690ULL, 182},
    {0x908760718CA13843ULL, 184}, {0x909170718CA9A7F8ULL, 186}, {0x909173718CA9AD11ULL, 188}, {0x909172718CA9AB5EULL, 190},
    {0x909175718CA9B077ULL, 192}, {0x908DEC718CA691D5ULL, 194}, {0x525D03DF0309D804ULL, 164}, {0xB0621052994A5089ULL, 164},
    {0x91B1D1436BAF5871ULL, 164}, {0xC449DFAB894F632CULL, 178}, {0x273693DD91D7BD10ULL, 170}, {0xD61582D408C39E60ULL, 170},
    {0xBECC9623CD3E8C77ULL, 166}, {0x1C051CB6F97C2068ULL, 166}, {0xBC028EF260AD9406ULL, 168}, {0x32AB88FC9797DC83ULL, 168},
    {0x39D078468AA0DCC1ULL, 170}, {0x3BFB22D0FB5B42D2ULL, 170}, {0x2B1DF6E85F9BAE28ULL, 172}, {0x36FE81B956D0DCB5ULL, 172},
    {0xBBAA199D0705405BULL, 174}, {0xFB9A7FD6D979C6DAULL, 176}, {0xBC0E1701C0276FCFULL, 176}, {0xAC2ED08E980FCFC5ULL, 178},
    {0x7D2E205E8E300EE1ULL, 178}, {0x67E3FF10EB64FB79ULL, 180}, {0x85E286D82C666BBCULL, 180}, {0x95E125D2EE3ED656ULL, 182},
    {0xA7F495799F209587ULL, 184}, {0x30AAD92559FCE81EULL, 186}, {0x6F748A46C8E3802CULL, 186}, {0x97A3E0687E3C5B01ULL, 188},
    {0xDDDFF88957FD5B5CULL, 190}, {0xF3036CD294CE9365ULL, 188}, {0xFB9BB438425D58DAULL, 190}, {0xC16C1E2A1B5FFE87ULL, 192},
    {0x081D7EF6A1C192B1ULL, 194}, {0x86EFBF49516B5555ULL, 194}, {0x39AB700A9F1AB71FULL, 180}, {0x96C6A2A36131F383ULL, 188},
    {0xC92D06352150C78AULL, 190}, {0xED1F9772AA35C3CDULL, 186}, {0x9C0049D3E6129924ULL, 192}, {0x87E14B7187BC1CC1ULL, 204},
    {0x87E1487187BC17A8ULL, 206}, {0x87E1497187BC195BULL, 208}, {0x87E14E7187BC21DAULL, 210}, {0x87E14F7187BC238DULL, 212},
    {0x87E14C7187BC1E74ULL, 214}, {0x87E14D7187BC2027ULL, 216}, {0x87E1427187BC0D76ULL, 218}, {0x87E1437187BC0F29ULL, 220},
    {0x87E4507187BE5B17ULL, 222}, {0x87E44F7187BE5964ULL, 224}, {0x87E4527187BE5E7DULL, 226}, {0x87E4517187BE5CCAULL, 228},
    {0x87DA3F7187B5E9AFULL, 230}, {0x87DA407187B5EB62ULL, 232}, {0x87DA417187B5ED15ULL, 234}, {0xD6EA3DE40B009E55ULL, 204},
    {0xADF616908BD308DFULL, 208}, {0x308C5EB6A846D1F0ULL, 210}, {0x50E781F91B97C049ULL, 212}, {0xC303110BF1EC3322ULL, 214},
    {0xB768660B0BF4C0C3ULL, 216}, {0xFCB78AFCCECAF094ULL, 218}, {0xA345459C03EA6673ULL, 222}, {0xE4A982819ACF7292ULL, 224},
    {0x18AAF85178C7B839ULL, 226}, {0x3EC6FCDC0C77D460ULL, 228}, {0xE5225F9325CCA74BULL, 230}, {0x2F1B41507D695958ULL, 232},
    {0xF8A59FCA719D1EAEULL, 210}, {0x55D8F226A42368B7ULL, 224}, {0x78536116469DC44DULL, 226}, {0x9BDD6D11FFBEDA3FULL, 230},
    };
    return map;
}

} // namespace

bool WildArchive::load(const QByteArray& data) {
    zones.clear();
    if (data.size() < 8) return false;
    FbReader r(data);
    const int root = r.root();
    field0 = static_cast<quint32>(r.scalar(root, 0, 4));
    for (int t : r.tables(root, 1)) {
        WildZone zone;
        zone.zoneId = static_cast<quint64>(r.scalar(t, 0, 8));
        for (int s : r.tables(t, 1)) {
            WildSubTable sub;
            sub.levelMin = static_cast<int>(r.scalar(s, 0, 1));
            sub.levelMax = static_cast<int>(r.scalar(s, 1, 1));
            for (int slot : r.tables(s, 2)) {
                WildSlot w;
                w.probability = static_cast<int>(r.scalar(slot, 0, 1));
                w.species = static_cast<int>(r.scalar(slot, 1, 4));
                w.form = static_cast<int>(r.scalar(slot, 2, 1));
                sub.entries.append(w);
            }
            zone.subTables.append(sub);
        }
        zones.append(zone);
    }
    return !zones.isEmpty();
}

QByteArray WildArchive::save() const {
    FbWriter w;
    using F = FbWriter::Field;
    // Die Tabellenanfaenge ergeben sich aus der Position des soffset (4 Byte vor dem ersten Feld)
    auto tableOf = [](const QVector<int>& pos) { return *std::min_element(pos.begin(), pos.end()) - 4; };

    QVector<int> rootPos = w.table({F{4, field0, false}, F{4, 0, true}});
    w.patch(0, tableOf(rootPos));
    const int zoneSlots = w.vector(rootPos[1], zones.size());
    for (int z = 0; z < zones.size(); z++) {
        const WildZone& zone = zones[z];
        QVector<int> zonePos = w.table({F{8, static_cast<qint64>(zone.zoneId), false}, F{4, 0, true}});
        w.patch(zoneSlots + 4 * z, tableOf(zonePos));
        const int subSlots = w.vector(zonePos[1], zone.subTables.size());
        for (int s = 0; s < zone.subTables.size(); s++) {
            const WildSubTable& sub = zone.subTables[s];
            QVector<int> subPos = w.table({F{1, sub.levelMin, false}, F{1, sub.levelMax, false}, F{4, 0, true}});
            w.patch(subSlots + 4 * s, tableOf(subPos));
            const int slotSlots = w.vector(subPos[2], sub.entries.size());
            for (int k = 0; k < sub.entries.size(); k++) {
                const WildSlot& slot = sub.entries[k];
                QVector<int> slotPos = w.table({F{1, slot.probability, false}, F{4, slot.species, false},
                                                F{1, slot.form, false}});
                w.patch(slotSlots + 4 * k, tableOf(slotPos));
            }
        }
    }
    while (w.buf.size() % 4) w.buf.append('\0');
    return w.buf;
}

QString zoneName(quint64 zoneId, const QStringList& placeNames) {
    const int location = zoneLocations().value(zoneId, -1);
    if (location < 0 || location >= placeNames.size() || placeNames[location].isEmpty()) {
        return QString("Gebiet %1").arg(zoneId, 16, 16, QChar('0')).toUpper();
    }
    QString name = placeNames[location];
    static const QStringList prefixes = {"auf dem ", "auf der ", "auf den ", "auf ", "in der ", "in den ", "in dem ",
                                         "im ", "in ", "am ", "an der ", "an den ", "beim ", "bei der ", "bei ", "zum ", "zur "};
    for (const QString& p : prefixes) {
        if (name.startsWith(p)) {
            name = name.mid(p.size());
            break;
        }
    }
    return name;
}

// ------------------------------------------------------------- Einstellungen

QJsonObject wildSettingsToJson(const WildSettings& s) {
    QJsonObject j;
    j["enabled"] = s.enabled;
    j["mode"] = s.mode;
    j["levelAppropriate"] = s.levelAppropriate;
    j["sameType"] = s.sameType;
    j["similarStrength"] = s.similarStrength;
    j["legendaries"] = s.legendaries;
    return j;
}

void wildSettingsFromJson(const QJsonObject& j, WildSettings& s) {
    s.enabled = j["enabled"].toBool(s.enabled);
    s.mode = qBound(0, j["mode"].toInt(s.mode), 2);
    s.levelAppropriate = j["levelAppropriate"].toBool(s.levelAppropriate);
    s.sameType = j["sameType"].toBool(s.sameType);
    s.similarStrength = j["similarStrength"].toBool(s.similarStrength);
    s.legendaries = j["legendaries"].toBool(s.legendaries);
}

// ------------------------------------------------------------- Randomizer

namespace {

struct Mon {
    int species = 0;
    int form = 0;
    bool operator==(const Mon& o) const { return species == o.species && form == o.form; }
};
inline size_t qHash(const Mon& m, size_t seed = 0) { return ::qHash(m.species * 100 + m.form, seed); }

class WildPicker {
public:
    WildPicker(const GameFiles& f, const WildSettings& s) : files(f), settings(s) {
        smart.build(files.toGameData());
        for (int species = 1; species <= kMaxSpecies; species++) {
            if (!s.legendaries && isLegendary(species)) continue;
            const int forms = qMax(1, files.personal.formCount(files.personal.indexOf(species, 0)));
            for (int form = 0; form < forms; form++) {
                const int index = files.personal.indexOf(species, form);
                if (index > 0 && files.personal.isPresent(index) && !isExcludedForm(species, form)) {
                    all.append({species, form});
                }
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

    // Waehlt einen Ersatz. Regeln werden schrittweise gelockert, falls nichts passt:
    // erst Staerke, dann Typ, dann Level, zuletzt Doppelte.
    Mon pick(QRandomGenerator& rng, const Mon& original, int level, const QSet<int>& avoid) const {
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
            QHash<int, QList<int>> bySpecies;
            QList<int> order;
            for (const Mon& m : all) {
                if (st.unique && avoid.contains(m.species)) continue;
                if (st.type && !sharesType(original, m)) continue;
                if (st.levelRule && !smart.isAllowedAtLevel(m.species, m.form, level, 0, true)) continue;
                if (st.strength && qAbs(bst(m) - reference) * 100 > reference * 15) continue;
                if (!bySpecies.contains(m.species)) order.append(m.species);
                bySpecies[m.species].append(m.form);
            }
            if (!order.isEmpty()) {
                const int species = order[static_cast<int>(rng.bounded(static_cast<int>(order.size())))];
                const QList<int>& forms = bySpecies[species];
                return {species, forms[static_cast<int>(rng.bounded(static_cast<int>(forms.size())))]};
            }
        }
        return original;
    }

private:
    const GameFiles& files;
    const WildSettings& settings;
    TrainerSmartData smart;
    QList<Mon> all;
};

} // namespace

bool randomizeWild(const QString& romfs, const GameFiles& files, const WildSettings& s, quint64 seed,
                   WildResult& result, QString* error) {
    result = WildResult();
    if (!s.enabled) return true;

    GfPak pack;
    if (!pack.load(readFile(romfs + "/" + kDataTable))) {
        if (error) *error = "Die Datei data_table.gfpak (wilde Pokémon) konnte nicht gelesen werden.";
        return false;
    }
    WildArchive archives[4];
    int indexes[4];
    for (int i = 0; i < 4; i++) {
        indexes[i] = pack.indexOf(kWildFiles[i]);
        if (indexes[i] < 0 || !archives[i].load(pack.file(indexes[i]))) {
            if (error) *error = QString("Die Tabelle %1 konnte nicht gelesen werden.").arg(kWildFiles[i]);
            return false;
        }
    }

    WildPicker picker(files, s);
    QRandomGenerator rng(seed ^ 0x77117711ULL);

    // Global 1:1: niedrigstes Level jeder Art ueber alle Tabellen
    QHash<Mon, Mon> globalMap;
    if (s.mode == 1) {
        QHash<Mon, int> minLevel;
        QList<Mon> order;
        for (const WildArchive& a : archives)
            for (const WildZone& z : a.zones)
                for (const WildSubTable& t : z.subTables)
                    for (const WildSlot& slot : t.entries) {
                        if (slot.species <= 0) continue;
                        const Mon m{slot.species, slot.form};
                        if (!minLevel.contains(m)) { order.append(m); minLevel[m] = t.levelMax; }
                        else minLevel[m] = qMin(minLevel[m], t.levelMax);
                    }
        QSet<int> used;
        for (const Mon& m : order) {
            const Mon n = picker.pick(rng, m, minLevel[m], used);
            used.insert(n.species);
            globalMap[m] = n;
        }
    }

    for (int f = 0; f < 4; f++) {
        for (WildZone& zone : archives[f].zones) {
            QHash<Mon, Mon> zoneMap;
            if (s.mode == 0) {
                QHash<Mon, int> minLevel;
                QList<Mon> order;
                for (const WildSubTable& t : zone.subTables)
                    for (const WildSlot& slot : t.entries) {
                        if (slot.species <= 0) continue;
                        const Mon m{slot.species, slot.form};
                        if (!minLevel.contains(m)) { order.append(m); minLevel[m] = t.levelMax; }
                        else minLevel[m] = qMin(minLevel[m], t.levelMax);
                    }
                QSet<int> used;
                for (const Mon& m : order) {
                    const Mon n = picker.pick(rng, m, minLevel[m], used);
                    used.insert(n.species);
                    zoneMap[m] = n;
                }
            }

            WildResult::ZoneLog log;
            log.symbol = (f % 2) == 1;
            log.shield = f >= 2;
            log.zoneId = zone.zoneId;
            log.levelMin = 100;
            QSet<qint64> logged;
            for (WildSubTable& t : zone.subTables) {
                QSet<int> usedInTable;
                for (WildSlot& slot : t.entries) {
                    if (slot.species <= 0) continue;
                    const Mon old{slot.species, slot.form};
                    Mon n;
                    if (s.mode == 0) n = zoneMap.value(old, old);
                    else if (s.mode == 1) n = globalMap.value(old, old);
                    else n = picker.pick(rng, old, t.levelMax, usedInTable);
                    usedInTable.insert(n.species);
                    slot.species = n.species;
                    slot.form = n.form;
                    log.levelMin = qMin(log.levelMin, t.levelMin);
                    log.levelMax = qMax(log.levelMax, t.levelMax);
                    const qint64 key = (static_cast<qint64>(old.species) * 100 + old.form) * 100000 + n.species * 100 + n.form;
                    if (!logged.contains(key)) {
                        logged.insert(key);
                        log.pairs.append({StarterChoice{old.species, old.form}, StarterChoice{n.species, n.form}});
                    }
                }
            }
            if (!log.pairs.isEmpty()) result.zones.append(log);
        }
        pack.setFile(indexes[f], archives[f].save());
    }
    result.dataTable = pack.save();
    return true;
}

bool writeWild(const QString& outRomfs, const WildResult& result) {
    if (result.dataTable.isEmpty()) return true;
    return writeFile(outRomfs + "/" + kDataTable, result.dataTable);
}

// ------------------------------------------------------------ Spoiler-Log

QList<SpoilerSection> wildSpoiler(const WildResult& result, const GameFiles& files, const GameTexts& texts,
                                  const QStringList& placeNames, Version version) {
    QList<SpoilerSection> sections;
    if (result.zones.isEmpty()) return sections;
    const bool shield = version == Version::Shield;
    auto esc = [](const QString& t) { return t.toHtmlEscaped(); };

    for (int symbol = 1; symbol >= 0; symbol--) {
        SpoilerSection section;
        section.title = symbol ? "Wilde Pokémon (sichtbar in der Spielwelt)" : "Wilde Pokémon (im hohen Gras, beim Angeln …)";
        QString rows;
        for (const WildResult::ZoneLog& z : result.zones) {
            if (z.symbol != (symbol == 1) || z.shield != shield) continue;
            section.count++;
            const QString place = zoneName(z.zoneId, placeNames);
            QString search = place.toLower();
            QStringList items;
            for (const auto& pair : z.pairs) {
                const QString newName = texts.pokemonName(pair.second.species, pair.second.form);
                const QString oldName = texts.pokemonName(pair.first.species, pair.first.form);
                const int index = files.personal.indexOf(pair.second.species, pair.second.form);
                const int t1 = files.personal.type1(index);
                items << "<span class=\"wild\" title=\"statt " + esc(oldName) + "\"><span class=\"dot t" +
                             QString::number(t1) + "\"></span>" + esc(newName) + "</span>";
                search += " " + newName.toLower() + " " + oldName.toLower();
            }
            rows += "<tr data-search=\"" + esc(search) + "\"><td class=\"place\">" + esc(place) + "</td><td class=\"num\">Lv. " +
                    QString::number(z.levelMin) + "–" + QString::number(z.levelMax) + "</td><td>" + items.join(" ") + "</td></tr>";
        }
        if (section.count == 0) continue;
        section.html = "<p class=\"hint\">Fahre mit der Maus über ein Pokémon, um zu sehen, welches es ersetzt. "
                       "Gezeigt wird " + QString(shield ? "Schild" : "Schwert") + "; der Mod enthält beide Versionen.</p>"
                       "<table class=\"enc\"><thead><tr><th>Ort</th><th>Level</th><th>Pokémon</th></tr></thead><tbody>" +
                       rows + "</tbody></table>";
        sections.append(section);
    }
    return sections;
}

} // namespace swsh
