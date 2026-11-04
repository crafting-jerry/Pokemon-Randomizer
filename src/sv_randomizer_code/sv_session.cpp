// ---------------------------------------------------------------------------
// Pokemon Karmesin/Purpur: Bearbeiten-Modus (siehe sv_session.h)
// ---------------------------------------------------------------------------

#include "headers/sv_randomizer_headers/sv_session.h"
#include "headers/sv_randomizer_headers/sv_trainers.h"
#include "headers/sv_randomizer_headers/sv_trainer_smart.h"
#include "headers/common/spoiler_html.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QTemporaryDir>
#include <algorithm>

namespace svedit {

namespace {

// ------------------------------------------------------------------ Hilfen

QString qs(const std::string& text) { return QString::fromStdString(text); }

QString str(const json& j, const char* key, const QString& fallback = QString()) {
    if (j.is_object() && j.contains(key) && j[key].is_string()) return qs(j[key].get<std::string>());
    return fallback;
}

int num(const json& j, const char* key, int fallback = 0) {
    if (j.is_object() && j.contains(key) && j[key].is_number()) return j[key].get<int>();
    return fallback;
}

bool flag(const json& j, const char* key) {
    return j.is_object() && j.contains(key) && j[key].is_boolean() && j[key].get<bool>();
}

bool has(const json& j, const char* key) { return j.is_object() && j.contains(key); }

json readJson(const QString& path, bool* ok) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *ok = false;
        return json();
    }
    const QByteArray data = file.readAll();
    json result = json::parse(data.constData(), data.constData() + data.size(), nullptr, false);
    *ok = !result.is_discarded();
    return result;
}

QString flatcPath() {
#if defined(_WIN32)
    return QDir::toNativeSeparators("thirdparty/flatbuffers/windows/flatc.exe");
#elif defined(__APPLE__)
#if defined(__aarch64__)
    return "thirdparty/flatbuffers/mac_ant/flatc";
#else
    return "thirdparty/flatbuffers/mac_intel/flatc";
#endif
#elif defined(__clang__)
    return "thirdparty/flatbuffers/linux_clang/flatc";
#else
    return "thirdparty/flatbuffers/linux_gcc/flatc";
#endif
}

// Binaerdatei -> JSON (mit allen Standardwerten, wie die *_clean.json)
bool binToJson(const QString& schema, const QByteArray& bin, json& out, QString* error) {
    QTemporaryDir dir;
    if (!dir.isValid()) {
        *error = "Es konnte kein temporärer Ordner angelegt werden.";
        return false;
    }
    const QString binFile = dir.filePath("data.bin");
    {
        QFile f(binFile);
        if (!f.open(QIODevice::WriteOnly)) {
            *error = "Die temporäre Datei konnte nicht geschrieben werden.";
            return false;
        }
        f.write(bin);
    }
    QProcess process;
    process.start(flatcPath(), {"--json", "--strict-json", "--defaults-json", "--raw-binary", "-o", dir.path(), schema,
                                "--", binFile});
    if (!process.waitForFinished(120000) || process.exitCode() != 0) {
        *error = "flatc konnte die Datei nicht lesen: " + QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        return false;
    }
    bool ok = false;
    out = readJson(dir.filePath("data.json"), &ok);
    if (!ok) *error = "flatc hat keine gültige JSON-Datei erzeugt.";
    return ok;
}

// ----------------------------------------------------------- Aufzaehlungen

const QStringList kSex = {"DEFAULT", "MALE", "FEMALE"};
const QStringList kSexText = {"Standard", "Männlich", "Weiblich"};
const QStringList kRare = {"DEFAULT", "NO_RARE", "RARE"};
const QStringList kRareText = {"Standard (Zufall)", "Nein", "Ja"};
const QStringList kTokusei = {"RANDOM_12", "RANDOM_123", "SET_1", "SET_2", "SET_3"};
const QStringList kSeikaku = {"DEFAULT", "GANBARIYA", "SAMISIGARIYA", "YUUKAN", "IJIPPARI", "YANTYA", "ZUBUTOI", "SUNAO",
                              "NONKI", "WANPAKU", "NOUTENKI", "OKUBYOU", "SEKKATI", "MAJIME", "YOUKI", "MUJYAKI", "HIKAEME",
                              "OTTORI", "REISEI", "TEREYA", "UKKARIYA", "ODAYAKA", "OTONASII", "NAMAIKI", "SINNTYOU",
                              "KIMAGURE"};
const QStringList kGem = {"DEFAULT", "RANDOM", "NORMAL", "KAKUTOU", "HIKOU", "DOKU", "JIMEN", "IWA", "MUSHI",
                          "GHOST", "HAGANE", "HONOO", "MIZU", "KUSA", "DENKI", "ESPER", "KOORI", "DRAGON", "AKU",
                          "FAIRY", "NIJI"};
const QStringList kWazaType = {"DEFAULT", "MANUAL"};
const QStringList kWazaTypeText = {"Spiel-Standard", "Eigene Attacken"};
const QStringList kTalentType = {"RANDOM", "VALUE", "V_NUM"};
const QStringList kTalentTypeText = {"Zufällig", "Feste Werte", "Anzahl perfekter DVs"};
const QStringList kRomVer = {"BOTH", "TYPE_A", "TYPE_B"};
const QStringList kRomVerText = {"Beide Editionen", "nur Karmesin", "nur Purpur"};
const QStringList kBalls = {"NONE", "MASUTAABOORU", "HAIPAABOORU", "SUUPAABOORU", "MONSUTAABOORU", "SAFARIBOORU",
                            "NETTOBOORU", "DAIBUBOORU", "NESUTOBOORU", "RIPIITOBOORU", "TAIMAABOORU", "GOOZYASUBOORU",
                            "PUREMIABOORU", "DAAKUBOORU", "HIIRUBOORU", "KUIKKUBOORU", "PURESYASUBOORU", "SUPIIDOBOORU",
                            "REBERUBOORU", "RUAABOORU", "HEBIIBOORU", "RABURABUBOORU", "HURENDOBOORU", "MUUNBOORU",
                            "KONPEBOORU", "DORIIMUBOORU", "URUTORABOORU"};
const QStringList kBiomes = {"GRASS", "FOREST", "SWAMP", "LAKE", "TOWN", "MOUNTAIN", "BAMBOO", "MINE", "CAVE",
                             "OLIVE", "UNDERGROUND", "RIVER", "ROCKY", "BEACH", "SNOW", "OSEAN", "RUINS", "FLOWER"};
const QStringList kBiomeText = {"Wiese", "Wald", "Sumpf", "See", "Ort", "Berg", "Bambus", "Mine", "Höhle", "Oliven",
                                "Untergrund", "Fluss", "Felsen", "Strand", "Schnee", "Meer", "Ruinen", "Blumen"};
const char* const kStatKeys[6] = {"hp", "atk", "def", "spAtk", "spDef", "agi"};
const char* const kBaseStatKeys[6] = {"hp", "atk", "def", "spa", "spd", "spe"};
const QStringList kStatText = {"KP", "Angriff", "Verteidigung", "Sp.-Angriff", "Sp.-Verteidigung", "Initiative"};

QStringList seikakuText() {
    QStringList out = {"Standard (Zufall)"};
    out << edit::natureNames();
    return out;
}

QStringList gemText(const EditSession* s) {
    QStringList out = {"Standard (eigener Typ)", "Zufällig"};
    for (int t = 0; t < 18; t++) out << s->typeName(t);
    out << "Stellar";
    return out;
}

QStringList ballText(const EditSession* s) {
    QStringList out = {"Standard"};
    for (int i = 1; i < kBalls.size(); i++) out << s->itemName("ITEMID_" + kBalls[i]);
    return out;
}

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
    for (int i = names.size(); i < 64; i++) out << QString("%1: (anderes)").arg(i);
    return out;
}

// ------------------------------------------------------------------ Felder

using Ref = std::function<json&()>;

std::function<int()> origFn(const json* o, std::function<int(const json&)> fn) {
    if (o == nullptr) return {};
    return [o, fn]() { return fn(*o); };
}

edit::Field numField(const QString& label, int min, int max, Ref r, const json* o, const char* key) {
    const std::string k(key);
    return edit::number(label, min, max, [r, k]() { return num(r(), k.c_str()); }, [r, k](int v) { r()[k] = v; },
                        origFn(o, [k](const json& j) { return num(j, k.c_str()); }));
}

edit::Field boolField(const QString& label, Ref r, const json* o, const char* key) {
    const std::string k(key);
    return edit::check(label, [r, k]() { return flag(r(), k.c_str()) ? 1 : 0; }, [r, k](int v) { r()[k] = (v != 0); },
                       origFn(o, [k](const json& j) { return flag(j, k.c_str()) ? 1 : 0; }));
}

edit::Field enumField(const QString& label, QStringList values, QStringList texts, Ref r, const json* o, const char* key) {
    const std::string k(key);
    // Unbekannte Werte (aus Mods) anhaengen, damit nichts falsch angezeigt wird
    for (const QString& v : {str(r(), key), o ? str(*o, key) : QString()}) {
        if (!v.isEmpty() && !values.contains(v)) {
            values << v;
            texts << v;
        }
    }
    auto index = [values, k](const json& j) { return std::max(0, static_cast<int>(values.indexOf(str(j, k.c_str())))); };
    return edit::choice(label, texts, [r, index]() { return index(r()); },
                        [r, k, values](int v) { if (v >= 0 && v < values.size()) r()[k] = values[v].toStdString(); },
                        origFn(o, index));
}

edit::Field devPokemonField(EditSession* s, const QString& label, Ref r, const json* o, const char* devKey,
                            const char* formKey) {
    const std::string dk(devKey), fk(formKey);
    auto value = [s, dk, fk](const json& j) {
        return edit::pokemonValue(s->natdexOfDev(str(j, dk.c_str())), num(j, fk.c_str()));
    };
    edit::Field f = edit::picker(label, edit::Kind::Pokemon, [r, value]() { return value(r()); },
                                 [s, r, dk, fk](int v) {
                                     const int natdex = edit::speciesOf(v);
                                     const QString dev = s->devOfNatdex(natdex);
                                     if (natdex <= 0 || dev.isEmpty()) return;
                                     r()[dk] = dev.toStdString();
                                     r()[fk] = edit::formOf(v);
                                 },
                                 origFn(o, value));
    f.rebuild = true;
    f.span = 2;
    return f;
}

edit::Field devidPokemonField(EditSession* s, const QString& label, Ref r, const json* o, const char* idKey,
                              const char* formKey) {
    const std::string ik(idKey), fk(formKey);
    auto value = [s, ik, fk](const json& j) {
        return edit::pokemonValue(s->natdexOfDevid(num(j, ik.c_str())), num(j, fk.c_str()));
    };
    edit::Field f = edit::picker(label, edit::Kind::Pokemon, [r, value]() { return value(r()); },
                                 [s, r, ik, fk](int v) {
                                     const int devid = s->devidOfNatdex(edit::speciesOf(v));
                                     if (devid <= 0) return;
                                     r()[ik] = devid;
                                     r()[fk] = edit::formOf(v);
                                 },
                                 origFn(o, value));
    f.rebuild = true;
    f.span = 2;
    return f;
}

edit::Field moveField(EditSession* s, const QString& label, Ref r, const json* o, const char* wazaKey) {
    const std::string k(wazaKey);
    auto value = [s, k](const json& j) { return has(j, k.c_str()) ? s->moveId(str(j[k], "wazaId")) : 0; };
    return edit::picker(label, edit::Kind::Move, [r, value]() { return value(r()); },
                        [s, r, k](int v) {
                            const QString dev = v <= 0 ? QString("WAZA_NULL") : s->moveDev(v);
                            if (!dev.isEmpty()) r()[k]["wazaId"] = dev.toStdString();
                        },
                        origFn(o, value));
}

edit::Field itemField(EditSession* s, const QString& label, Ref r, const json* o, const char* key) {
    const std::string k(key);
    auto value = [s, k](const json& j) { return s->itemIdConst(str(j, k.c_str(), "ITEMID_NONE")); };
    return edit::picker(label, edit::Kind::Item, [r, value]() { return value(r()); },
                        [s, r, k](int v) { r()[k] = s->itemDev(v).toStdString(); }, origFn(o, value));
}

// Faehigkeiten einer Art als Text fuer die Auswahl "Fähigkeit"
QStringList tokuseiText(const EditSession* s, int natdex, int form) {
    const json* p = s->personal(natdex, form);
    auto name = [s, p](const char* key) { return p ? s->abilityName(num(*p, key)) : QString("?"); };
    return {"Zufällig: " + name("ability_1") + " / " + name("ability_2"),
            "Zufällig: " + name("ability_1") + " / " + name("ability_2") + " / " + name("ability_hidden"),
            "Fähigkeit 1: " + name("ability_1"), "Fähigkeit 2: " + name("ability_2"),
            "Versteckt: " + name("ability_hidden")};
}

// Standardfelder eines Pokemon (Trainer, Geschenk, Kampf, Raid, Begegnung).
// Es werden nur Felder gezeigt, die es in dieser Tabelle gibt.
QList<edit::Field> monFields(EditSession* s, Ref r, const json* o, bool full) {
    QList<edit::Field> f;
    const json& cur = r();
    f << devPokemonField(s, "Pokémon", r, o, "devId", "formId");
    if (has(cur, "level")) f << numField("Level", 1, 100, r, o, "level");
    if (has(cur, "sex")) f << enumField("Geschlecht", kSex, kSexText, r, o, "sex");
    if (has(cur, "rareType")) f << enumField("Schillernd", kRare, kRareText, r, o, "rareType");
    if (has(cur, "item")) f << itemField(s, "Item", r, o, "item");
    const int natdex = s->natdexOfDev(str(cur, "devId"));
    const int form = num(cur, "formId");
    if (has(cur, "tokusei")) f << enumField("Fähigkeit", kTokusei, tokuseiText(s, natdex, form), r, o, "tokusei");
    if (has(cur, "tokuseiIndex")) f << enumField("Fähigkeit", kTokusei, tokuseiText(s, natdex, form), r, o, "tokuseiIndex");
    if (has(cur, "seikaku")) f << enumField("Wesen", kSeikaku, seikakuText(), r, o, "seikaku");
    if (has(cur, "gemType")) f << enumField("Tera-Typ", kGem, gemText(s), r, o, "gemType");
    if (has(cur, "ballId")) f << enumField("Ball", kBalls, ballText(s), r, o, "ballId");
    if (has(cur, "wazaType")) {
        edit::Field mode = enumField("Attacken", kWazaType, kWazaTypeText, r, o, "wazaType");
        mode.info = "Spiel-Standard: das Spiel nimmt die letzten vier Attacken, die das Pokémon bis zu seinem Level lernt. "
                    "Eigene Attacken: die vier Attacken unten.";
        f << mode;
    }
    for (int k = 1; k <= 4; k++) {
        const std::string key = "waza" + std::to_string(k);
        if (has(cur, key.c_str())) f << moveField(s, QString("Attacke %1").arg(k), r, o, key.c_str());
    }
    if (!full) return f;
    if (has(cur, "talentType")) f << enumField("DVs", kTalentType, kTalentTypeText, r, o, "talentType");
    if (has(cur, "talentVnum")) {
        edit::Field vnum = numField("Perfekte DVs", 0, 6, r, o, "talentVnum");
        vnum.info = "Nur bei „Anzahl perfekter DVs“: so viele Werte sind 31.";
        f << vnum;
    }
    if (has(cur, "talentValue")) {
        for (int k = 0; k < 6; k++) {
            const std::string key(kStatKeys[k]);
            Ref sub = [r]() -> json& { return r()["talentValue"]; };
            const json* osub = (o && has(*o, "talentValue")) ? &(*o)["talentValue"] : nullptr;
            f << numField("DV " + kStatText[k], 0, 31, sub, osub, key.c_str());
        }
    }
    if (has(cur, "effortValue")) {
        for (int k = 0; k < 6; k++) {
            const std::string key(kStatKeys[k]);
            Ref sub = [r]() -> json& { return r()["effortValue"]; };
            const json* osub = (o && has(*o, "effortValue")) ? &(*o)["effortValue"] : nullptr;
            f << numField("FP " + kStatText[k], 0, 252, sub, osub, key.c_str());
        }
    }
    return f;
}

QString monName(const EditSession* s, const json& poke) {
    return s->pokemonName(s->natdexOfDev(str(poke, "devId")), num(poke, "formId"));
}

// Warnung bei eigenen Attacken ohne eine einzige Attacke
QStringList moveWarnings(const json& poke, const QString& who) {
    if (str(poke, "wazaType") != "MANUAL") return {};
    for (int k = 1; k <= 4; k++) {
        const std::string key = "waza" + std::to_string(k);
        if (has(poke, key.c_str()) && str(poke[key], "wazaId", "WAZA_NULL") != "WAZA_NULL") return {};
    }
    return {who + ": „Eigene Attacken“ ist gewählt, aber keine Attacke eingetragen."};
}

// ----------------------------------------------------------------- Trainer

bool isEmptyMon(const json& poke) {
    const QString dev = str(poke, "devId", "DEV_NULL");
    return dev.isEmpty() || dev == "DEV_NULL";
}

int teamSize(const json& trainer) {
    int n = 0;
    for (int k = 1; k <= 6; k++) {
        if (!isEmptyMon(trainer["poke" + std::to_string(k)])) n = k;
    }
    return n;
}

json emptyMon(json slot) {
    slot["devId"] = "DEV_NULL";
    slot["formId"] = 0;
    slot["sex"] = "DEFAULT";
    slot["item"] = "ITEMID_NONE";
    slot["level"] = 0;
    slot["ballId"] = "NONE";
    slot["wazaType"] = "DEFAULT";
    for (int k = 1; k <= 4; k++) slot["waza" + std::to_string(k)] = {{"wazaId", "WAZA_NULL"}, {"pointUp", 0}};
    slot["gemType"] = "DEFAULT";
    slot["seikaku"] = "DEFAULT";
    slot["tokusei"] = "RANDOM_12";
    slot["talentType"] = "RANDOM";
    for (const char* key : kStatKeys) {
        slot["talentValue"][key] = 0;
        slot["effortValue"][key] = 0;
    }
    slot["talentVnum"] = 0;
    slot["rareType"] = "DEFAULT";
    slot["scaleType"] = "VALUE";
    slot["scaleValue"] = 128;
    return slot;
}

edit::Section trainerSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Trainer";
    section.subtitle = "Alle Trainer mit ihren Teams. Fahre mit der Maus über ein Feld, um den Vorher-Wert zu sehen.";
    DataTable* t = s->table("trainers");
    if (t == nullptr || !t->loaded) return section;

    struct Item { int category; int index; };
    QList<Item> order;
    const json& values = t->work["values"];
    for (int i = 0; i < static_cast<int>(values.size()); i++) {
        if (teamSize(values[i]) == 0 && teamSize(t->original["values"][i]) == 0) continue;
        const QString category = s->trainerLabels.value(i, {"Sonstige Trainer", "Trainer"}).first;
        int c = s->categoryOrder.indexOf(category);
        if (c < 0) c = s->categoryOrder.size();
        order.append({c, i});
    }
    std::stable_sort(order.begin(), order.end(), [](const Item& a, const Item& b) { return a.category < b.category; });

    for (const Item& it : order) {
        const int ti = it.index;
        const json* orig = &t->original["values"][ti];
        int highest = 0;
        for (int k = 1; k <= 6; k++) highest = std::max(highest, num((*orig)["poke" + std::to_string(k)], "level"));
        const QPair<QString, QString> label = s->trainerLabels.value(ti, {"Sonstige Trainer", "Trainer"});
        const QString trid = str(*orig, "trid");

        edit::Entry entry;
        entry.group = label.first;
        entry.title = label.second + "  ·  Lv. " + QString::number(highest);
        Ref trainer = [t, ti]() -> json& { return t->work["values"][ti]; };
        entry.search = [s, trainer, ti, trid]() {
            QString text = "#" + QString::number(ti) + " " + trid;
            const json& tr = trainer();
            for (int k = 1; k <= 6; k++) {
                const json& poke = tr["poke" + std::to_string(k)];
                if (!isEmptyMon(poke)) text += " " + monName(s, poke);
            }
            return text;
        };
        entry.changed = [t, ti]() { return t->work["values"][ti] != t->original["values"][ti]; };
        entry.build = [s, t, ti, trainer, orig, trid]() {
            edit::Content c;
            const json& tr = trainer();
            c.hint = QString(str(tr, "battleType") == "_2vs2" ? "Doppelkampf" : "Einzelkampf") + " · " + trid + " · #" +
                     QString::number(ti);
            edit::Field gem = boolField("Terakristallisiert", trainer, orig, "changeGem");
            gem.info = "Der Trainer kann in diesem Kampf terakristallisieren (Tera-Typ steht beim jeweiligen Pokémon).";
            c.fields << gem;
            c.warnings = [s, trainer]() {
                QStringList w;
                QHash<QString, int> count;
                const json& tr = trainer();
                for (int k = 1; k <= 6; k++) {
                    const json& poke = tr["poke" + std::to_string(k)];
                    if (isEmptyMon(poke)) continue;
                    count[str(poke, "devId")]++;
                    w << moveWarnings(poke, QString("Pokémon %1").arg(k));
                }
                for (auto i = count.constBegin(); i != count.constEnd(); ++i) {
                    if (i.value() > 1) w << s->pokemonName(s->natdexOfDev(i.key())) + " ist mehrfach im Team.";
                }
                w.sort();
                return w;
            };

            edit::Table team;
            team.title = "Team";
            team.cards = true;
            team.columnsPerLine = 4;
            team.rowCount = [trainer]() { return teamSize(trainer()); };
            team.rowTitle = [s, trainer](int r) {
                return QString("Pokémon %1 – %2").arg(r + 1).arg(monName(s, trainer()["poke" + std::to_string(r + 1)]));
            };
            team.canAdd = [trainer]() { return teamSize(trainer()) < 6; };
            team.add = [trainer]() {
                json& tr = trainer();
                const int n = teamSize(tr);
                if (n > 0 && n < 6) tr["poke" + std::to_string(n + 1)] = tr["poke" + std::to_string(n)];
            };
            team.canRemove = [trainer]() { return teamSize(trainer()) > 1; };
            team.remove = [trainer](int r) {
                json& tr = trainer();
                const int n = teamSize(tr);
                if (n <= 1 || r < 0 || r >= n) return;
                for (int k = r + 1; k < n; k++) tr["poke" + std::to_string(k)] = tr["poke" + std::to_string(k + 1)];
                tr["poke" + std::to_string(n)] = emptyMon(tr["poke" + std::to_string(n)]);
            };
            team.row = [s, t, ti, trainer](int r) {
                const std::string key = "poke" + std::to_string(r + 1);
                Ref mon = [trainer, key]() -> json& { return trainer()[key]; };
                const json& o = t->original["values"][ti][key];
                return monFields(s, mon, isEmptyMon(o) ? nullptr : &o, true);
            };
            c.tables << team;
            return c;
        };
        section.entries.append(entry);
    }
    return section;
}

// ---------------------------------------------- Starter, Geschenke, Begegnungen

QString bossGroup(const QString& label) {
    if (label.startsWith("nusi_")) return "Herrscher-Pokémon";
    if (label.startsWith("su2_legend_")) return "Legendäre Pokémon (Blaubeer-Akademie)";
    if (label.contains("legend")) return "Legendäre Pokémon";
    if (label.startsWith("SDC01") || label.startsWith("S1_")) return "Kitakami (DLC 1)";
    if (label.startsWith("SDC02") || label.startsWith("su2_")) return "Blaubeer-Akademie (DLC 2)";
    return "Weitere Kämpfe";
}

QString fixedGroup(const QString& key) {
    if (key.startsWith("su1_") || key.startsWith("t_su1")) return "Feste Begegnungen · Kitakami";
    if (key.startsWith("su2_") || key.startsWith("t_su2")) return "Feste Begegnungen · Blaubeer-Akademie";
    if (key.startsWith("area") || key.startsWith("ai_")) return "Feste Begegnungen · Paldea";
    return "Feste Begegnungen · Sonstige";
}

const QStringList kStarterScenes = {"d030", "common_0060_always", "common_0060_main", "common_0070_always",
                                    "common_0088_always", "common_0090_main"};

edit::Section encounterSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Starter & Begegnungen";
    section.subtitle = "Starter, Geschenke, Kämpfe gegen besondere Pokémon und feste Begegnungen in der Spielwelt.";

    // Starter und Geschenke
    if (DataTable* t = s->table("starters"); t && t->loaded) {
        const json& values = t->original["values"];
        for (int i = 0; i < static_cast<int>(values.size()); i++) {
            const json* orig = &values[i]["pokeData"];
            const QString label = str(values[i], "label");
            edit::Entry entry;
            static const QStringList starterNames = {"Starter Feuer", "Starter Pflanze", "Starter Wasser"};
            entry.group = i < 3 ? "Starter" : "Geschenke";
            entry.title = (i < 3 ? starterNames[i] : QString("Geschenk")) + " · " + monName(s, *orig) + "  (" + label + ")";
            Ref mon = [t, i]() -> json& { return t->work["values"][i]["pokeData"]; };
            entry.search = [s, mon, label]() { return label + " " + monName(s, mon()); };
            entry.changed = [t, i]() { return t->work["values"][i] != t->original["values"][i]; };
            entry.build = [s, mon, orig, i]() {
                edit::Content c;
                c.fieldsPerLine = 4;
                if (i < 3 || i == 8) {
                    c.hint = "Die Pokémon-Modelle in den Zwischensequenzen werden beim Export passend zur Art, Form, "
                             "Geschlecht und Schillernd angepasst.";
                }
                if (i < 3) {
                    c.hint += "\nNemila hat je nach Starter-Platz ein eigenes Team (Trainer → Rivalen & Freunde) – es ändert sich nicht automatisch mit.";
                }
                c.fields = monFields(s, mon, orig, true);
                c.warnings = [mon]() { return moveWarnings(mon(), "Pokémon"); };
                return c;
            };
            section.entries.append(entry);
        }
    }

    // Kaempfe (Herrscher, Legendaere, Story)
    if (DataTable* t = s->table("bosses"); t && t->loaded) {
        const json& values = t->original["values"];
        QList<QPair<QString, int>> order;
        for (int i = 0; i < static_cast<int>(values.size()); i++) order.append({bossGroup(str(values[i], "label")), i});
        std::stable_sort(order.begin(), order.end(), [](const QPair<QString, int>& a, const QPair<QString, int>& b) {
            static const QStringList groups = {"Herrscher-Pokémon", "Legendäre Pokémon", "Kitakami (DLC 1)",
                                               "Blaubeer-Akademie (DLC 2)", "Legendäre Pokémon (Blaubeer-Akademie)",
                                               "Weitere Kämpfe"};
            return groups.indexOf(a.first) < groups.indexOf(b.first);
        });
        for (const auto& item : order) {
            const int i = item.second;
            const json* orig = &values[i]["pokeData"];
            const QString label = str(values[i], "label");
            edit::Entry entry;
            entry.group = item.first;
            entry.title = monName(s, *orig) + " · Lv. " + QString::number(num(*orig, "level")) + "  (" + label + ")";
            Ref mon = [t, i]() -> json& { return t->work["values"][i]["pokeData"]; };
            entry.search = [s, mon, label]() { return label + " " + monName(s, mon()); };
            entry.changed = [t, i]() { return t->work["values"][i] != t->original["values"][i]; };
            entry.build = [s, mon, orig]() {
                edit::Content c;
                c.fieldsPerLine = 4;
                c.hint = "Kampf gegen ein besonderes Pokémon. Achtung: Modelle in Zwischensequenzen ändern sich beim "
                         "Bearbeiten nicht mit – bei einer anderen Art zeigt die Szene eventuell weiter das alte Pokémon.";
                c.fields = monFields(s, mon, orig, true);
                c.warnings = [mon]() { return moveWarnings(mon(), "Pokémon"); };
                return c;
            };
            section.entries.append(entry);
        }
    }

    // Feste Begegnungen in der Spielwelt
    if (DataTable* t = s->table("fixed"); t && t->loaded) {
        const json& values = t->original["values"];
        QList<QPair<QString, int>> order;
        for (int i = 0; i < static_cast<int>(values.size()); i++) order.append({fixedGroup(str(values[i], "tableKey")), i});
        std::stable_sort(order.begin(), order.end(), [](const QPair<QString, int>& a, const QPair<QString, int>& b) {
            static const QStringList groups = {"Feste Begegnungen · Paldea", "Feste Begegnungen · Kitakami",
                                               "Feste Begegnungen · Blaubeer-Akademie", "Feste Begegnungen · Sonstige"};
            return groups.indexOf(a.first) < groups.indexOf(b.first);
        });
        for (const auto& item : order) {
            const int i = item.second;
            const json* orig = &values[i]["pokeDataSymbol"];
            const QString key = str(values[i], "tableKey");
            edit::Entry entry;
            entry.group = item.first;
            entry.title = monName(s, *orig) + " · Lv. " + QString::number(num(*orig, "level")) + "  (" + key + ")";
            Ref mon = [t, i]() -> json& { return t->work["values"][i]["pokeDataSymbol"]; };
            entry.search = [s, mon, key]() { return key + " " + monName(s, mon()); };
            entry.changed = [t, i]() { return t->work["values"][i] != t->original["values"][i]; };
            entry.build = [s, mon, orig]() {
                edit::Content c;
                c.fieldsPerLine = 4;
                c.hint = "Pokémon, das an einer festen Stelle in der Spielwelt steht.";
                c.fields = monFields(s, mon, orig, true);
                c.warnings = [mon]() { return moveWarnings(mon(), "Pokémon"); };
                return c;
            };
            section.entries.append(entry);
        }
    }
    return section;
}

// ----------------------------------------------------------- Wilde Pokemon

struct WildInfo { const char* key; const char* region; };
const WildInfo kWild[3] = {{"wild", "Paldea"}, {"wild_su1", "Kitakami"}, {"wild_su2", "Blaubeer-Akademie"}};

edit::Section wildSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Wilde Pokémon";
    section.subtitle = "Je Pokémon: Level, Häufigkeit, Lebensräume, Tageszeit und Edition.";
    for (const WildInfo& info : kWild) {
        DataTable* t = s->table(info.key);
        const QString region = info.region;
        if (t == nullptr || !t->loaded) {
            edit::Entry entry;
            entry.group = region;
            entry.title = "Nicht randomisiert";
            entry.build = [region]() {
                edit::Content c;
                c.hint = "Die Original-Tabellen der wilden Pokémon sind im Randomizer nicht enthalten. Bearbeiten geht "
                         "deshalb nur, wenn die wilden Pokémon in " + region + " randomisiert wurden (oder die geöffnete "
                         "Mod sie enthält).";
                return c;
            };
            section.entries.append(entry);
            continue;
        }
        const json& values = t->work["values"];
        for (int i = 0; i < static_cast<int>(values.size()); i++) {
            const json* orig = (i < static_cast<int>(t->original["values"].size())) ? &t->original["values"][i] : nullptr;
            Ref row = [t, i]() -> json& { return t->work["values"][i]; };
            edit::Entry entry;
            entry.group = region;
            const json& cur = values[i];
            entry.title = s->pokemonName(s->natdexOfDev(str(cur, "devid")), num(cur, "formno")) + " · Lv. " +
                          QString::number(num(cur, "minlevel")) + "–" + QString::number(num(cur, "maxlevel"));
            entry.search = [s, row]() {
                const json& r = row();
                QString text = s->pokemonName(s->natdexOfDev(str(r, "devid")), num(r, "formno"));
                for (int b = 1; b <= 4; b++) {
                    const int bi = kBiomes.indexOf(str(r, ("biome" + std::to_string(b)).c_str()));
                    if (bi >= 0) text += " " + kBiomeText[bi];
                }
                return text;
            };
            entry.changed = [t, i]() {
                return i >= static_cast<int>(t->original["values"].size()) || t->work["values"][i] != t->original["values"][i];
            };
            entry.build = [s, row, orig]() {
                edit::Content c;
                c.fieldsPerLine = 4;
                c.hint = "Vorher-Werte sind hier der Stand beim Öffnen (die Original-Tabellen sind nicht enthalten). "
                         "„Gebiete“ ist die Liste der Gebietsnummern, in denen das Pokémon erscheint.";
                c.fields << devPokemonField(s, "Pokémon", row, orig, "devid", "formno");
                c.fields << enumField("Geschlecht", kSex, kSexText, row, orig, "sex");
                c.fields << numField("Level von", 1, 100, row, orig, "minlevel");
                c.fields << numField("Level bis", 1, 100, row, orig, "maxlevel");
                c.fields << numField("Häufigkeit", 0, 1000, row, orig, "lotvalue");
                for (int b = 1; b <= 4; b++) {
                    const std::string bk = "biome" + std::to_string(b), lk = "lotvalue" + std::to_string(b);
                    QStringList values = kBiomes, texts = kBiomeText;
                    values.prepend("NONE");
                    texts.prepend("—");
                    c.fields << enumField(QString("Lebensraum %1").arg(b), values, texts, row, orig, bk.c_str());
                    c.fields << numField(QString("Häufigkeit %1").arg(b), 0, 1000, row, orig, lk.c_str());
                }
                Ref version = [row]() -> json& { return row()["versiontable"]; };
                const json* oversion = (orig && has(*orig, "versiontable")) ? &(*orig)["versiontable"] : nullptr;
                c.fields << boolField("Karmesin", version, oversion, "A");
                c.fields << boolField("Purpur", version, oversion, "B");
                Ref times = [row]() -> json& { return row()["timetable"]; };
                const json* otimes = (orig && has(*orig, "timetable")) ? &(*orig)["timetable"] : nullptr;
                c.fields << boolField("Morgens", times, otimes, "morning");
                c.fields << boolField("Mittags", times, otimes, "noon");
                c.fields << boolField("Abends", times, otimes, "evening");
                c.fields << boolField("Nachts", times, otimes, "night");
                Ref places = [row]() -> json& { return row()["enabletable"]; };
                const json* oplaces = (orig && has(*orig, "enabletable")) ? &(*orig)["enabletable"] : nullptr;
                c.fields << boolField("An Land", places, oplaces, "land");
                c.fields << boolField("Auf dem Wasser", places, oplaces, "up_water");
                c.fields << boolField("Unter Wasser", places, oplaces, "underwater");
                c.fields << boolField("In der Luft 1", places, oplaces, "air1");
                c.fields << boolField("In der Luft 2", places, oplaces, "air2");
                Ref bring = [row]() -> json& { return row()["bringItem"]; };
                const json* obring = (orig && has(*orig, "bringItem")) ? &(*orig)["bringItem"] : nullptr;
                c.fields << itemField(s, "Getragenes Item", bring, obring, "itemID");
                c.fields << numField("Item-Chance (%)", 0, 100, bring, obring, "bringRate");
                c.fields << edit::text("Gebiete", [row]() { return str(row(), "area"); });
                c.warnings = [row]() {
                    QStringList w;
                    const json& r = row();
                    if (num(r, "minlevel") > num(r, "maxlevel")) w << "„Level von“ ist größer als „Level bis“.";
                    if (!flag(r["versiontable"], "A") && !flag(r["versiontable"], "B"))
                        w << "Das Pokémon erscheint in keiner Edition.";
                    const json& tt = r["timetable"];
                    if (!flag(tt, "morning") && !flag(tt, "noon") && !flag(tt, "evening") && !flag(tt, "night"))
                        w << "Das Pokémon erscheint zu keiner Tageszeit.";
                    return w;
                };
                return c;
            };
            section.entries.append(entry);
        }
    }
    return section;
}

// -------------------------------------------------------------- Tera-Raids

struct RaidRegion { const char* prefix; const char* region; };
const RaidRegion kRaidRegions[3] = {{"", "Paldea"}, {"su1_", "Kitakami"}, {"su2_", "Blaubeer-Akademie"}};

QString raidName(const char* prefix, int stars) {
    return QString("%1raid_enemy_%2").arg(prefix).arg(stars, 2, 10, QChar('0'));
}

edit::Section raidSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Tera-Raids";
    section.subtitle = "Alle Raid-Pokémon nach Region und Sternen.";
    for (const RaidRegion& region : kRaidRegions) {
        for (int stars = 1; stars <= 6; stars++) {
            DataTable* t = s->table("raid:" + raidName(region.prefix, stars));
            if (t == nullptr || !t->loaded) continue;
            const json& values = t->original["values"];
            const QString group = QString(region.region) + " · " + QString(stars, QChar(0x2605));
            for (int i = 0; i < static_cast<int>(values.size()); i++) {
                const json* oinfo = &values[i]["raidEnemyInfo"];
                const json* omon = &(*oinfo)["bossPokePara"];
                edit::Entry entry;
                entry.group = group;
                const QString romVer = str(*oinfo, "romVer");
                entry.title = monName(s, *omon) + (romVer == "TYPE_A" ? "  (Karmesin)" : romVer == "TYPE_B" ? "  (Purpur)" : "");
                Ref info = [t, i]() -> json& { return t->work["values"][i]["raidEnemyInfo"]; };
                Ref mon = [info]() -> json& { return info()["bossPokePara"]; };
                entry.search = [s, mon]() { return monName(s, mon()) + " raid tera"; };
                entry.changed = [t, i]() { return t->work["values"][i] != t->original["values"][i]; };
                entry.build = [s, info, mon, oinfo, omon]() {
                    edit::Content c;
                    c.fieldsPerLine = 4;
                    c.hint = "Raid-Nr. " + QString::number(num(info(), "no")) + ". Fangen ist ab dem Fang-Level möglich.";
                    c.fields << enumField("Edition", kRomVer, kRomVerText, info, oinfo, "romVer");
                    c.fields << numField("Häufigkeit", 0, 1000, info, oinfo, "rate");
                    c.fields << numField("Fang-Level", 1, 100, info, oinfo, "captureLv");
                    c.fields << monFields(s, mon, omon, true);
                    c.warnings = [mon]() { return moveWarnings(mon(), "Raid-Pokémon"); };
                    return c;
                };
                section.entries.append(entry);
            }
        }
    }
    return section;
}

// ------------------------------------------------------------------ Items

struct HiddenInfo { const char* key; const char* title; };
const HiddenInfo kHidden[4] = {{"hidden", "Paldea"}, {"hidden_su1", "Kitakami"}, {"hidden_su2", "Blaubeer-Akademie"},
                               {"hidden_lc", "Tabelle „lc“"}};

edit::Section itemSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Items";
    section.subtitle = "Versteckte Items, Materialien wilder Pokémon, Mitnahme und Items aus dem Let's-Go-Suchen.";

    for (const HiddenInfo& h : kHidden) {
        DataTable* t = s->table(h.key);
        if (t == nullptr || !t->loaded) continue;
        const json& values = t->original["values"];
        for (int i = 0; i < static_cast<int>(values.size()); i++) {
            edit::Entry entry;
            entry.group = QString("Versteckte Items · ") + h.title;
            entry.title = "Tabelle " + str(values[i], "table_Id");
            Ref row = [t, i]() -> json& { return t->work["values"][i]; };
            entry.search = [s, row]() {
                QString text;
                for (int k = 1; k <= 10; k++) text += " " + s->itemName(str(row()["item_" + std::to_string(k)], "itemId"));
                return text;
            };
            entry.changed = [t, i]() { return t->work["values"][i] != t->original["values"][i]; };
            entry.build = [s, t, i, row]() {
                edit::Content c;
                c.hint = "Funkelnde Items am Boden. Chance: Gewicht im Vergleich zu den anderen Zeilen.";
                edit::Table table;
                table.title = "Items";
                table.columns = {"Item", "Chance", "Anzahl"};
                table.rowCount = []() { return 10; };
                table.row = [s, t, i, row](int r) {
                    const std::string key = "item_" + std::to_string(r + 1);
                    Ref cell = [row, key]() -> json& { return row()[key]; };
                    const json* o = &t->original["values"][i][key];
                    return QList<edit::Field>{itemField(s, "Item", cell, o, "itemId"),
                                              numField("Chance", 0, 10000, cell, o, "emergePercent"),
                                              numField("Anzahl", 0, 999, cell, o, "dropCount")};
                };
                c.tables << table;
                return c;
            };
            section.entries.append(entry);
        }
    }

    if (DataTable* t = s->table("drops"); t && t->loaded) {
        const json& values = t->original["values"];
        for (int i = 0; i < static_cast<int>(values.size()); i++) {
            edit::Entry entry;
            entry.group = "Materialien wilder Pokémon";
            entry.title = s->pokemonName(s->natdexOfDev(str(values[i], "devid")));
            Ref row = [t, i]() -> json& { return t->work["values"][i]["item1"]; };
            const json* o = &values[i]["item1"];
            entry.search = [s, row]() { return s->itemName(str(row(), "itemid")); };
            entry.changed = [t, i]() { return t->work["values"][i] != t->original["values"][i]; };
            entry.build = [s, row, o]() {
                edit::Content c;
                c.hint = "Item, das dieses Pokémon nach einem Kampf oder beim Let's-Go-Kampf fallen lässt.";
                c.fields << itemField(s, "Item", row, o, "itemid");
                c.fields << numField("Chance (%)", 0, 100, row, o, "rate");
                c.fields << numField("Anzahl", 0, 99, row, o, "num");
                return c;
            };
            section.entries.append(entry);
        }
    }

    if (DataTable* t = s->table("monohiroi"); t && t->loaded) {
        const json& values = t->original["values"];
        for (int i = 0; i < static_cast<int>(values.size()); i++) {
            edit::Entry entry;
            entry.group = "Mitnahme";
            entry.title = QString("Tabelle %1").arg(i + 1);
            Ref row = [t, i]() -> json& { return t->work["values"][i]; };
            entry.search = [s, row]() {
                QString text;
                for (auto it = row().begin(); it != row().end(); ++it) {
                    if (it.value().is_object()) text += " " + s->itemName(str(it.value(), "itemId"));
                }
                return text;
            };
            entry.changed = [t, i]() { return t->work["values"][i] != t->original["values"][i]; };
            entry.build = [s, t, i, row]() {
                edit::Content c;
                c.hint = "Items, die Pokémon mit der Fähigkeit Mitnahme finden. Chance in Prozent.";
                QStringList keys;
                for (auto it = t->original["values"][i].begin(); it != t->original["values"][i].end(); ++it) {
                    if (it.value().is_object()) keys << qs(it.key());
                }
                edit::Table table;
                table.title = "Items";
                table.columns = {"Item", "Chance"};
                table.rowCount = [keys]() { return static_cast<int>(keys.size()); };
                table.row = [s, t, i, row, keys](int r) {
                    const std::string key = keys[r].toStdString();
                    Ref cell = [row, key]() -> json& { return row()[key]; };
                    const json* o = &t->original["values"][i][key];
                    return QList<edit::Field>{itemField(s, "Item", cell, o, "itemId"), numField("Chance", 0, 100, cell, o, "rate")};
                };
                c.tables << table;
                c.warnings = [row]() {
                    int sum = 0;
                    for (auto it = row().begin(); it != row().end(); ++it) {
                        if (it.value().is_object()) sum += num(it.value(), "rate");
                    }
                    return sum == 100 || sum == 0 ? QStringList() : QStringList{QString("Die Chancen ergeben zusammen %1 % statt 100 %.").arg(sum)};
                };
                return c;
            };
            section.entries.append(entry);
        }
    }

    if (DataTable* t = s->table("rummaging"); t && t->loaded) {
        const json& values = t->original["values"];
        for (int i = 0; i < static_cast<int>(values.size()); i++) {
            edit::Entry entry;
            entry.group = "Let's-Go-Suchen";
            entry.title = str(values[i], "Category") + " · " + str(values[i], "Pattern");
            Ref row = [t, i]() -> json& { return t->work["values"][i]; };
            const json* o = &values[i];
            entry.search = [s, row]() {
                QString text;
                for (int k = 0; k < 5; k++) text += " " + s->itemName(str(row(), QString("Item0%1").arg(k).toLatin1().constData()));
                return text;
            };
            entry.changed = [t, i]() { return t->work["values"][i] != t->original["values"][i]; };
            entry.build = [s, row, o]() {
                edit::Content c;
                c.hint = "Items, die dein Pokémon beim Let's-Go-Suchen an dieser Stelle (Busch, Felsen …) findet.";
                for (int k = 0; k < 5; k++) {
                    const QByteArray key = QString("Item0%1").arg(k).toLatin1();
                    c.fields << itemField(s, QString("Item %1").arg(k + 1), row, o, key.constData());
                }
                return c;
            };
            section.entries.append(entry);
        }
    }
    return section;
}

// ---------------------------------------------------------- Pokemon-Daten

edit::Section dataSection(EditSession& session) {
    EditSession* s = &session;
    edit::Section section;
    section.title = "Pokémon-Daten";
    section.subtitle = "Typen, Basiswerte, Fähigkeiten, Level-Attacken, TMs und Entwicklungen jedes Pokémon.";
    DataTable* t = s->table("personal");
    if (t == nullptr || !t->loaded) return section;
    static const QList<QPair<int, QString>> generations = {
        {151, "Kanto (1–151)"}, {251, "Johto (152–251)"}, {386, "Hoenn (252–386)"}, {493, "Sinnoh (387–493)"},
        {649, "Einall (494–649)"}, {721, "Kalos (650–721)"}, {809, "Alola (722–809)"}, {905, "Galar und Hisui (810–905)"},
        {1025, "Paldea (906–1025)"}};

    struct Item { int natdex; int form; int index; };
    QList<Item> order;
    const json& entries = t->original["entry"];
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        if (!flag(entries[i], "is_present") && !flag(t->work["entry"][i], "is_present")) continue;
        const int natdex = s->natdexOfDevid(num(entries[i]["species"], "species"));
        if (natdex <= 0) continue;
        order.append({natdex, num(entries[i]["species"], "form"), i});
    }
    std::sort(order.begin(), order.end(), [](const Item& a, const Item& b) {
        return a.natdex != b.natdex ? a.natdex < b.natdex : a.form < b.form;
    });

    for (const Item& it : order) {
        const int index = it.index;
        edit::Entry entry;
        for (const auto& g : generations) {
            if (it.natdex <= g.first) { entry.group = g.second; break; }
        }
        entry.title = QString("#%1 %2").arg(it.natdex, 4, 10, QChar('0')).arg(s->pokemonName(it.natdex, it.form));
        entry.changed = [t, index]() { return t->work["entry"][index] != t->original["entry"][index]; };
        entry.build = [s, t, index]() {
            edit::Content c;
            c.fieldsPerLine = 4;
            c.hint = "Werte gelten nur für diese Form. Entwicklungsreihen werden beim Bearbeiten nicht automatisch angeglichen.";
            Ref p = [t, index]() -> json& { return t->work["entry"][index]; };
            const json* o = &t->original["entry"][index];
            auto typeField = [p, o](const QString& label, const char* key) {
                const std::string k(key);
                return edit::picker(label, edit::Kind::Type, [p, k]() { return num(p(), k.c_str()); },
                                    [p, k](int v) { p()[k] = v; }, origFn(o, [k](const json& j) { return num(j, k.c_str()); }));
            };
            c.fields << typeField("Typ 1", "type_1") << typeField("Typ 2", "type_2");
            auto abilityField = [p, o](const QString& label, const char* key) {
                const std::string k(key);
                return edit::picker(label, edit::Kind::Ability, [p, k]() { return num(p(), k.c_str()); },
                                    [p, k](int v) { if (v > 0) p()[k] = v; },
                                    origFn(o, [k](const json& j) { return num(j, k.c_str()); }));
            };
            c.fields << abilityField("Fähigkeit 1", "ability_1") << abilityField("Fähigkeit 2", "ability_2")
                     << abilityField("Versteckte Fähigkeit", "ability_hidden");
            c.fields << edit::text("Summe der Basiswerte", [p]() {
                int sum = 0;
                for (const char* key : kBaseStatKeys) sum += num(p()["base_stats"], key);
                return QString::number(sum);
            });
            Ref stats = [p]() -> json& { return p()["base_stats"]; };
            for (int k = 0; k < 6; k++) {
                edit::Field f = numField(kStatText[k], 1, 255, stats, &(*o)["base_stats"], kBaseStatKeys[k]);
                f.rebuild = true;
                c.fields << f;
            }
            c.fields << numField("Fangrate", 0, 255, p, o, "catch_rate");
            c.fields << boolField("Im Spiel vorhanden", p, o, "is_present");

            // Level-Attacken
            edit::Table moves;
            moves.title = "Level-Attacken";
            moves.columns = {"Level", "Attacke"};
            Ref list = [p]() -> json& { return p()["levelup_moves"]; };
            const json* olist = &(*o)["levelup_moves"];
            moves.rowCount = [list]() { return static_cast<int>(list().size()); };
            moves.row = [s, list, olist](int r) {
                Ref cell = [list, r]() -> json& { return list()[r]; };
                const json* oc = r < static_cast<int>(olist->size()) ? &(*olist)[r] : nullptr;
                edit::Field level = numField("Level", 0, 100, cell, oc, "level");
                level.info = "0 = beim Entwickeln";
                edit::Field move = edit::picker("Attacke", edit::Kind::Move, [cell]() { return num(cell(), "move"); },
                                                [cell](int v) { if (v > 0) cell()["move"] = v; },
                                                origFn(oc, [](const json& j) { return num(j, "move"); }));
                return QList<edit::Field>{level, move};
            };
            moves.canAdd = [list]() { return list().size() < 60; };
            moves.add = [list]() {
                json& l = list();
                const int level = l.empty() ? 1 : num(l.back(), "level");
                l.push_back({{"move", 1}, {"level", level}});
            };
            moves.canRemove = [list]() { return list().size() > 1; };
            moves.remove = [list](int r) {
                json& l = list();
                if (r >= 0 && r < static_cast<int>(l.size())) l.erase(l.begin() + r);
            };
            c.tables << moves;

            // TMs
            edit::Table tms;
            tms.title = "TM-Attacken";
            tms.columns = {"Attacke"};
            Ref tmList = [p]() -> json& { return p()["tm_moves"]; };
            const json* otm = &(*o)["tm_moves"];
            tms.rowCount = [tmList]() { return static_cast<int>(tmList().size()); };
            tms.row = [tmList, otm](int r) {
                const bool hasOrig = r < static_cast<int>(otm->size());
                return QList<edit::Field>{edit::picker("Attacke", edit::Kind::Move,
                    [tmList, r]() { return tmList()[r].get<int>(); },
                    [tmList, r](int v) { if (v > 0) tmList()[r] = v; },
                    hasOrig ? std::function<int()>([otm, r]() { return (*otm)[r].get<int>(); }) : std::function<int()>())};
            };
            tms.canAdd = [tmList]() { return tmList().size() < 250; };
            tms.add = [tmList]() { tmList().push_back(tmList().empty() ? 1 : tmList().back().get<int>()); };
            tms.canRemove = [tmList]() { return !tmList().empty(); };
            tms.remove = [tmList](int r) {
                json& l = tmList();
                if (r >= 0 && r < static_cast<int>(l.size())) l.erase(l.begin() + r);
            };
            c.tables << tms;

            // Entwicklungen
            edit::Table evos;
            evos.title = "Entwicklungen";
            evos.columns = {"Methode", "Wert", "Entwickelt sich zu", "Level"};
            Ref evoList = [p]() -> json& { return p()["evolutions"]; };
            const json* oevo = &(*o)["evolutions"];
            evos.rowCount = [evoList]() { return static_cast<int>(evoList().size()); };
            evos.row = [s, evoList, oevo](int r) {
                Ref cell = [evoList, r]() -> json& { return evoList()[r]; };
                const json* oc = r < static_cast<int>(oevo->size()) ? &(*oevo)[r] : nullptr;
                edit::Field method = edit::choice("Methode", evolutionMethods(), [cell]() { return num(cell(), "condition"); },
                                                  [cell](int v) { if (v > 0) cell()["condition"] = v; },
                                                  origFn(oc, [](const json& j) { return num(j, "condition"); }));
                edit::Field arg = numField("Wert", 0, 65535, cell, oc, "parameter");
                arg.info = "Zusatzwert der Methode: Item-Nr. (Item benutzen, mit Item), Attacken-Nr. oder Pokémon-Nr.";
                edit::Field target = devidPokemonField(s, "Ziel", cell, oc, "species", "form");
                target.rebuild = false;
                target.span = 1;
                return QList<edit::Field>{method, arg, target, numField("Level", 0, 100, cell, oc, "level")};
            };
            evos.canAdd = [evoList]() { return evoList().size() < 8; };
            evos.add = [evoList, p]() {
                const json& sp = p()["species"];
                evoList().push_back({{"level", 100}, {"condition", 4}, {"parameter", 0}, {"reserved3", 0}, {"reserved4", 0},
                                     {"reserved5", 0}, {"species", num(sp, "species")}, {"form", num(sp, "form")}});
            };
            evos.remove = [evoList](int r) {
                json& l = evoList();
                if (r >= 0 && r < static_cast<int>(l.size())) l.erase(l.begin() + r);
            };
            c.tables << evos;
            c.warnings = [p, s]() {
                QStringList w;
                const json& e = p();
                if (num(e, "ability_1") <= 0) w << "Fähigkeit 1 fehlt.";
                for (const json& evo : e["evolutions"]) {
                    if (s->natdexOfDevid(num(evo, "species")) <= 0) w << "Eine Entwicklung hat kein gültiges Ziel.";
                }
                return w;
            };
            return c;
        };
        section.entries.append(entry);
    }
    return section;
}

// ---------------------------------------------------------- Spoiler-Log

QString esc(const QString& text) { return text.toHtmlEscaped(); }

QString typeBadge(const EditSession& s, int type) {
    return "<span class=\"type t" + QString::number(type) + "\">" + esc(s.typeName(type)) + "</span>";
}

QString teraText(const EditSession& s, const QString& gem) {
    const int index = kGem.indexOf(gem);
    if (index >= 2 && index < 20) return typeBadge(s, index - 2);
    if (gem == "NIJI") return "Stellar";
    if (gem == "RANDOM") return "zufällig";
    return QString();
}

// Karte eines Pokemon (Trainer, Raids). search wird um die Namen ergaenzt.
QString monCard(const EditSession& s, const TrainerSmartData& smart, const json& poke, bool canTera, QString& search) {
    const int natdex = s.natdexOfDev(str(poke, "devId"));
    const int form = num(poke, "formId");
    const int level = num(poke, "level");
    const QString name = s.pokemonName(natdex, form);
    QStringList moves;
    const bool isDefault = str(poke, "wazaType", "DEFAULT") != "MANUAL";
    if (isDefault) {
        for (int id : smart.defaultMoveset(natdex, form, level)) moves << s.moveName(id);
    } else {
        for (int k = 1; k <= 4; k++) {
            const int id = s.moveId(str(poke["waza" + std::to_string(k)], "wazaId"));
            if (id > 0) moves << s.moveName(id);
        }
    }
    const QString item = str(poke, "item", "ITEMID_NONE");
    const QString itemText = item == "ITEMID_NONE" ? QString() : s.itemName(item);
    search += " " + name + " " + moves.join(" ") + " " + itemText;

    QString card = "<div class=\"mon\"><div class=\"mon-head\"><span class=\"mon-name\">" + esc(name) + "</span>";
    if (str(poke, "rareType") == "RARE") card += "<span class=\"shiny\" title=\"Schillernd\">✦</span>";
    card += "<span class=\"lvl\">Lv. " + QString::number(level) + "</span></div>";
    const QPair<int, int> types = smart.types(natdex, form);
    card += "<div class=\"types\">" + typeBadge(s, types.first);
    if (types.second != types.first) card += typeBadge(s, types.second);
    card += "</div>";
    const QString tera = teraText(s, str(poke, "gemType"));
    if (!tera.isEmpty()) {
        card += "<div class=\"meta\">Tera: " + tera + (canTera ? " <span class=\"hint\">(terakristallisiert)</span>" : "") + "</div>";
    }
    if (!itemText.isEmpty()) card += "<div class=\"meta\">Item: " + esc(itemText) + "</div>";
    card += "<ul class=\"moves\">";
    for (const QString& m : moves) card += "<li>" + esc(m) + "</li>";
    if (moves.isEmpty()) card += "<li class=\"hint\">keine Daten</li>";
    card += "</ul>";
    if (isDefault && !moves.isEmpty()) card += "<div class=\"hint\">Spiel-Standard</div>";
    card += "</div>";
    return card;
}

QString sectionHtml(const QString& category, const QString& countText, const QString& body, bool open) {
    return "<details class=\"section\" data-cat=\"" + esc(category) + "\"" + (open ? " open" : "") + "><summary>" +
           esc(category) + "<span class=\"count\">" + esc(countText) + "</span></summary><div class=\"section-body\">" +
           body + "</div></details>";
}

QString monShort(const EditSession& s, const json& poke) {
    QString text = s.pokemonName(s.natdexOfDev(str(poke, "devId")), num(poke, "formId"));
    if (str(poke, "rareType") == "RARE") text += " ✦";
    return text;
}

bool changedSinceStart(const DataTable* t, const char* root, int i) {
    return t->work[root][i] != t->start[root][i];
}

// Pokemon-Tabelle "vorher -> jetzt" (Starter, Kaempfe, feste Begegnungen)
QString encounterRows(const EditSession& s, const DataTable* t, const char* monKey, const char* labelKey, bool onlyChanged,
                      int& count) {
    QString rows;
    const json& values = t->work["values"];
    for (int i = 0; i < static_cast<int>(values.size()); i++) {
        const json& now = values[i][monKey];
        const json& before = t->original["values"][i][monKey];
        if (onlyChanged && values[i] == t->original["values"][i]) continue;
        count++;
        const QString label = str(values[i], labelKey);
        const QString beforeText = monShort(s, before);
        const QString nowText = monShort(s, now);
        QString extra;
        const QString tera = teraText(s, str(now, "gemType"));
        if (!tera.isEmpty()) extra += " Tera: " + tera;
        if (s.randomized && changedSinceStart(t, "values", i)) extra += " <span class=\"hint\">(von Hand)</span>";
        rows += "<tr data-search=\"" + esc((label + " " + beforeText + " " + nowText).toLower()) + "\"><td class=\"place\">" +
                esc(label) + "</td><td>" + esc(beforeText) + " <span class=\"arrow\">→</span> <b>" + esc(nowText) +
                "</b>" + extra + "</td><td class=\"num\">Lv. " + QString::number(num(now, "level")) + "</td></tr>";
    }
    return rows;
}

QString encTable(const QString& head, const QString& rows) {
    return "<table class=\"enc\"><tr>" + head + "</tr>" + rows + "</table>";
}

QString personalChanges(const EditSession& s, const json& a, const json& b) {
    QStringList parts;
    auto typeText = [&s](const json& j) {
        const int t1 = num(j, "type_1"), t2 = num(j, "type_2");
        return s.typeName(t1) + (t2 != t1 ? " / " + s.typeName(t2) : QString());
    };
    if (num(a, "type_1") != num(b, "type_1") || num(a, "type_2") != num(b, "type_2"))
        parts << "Typ: " + typeText(a) + " → " + typeText(b);
    auto abilityText = [&s](const json& j) {
        return s.abilityName(num(j, "ability_1")) + " / " + s.abilityName(num(j, "ability_2")) + " / " +
               s.abilityName(num(j, "ability_hidden"));
    };
    if (abilityText(a) != abilityText(b)) parts << "Fähigkeiten: " + abilityText(a) + " → " + abilityText(b);
    if (a["base_stats"] != b["base_stats"]) {
        QStringList before, after;
        for (const char* key : kBaseStatKeys) {
            before << QString::number(num(a["base_stats"], key));
            after << QString::number(num(b["base_stats"], key));
        }
        parts << "Basiswerte: " + before.join("/") + " → " + after.join("/");
    }
    if (a["levelup_moves"] != b["levelup_moves"]) {
        QStringList moves;
        for (const json& m : b["levelup_moves"]) moves << QString("%1 (Lv. %2)").arg(s.moveName(num(m, "move"))).arg(num(m, "level"));
        parts << "Level-Attacken: " + moves.join(", ");
    }
    if (a["tm_moves"] != b["tm_moves"]) parts << QString("TM-Attacken: %1 statt %2").arg(b["tm_moves"].size()).arg(a["tm_moves"].size());
    if (a["evolutions"] != b["evolutions"]) {
        QStringList evos;
        for (const json& e : b["evolutions"]) {
            evos << s.pokemonName(s.natdexOfDevid(num(e, "species")), num(e, "form")) +
                    (num(e, "level") > 0 ? QString(" (Lv. %1)").arg(num(e, "level")) : QString());
        }
        parts << "Entwicklungen: " + (evos.isEmpty() ? QString("keine") : evos.join(", "));
    }
    if (flag(a, "is_present") != flag(b, "is_present")) parts << (flag(b, "is_present") ? "jetzt im Spiel" : "nicht mehr im Spiel");
    if (num(a, "catch_rate") != num(b, "catch_rate"))
        parts << QString("Fangrate: %1 → %2").arg(num(a, "catch_rate")).arg(num(b, "catch_rate"));
    return parts.join("<br>");
}

bool writeSpoiler(const EditSession& s, const QString& path, const QString& seedText) {
    const DataTable* personal = s.table("personal");
    TrainerSmartData smart;
    smart.build(svGameData(s.mapping, personal && personal->loaded ? personal->work : json(), s.wazaTable, s.moveList));

    QString body;
    int trainerCount = 0;

    // Trainer
    if (const DataTable* t = s.table("trainers"); t && t->loaded && t->work != t->original) {
        QHash<QString, QString> html;
        QHash<QString, int> counts;
        const json& values = t->work["values"];
        for (int i = 0; i < static_cast<int>(values.size()); i++) {
            const json& tr = values[i];
            if (teamSize(tr) == 0) continue;
            // Nach "Original bearbeiten" bzw. bei einer geoeffneten Mod nur die geaenderten Trainer zeigen
            if (!s.randomized && tr == t->original["values"][i]) continue;
            const QPair<QString, QString> label = s.trainerLabels.value(i, {"Sonstige Trainer", "Trainer"});
            const QString trid = str(tr, "trid");
            QString search = label.second + " " + trid;
            QString cards;
            int highest = 0;
            for (int k = 1; k <= 6; k++) {
                const json& poke = tr["poke" + std::to_string(k)];
                if (isEmptyMon(poke)) continue;
                highest = std::max(highest, num(poke, "level"));
                cards += monCard(s, smart, poke, flag(tr, "changeGem"), search);
            }
            QString sub = QString(str(tr, "battleType") == "_2vs2" ? "Doppelkampf" : "Einzelkampf") + " · max. Lv. " +
                          QString::number(highest) + " · <code>" + esc(trid) + "</code> · #" + QString::number(i);
            if (tr != t->original["values"][i] && (s.randomized ? changedSinceStart(t, "values", i) : true)) {
                sub += s.randomized ? " · von Hand bearbeitet" : " · geändert";
            }
            html[label.first] += "<article class=\"trainer\" data-search=\"" + esc(search.toLower()) + "\"><header><h3>" +
                                 esc(label.second) + "</h3><div class=\"sub\">" + sub + "</div></header><div class=\"team\">" +
                                 cards + "</div></article>";
            counts[label.first]++;
            trainerCount++;
        }
        QStringList order = s.categoryOrder;
        for (auto it = html.constBegin(); it != html.constEnd(); ++it) {
            if (!order.contains(it.key())) order << it.key();
        }
        for (const QString& category : order) {
            if (!html.contains(category)) continue;
            body += sectionHtml(category, QString("%1 Trainer").arg(counts[category]), html[category], false);
        }
    }

    // Starter & Geschenke
    if (const DataTable* t = s.table("starters"); t && t->loaded && t->work != t->original) {
        int count = 0;
        const QString rows = encounterRows(s, t, "pokeData", "label", false, count);
        body += sectionHtml("Starter & Geschenke", QString("%1 Pokémon").arg(count),
                            encTable("<th>Ereignis</th><th>Pokémon</th><th>Level</th>", rows), true);
    }

    // Kaempfe und feste Begegnungen (nur geaenderte)
    if (const DataTable* t = s.table("bosses"); t && t->loaded && t->work != t->original) {
        int count = 0;
        const QString rows = encounterRows(s, t, "pokeData", "label", true, count);
        body += sectionHtml("Besondere Kämpfe", QString("%1 geändert").arg(count),
                            encTable("<th>Kampf</th><th>Pokémon</th><th>Level</th>", rows), false);
    }
    if (const DataTable* t = s.table("fixed"); t && t->loaded && t->work != t->original) {
        int count = 0;
        const QString rows = encounterRows(s, t, "pokeDataSymbol", "tableKey", true, count);
        body += sectionHtml("Feste Begegnungen", QString("%1 geändert").arg(count),
                            encTable("<th>Stelle</th><th>Pokémon</th><th>Level</th>", rows), false);
    }

    // Wilde Pokemon (je Pokemon: Level, Lebensraeume, Edition)
    for (const WildInfo& info : kWild) {
        const DataTable* t = s.table(info.key);
        if (t == nullptr || !t->loaded) continue;
        QString rows;
        int count = 0;
        for (const json& r : t->work["values"]) {
            const QString name = s.pokemonName(s.natdexOfDev(str(r, "devid")), num(r, "formno"));
            QStringList biomes;
            for (int b = 1; b <= 4; b++) {
                const int bi = kBiomes.indexOf(str(r, ("biome" + std::to_string(b)).c_str()));
                if (bi >= 0 && !biomes.contains(kBiomeText[bi])) biomes << kBiomeText[bi];
            }
            QString edition;
            const bool a = flag(r["versiontable"], "A"), b = flag(r["versiontable"], "B");
            if (a != b) edition = a ? "nur Karmesin" : "nur Purpur";
            QStringList times;
            const json& tt = r["timetable"];
            if (!(flag(tt, "morning") && flag(tt, "noon") && flag(tt, "evening") && flag(tt, "night"))) {
                if (flag(tt, "morning")) times << "morgens";
                if (flag(tt, "noon")) times << "mittags";
                if (flag(tt, "evening")) times << "abends";
                if (flag(tt, "night")) times << "nachts";
            }
            count++;
            rows += "<tr data-search=\"" + esc((name + " " + biomes.join(" ")).toLower()) + "\"><td class=\"place\">" +
                    esc(name) + "</td><td class=\"num\">Lv. " + QString::number(num(r, "minlevel")) + "–" +
                    QString::number(num(r, "maxlevel")) + "</td><td>" + esc(biomes.join(", ")) + "</td><td class=\"hint\">" +
                    esc((edition.isEmpty() || times.isEmpty()) ? edition + times.join(", ") : edition + " · " + times.join(", ")) +
                    "</td></tr>";
        }
        body += sectionHtml(QString("Wilde Pokémon · ") + info.region, QString("%1 Pokémon").arg(count),
                            encTable("<th>Pokémon</th><th>Level</th><th>Lebensräume</th><th></th>", rows), false);
    }

    // Tera-Raids
    for (const RaidRegion& region : kRaidRegions) {
        QString raidBody;
        int count = 0;
        for (int stars = 1; stars <= 6; stars++) {
            const DataTable* t = s.table("raid:" + raidName(region.prefix, stars));
            if (t == nullptr || !t->loaded || t->work == t->original) continue;
            QString cards;
            QString search = QString("raid tera %1 %2★").arg(region.region).arg(stars);
            int tableCount = 0;
            for (int r = 0; r < static_cast<int>(t->work["values"].size()); r++) {
                const json& raid = t->work["values"][r];
                if (!s.randomized && raid == t->original["values"][r]) continue;
                const json& info = raid["raidEnemyInfo"];
                QString card = monCard(s, smart, info["bossPokePara"], false, search);
                const QString romVer = str(info, "romVer");
                if (romVer == "TYPE_A" || romVer == "TYPE_B") {
                    card.insert(card.lastIndexOf("</div>"), "<div class=\"meta\">nur " +
                                QString(romVer == "TYPE_A" ? "Karmesin" : "Purpur") + "</div>");
                }
                cards += card;
                tableCount++;
            }
            count += tableCount;
            raidBody += "<article class=\"trainer\" data-search=\"" + esc(search.toLower()) + "\"><header><h3>" +
                        QString(stars, QChar(0x2605)) + " Tera-Raids</h3><div class=\"sub\">" + esc(region.region) + " · " +
                        QString::number(tableCount) + " Pokémon</div></header><div class=\"team\">" + cards + "</div></article>";
        }
        if (count > 0) {
            body += sectionHtml(QString("Tera-Raids · ") + region.region, QString("%1 Raid-Pokémon").arg(count), raidBody, false);
        }
    }

    // Items (nur geaenderte Eintraege)
    {
        QString rows;
        int count = 0;
        auto addRow = [&](const QString& place, const QString& before, const QString& after) {
            count++;
            rows += "<tr data-search=\"" + esc((place + " " + before + " " + after).toLower()) + "\"><td class=\"place\">" +
                    esc(place) + "</td><td>" + esc(before) + " <span class=\"arrow\">→</span> <b>" + esc(after) +
                    "</b></td></tr>";
        };
        for (const HiddenInfo& h : kHidden) {
            const DataTable* t = s.table(h.key);
            if (t == nullptr || !t->loaded) continue;
            for (int i = 0; i < static_cast<int>(t->work["values"].size()); i++) {
                const json& now = t->work["values"][i];
                const json& before = t->original["values"][i];
                for (int k = 1; k <= 10; k++) {
                    const std::string key = "item_" + std::to_string(k);
                    if (now[key] == before[key]) continue;
                    addRow(QString("Versteckt · %1 · Tabelle %2").arg(h.title, str(now, "table_Id")),
                           s.itemName(str(before[key], "itemId")) + QString(" ×%1").arg(num(before[key], "dropCount")),
                           s.itemName(str(now[key], "itemId")) + QString(" ×%1").arg(num(now[key], "dropCount")));
                }
            }
        }
        if (const DataTable* t = s.table("drops"); t && t->loaded) {
            for (int i = 0; i < static_cast<int>(t->work["values"].size()); i++) {
                const json& now = t->work["values"][i]["item1"];
                const json& before = t->original["values"][i]["item1"];
                if (now == before) continue;
                addRow("Material · " + s.pokemonName(s.natdexOfDev(str(t->work["values"][i], "devid"))),
                       s.itemName(str(before, "itemid")), s.itemName(str(now, "itemid")));
            }
        }
        if (const DataTable* t = s.table("monohiroi"); t && t->loaded) {
            for (int i = 0; i < static_cast<int>(t->work["values"].size()); i++) {
                const json& now = t->work["values"][i];
                const json& before = t->original["values"][i];
                for (auto it = now.begin(); it != now.end(); ++it) {
                    if (!it.value().is_object() || it.value() == before[it.key()]) continue;
                    addRow(QString("Mitnahme · Tabelle %1").arg(i + 1), s.itemName(str(before[it.key()], "itemId")),
                           s.itemName(str(it.value(), "itemId")) + QString(" (%1 %)").arg(num(it.value(), "rate")));
                }
            }
        }
        if (const DataTable* t = s.table("rummaging"); t && t->loaded) {
            for (int i = 0; i < static_cast<int>(t->work["values"].size()); i++) {
                const json& now = t->work["values"][i];
                const json& before = t->original["values"][i];
                for (int k = 0; k < 5; k++) {
                    const std::string key = "Item0" + std::to_string(k);
                    if (now[key] == before[key]) continue;
                    addRow("Let's-Go-Suchen · " + str(now, "Category") + " · " + str(now, "Pattern"),
                           s.itemName(str(before, key.c_str())), s.itemName(str(now, key.c_str())));
                }
            }
        }
        if (count > 0) {
            body += sectionHtml("Items", QString("%1 geändert").arg(count),
                                encTable("<th>Stelle</th><th>Item</th>", rows), false);
        }
    }

    // Pokemon-Daten
    if (personal && personal->loaded && personal->work != personal->original) {
        QString rows;
        int count = 0;
        const json& entries = personal->work["entry"];
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            const json& now = entries[i];
            const json& before = personal->original["entry"][i];
            if (now == before) continue;
            const int natdex = s.natdexOfDevid(num(now["species"], "species"));
            if (natdex <= 0) continue;
            const QString changes = personalChanges(s, before, now);
            if (changes.isEmpty()) continue;
            count++;
            const QString name = s.pokemonName(natdex, num(now["species"], "form"));
            rows += "<tr data-search=\"" + esc(name.toLower()) + "\"><td class=\"place\">" + esc(name) + "</td><td>" +
                    changes + "</td></tr>";
        }
        if (count > 0) {
            body += sectionHtml("Pokémon-Daten", QString("%1 Pokémon geändert").arg(count),
                                encTable("<th>Pokémon</th><th>Änderungen</th>", rows), false);
        }
    }

    QString info = "Seed: " + esc(seedText) + " · erstellt am " +
                   esc(QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm"));
    if (trainerCount > 0) info += " · " + QString::number(trainerCount) + " Trainer";
    if (body.isEmpty()) body = "<p class=\"hint\">Gegenüber dem Original wurde nichts geändert.</p>";
    return spoiler::write(path, spoiler::head("Spoiler-Log · Karmesin/Purpur Randomizer", "Spoiler-Log", info) + body +
                                    spoiler::foot());
}

// ------------------------------------------------------------- Szenen

bool sameModel(const json& a, const json& b) {
    return str(a, "devId") == str(b, "devId") && num(a, "formId") == num(b, "formId") && str(a, "sex") == str(b, "sex") &&
           str(a, "rareType") == str(b, "rareType");
}

void addModel(const EditSession& s, const json& poke, QVector<int>& devIds, QVector<int>& forms, QVector<int>& genders,
              QVector<bool>& rares) {
    devIds << s.devidOfNatdex(s.natdexOfDev(str(poke, "devId")));
    forms << num(poke, "formId");
    genders << (str(poke, "sex") == "FEMALE" ? 1 : 0);
    rares << (str(poke, "rareType") == "RARE");
}

void patchStarterScenes(const EditSession& s, SVShared& tools) {
    const DataTable* t = s.table("starters");
    if (t == nullptr || !t->loaded) return;
    const json& now = t->work["values"];
    const json& start = t->start["values"];
    const QDir base = tools.qBaseDir;

    bool starters = false;
    for (int i = 0; i < 3; i++) starters |= !sameModel(now[i]["pokeData"], start[i]["pokeData"]);
    if (starters) {
        QVector<int> devIds, forms, genders;
        QVector<bool> rares;
        for (int i : {1, 2, 0}) addModel(s, now[i]["pokeData"], devIds, forms, genders, rares);
        const QList<QPair<QString, QString>> files = {
            {base.filePath("SV_SCENES/trscn.fbs"), base.filePath("SV_SCENES/SV_STARTERS_SCENES/d030")},
            {base.filePath("SV_SCENES/trsog.fbs"), base.filePath("SV_SCENES/SV_STARTERS_SCENES/common_0060_always")},
            {base.filePath("SV_SCENES/trsog.fbs"), base.filePath("SV_SCENES/SV_STARTERS_SCENES/common_0060_main")},
            {base.filePath("SV_SCENES/trsog.fbs"), base.filePath("SV_SCENES/SV_STARTERS_SCENES/common_0070_always")},
            {base.filePath("SV_SCENES/trsog.fbs"), base.filePath("SV_SCENES/SV_STARTERS_SCENES/common_0088_always")},
            {base.filePath("SV_SCENES/trsog.fbs"), base.filePath("SV_SCENES/SV_STARTERS_SCENES/common_0090_main")},
        };
        const QList<QString> outputs = {
            "world/scene/parts/demo/ev/d030_/",
            "world/scene/parts/event/event_scenario/main_scenario/common_0060_/",
            "world/scene/parts/event/event_scenario/main_scenario/common_0060_/",
            "world/scene/parts/event/event_scenario/main_scenario/common_0070_/",
            "world/scene/parts/event/event_scenario/main_scenario/common_0088_/",
            "world/scene/parts/event/event_scenario/main_scenario/common_0090_/"};
        tools.patchScenes(files, outputs, devIds, forms, genders, rares);
    }

    if (now.size() > 8 && !sameModel(now[8]["pokeData"], start[8]["pokeData"])) {
        QVector<int> devIds, forms, genders;
        QVector<bool> rares;
        addModel(s, now[8]["pokeData"], devIds, forms, genders, rares);
        const QString fbs = base.filePath("SV_SCENES/trsog.fbs");
        const QList<QPair<QString, QString>> files = {
            {fbs, base.filePath("SV_SCENES/SV_Kitakami/Growlithe/s1_side02_0000_always")},
            {fbs, base.filePath("SV_SCENES/SV_Kitakami/Growlithe/s1_side02_0030_always")},
            {fbs, base.filePath("SV_SCENES/SV_Kitakami/Growlithe/s1_side02_0060_pre_start")},
            {fbs, base.filePath("SV_SCENES/SV_Kitakami/Growlithe/s2_side02_0000_pre_start")},
            {fbs, base.filePath("SV_SCENES/SV_Kitakami/Growlithe/s2_side02_0005_pre_start")},
            {fbs, base.filePath("SV_SCENES/SV_Kitakami/Growlithe/s2_side02_0030_always")},
        };
        const QList<QString> outputs = {
            "world/scene/parts/event/event_scenario/sub_scenario/s1_side02_0000_/",
            "world/scene/parts/event/event_scenario/sub_scenario/s1_side02_0030_/",
            "world/scene/parts/event/event_scenario/sub_scenario/s1_side02_0060_/",
            "world/scene/parts/event/event_scenario/sub_scenario/s2_side02_0000_/",
            "world/scene/parts/event/event_scenario/sub_scenario/s2_side02_0005_/",
            "world/scene/parts/event/event_scenario/sub_scenario/s2_side02_0030_/"};
        tools.patchScenes(files, outputs, devIds, forms, genders, rares);
    }
}

void collectItems(EditSession& s, const json& j) {
    if (j.is_string()) {
        const std::string& text = j.get_ref<const std::string&>();
        if (text.rfind("ITEMID_", 0) == 0) s.itemId(qs(text));
    } else if (j.is_structured()) {
        for (const json& child : j) collectItems(s, child);
    }
}

} // namespace

// --------------------------------------------------------------- Session

DataTable* EditSession::table(const QString& key) {
    for (DataTable& t : tables) {
        if (t.key == key) return &t;
    }
    return nullptr;
}

const DataTable* EditSession::table(const QString& key) const {
    for (const DataTable& t : tables) {
        if (t.key == key) return &t;
    }
    return nullptr;
}

int EditSession::natdexOfDev(const QString& dev) const { return natdexByDev.value(dev, 0); }
QString EditSession::devOfNatdex(int natdex) const { return devByNatdex.value(natdex); }
int EditSession::natdexOfDevid(int devid) const { return natdexByDevid.value(devid, 0); }
int EditSession::devidOfNatdex(int natdex) const { return devidByNatdex.value(natdex, 0); }
int EditSession::moveId(const QString& dev) const { return moveIdByDev.value(dev, 0); }
QString EditSession::moveDev(int id) const { return id <= 0 ? QString("WAZA_NULL") : moveDevById.value(id); }

int EditSession::itemId(const QString& dev) {
    auto it = itemIndex.constFind(dev);
    if (it != itemIndex.constEnd()) return it.value();
    itemDevs << dev;
    itemIndex.insert(dev, itemDevs.size() - 1);
    return itemDevs.size() - 1;
}

int EditSession::itemIdConst(const QString& dev) const { return itemIndex.value(dev, 0); }
QString EditSession::itemDev(int index) const {
    return index > 0 && index < itemDevs.size() ? itemDevs[index] : QString("ITEMID_NONE");
}

QString EditSession::pokemonName(int natdex, int form) const {
    const std::string key = std::to_string(natdex);
    QString name = names["pokemon"].contains(key) ? qs(names["pokemon"][key].get<std::string>())
                                                  : QString("#%1").arg(natdex);
    if (form > 0 && natdex > 0 && natdex < static_cast<int>(mapping["pokemons"].size())) {
        const json& forms = mapping["pokemons"][natdex]["forms"];
        std::string raw = form < static_cast<int>(forms.size()) ? forms[form].value("formName", std::string()) : std::string();
        QString formText = raw.empty() ? QString("Form %1").arg(form)
                                       : (names["forms"].contains(raw) ? qs(names["forms"][raw].get<std::string>()) : qs(raw));
        name += " (" + formText + ")";
    }
    return name;
}

QString EditSession::moveName(int id) const {
    const std::string key = std::to_string(id);
    return names["moves"].contains(key) ? qs(names["moves"][key].get<std::string>()) : QString("Attacke %1").arg(id);
}

QString EditSession::itemName(const QString& dev) const {
    if (dev.isEmpty() || dev == "ITEMID_NONE") return "—";
    const std::string key = dev.toStdString();
    return names["items"].contains(key) ? qs(names["items"][key].get<std::string>()) : dev;
}

QString EditSession::abilityName(int id) const {
    const std::string key = std::to_string(id);
    if (names.contains("abilities") && names["abilities"].contains(key)) return qs(names["abilities"][key].get<std::string>());
    return id <= 0 ? QString("—") : QString("Fähigkeit %1").arg(id);
}

QString EditSession::typeName(int type) const {
    return type >= 0 && type < static_cast<int>(names["types"].size()) ? qs(names["types"][type].get<std::string>())
                                                                        : QString("?");
}

const json* EditSession::personal(int natdex, int form) const {
    const DataTable* t = table("personal");
    if (t == nullptr || !t->loaded) return nullptr;
    const qint64 key = (static_cast<qint64>(devidOfNatdex(natdex)) << 16) | form;
    auto it = personalIndex.constFind(key);
    if (it == personalIndex.constEnd()) {
        it = personalIndex.constFind(static_cast<qint64>(devidOfNatdex(natdex)) << 16);
        if (it == personalIndex.constEnd()) return nullptr;
    }
    return &t->work["entry"][it.value()];
}

bool openSession(EditSession& s, const QString& baseRomfs, const svTrainers& trainers, QString* error) {
    auto fail = [error](const QString& m) { if (error) *error = m; return false; };
    const QDir base(QDir::current().filePath("SV_FLATBUFFERS"));
    bool ok = true;
    s.mapping = readJson(base.filePath("pokemon_mapping.json"), &ok);
    if (!ok) return fail("SV_FLATBUFFERS/pokemon_mapping.json fehlt oder ist beschädigt.");
    s.names = readJson(base.filePath("german_names.json"), &ok);
    if (!ok) return fail("SV_FLATBUFFERS/german_names.json fehlt oder ist beschädigt.");
    s.moveList = readJson(base.filePath("SV_PERSONAL/sorted_move_list.json"), &ok);
    if (!ok) return fail("SV_FLATBUFFERS/SV_PERSONAL/sorted_move_list.json fehlt oder ist beschädigt.");
    s.wazaTable = readJson(base.filePath("SV_PERSONAL/waza_array_clean.json"), &ok);
    if (!ok) return fail("SV_FLATBUFFERS/SV_PERSONAL/waza_array_clean.json fehlt oder ist beschädigt.");

    // Zuordnungen
    for (const json& p : s.mapping["pokemons"]) {
        const int natdex = p.value("natdex", 0);
        const int devid = p.value("devid", 0);
        const QString dev = qs(p.value("devName", std::string()));
        if (natdex <= 0) continue;
        s.natdexByDev.insert(dev, natdex);
        s.devByNatdex.insert(natdex, dev);
        s.natdexByDevid.insert(devid, natdex);
        s.devidByNatdex.insert(natdex, devid);
    }
    for (const json& m : s.moveList["moves"]) {
        const QString dev = qs(m.value("devName", std::string()));
        const int id = m.value("id", 0);
        if (id <= 0 || dev.isEmpty()) continue;
        s.moveIdByDev.insert(dev, id);
        s.moveDevById.insert(id, dev);
    }
    s.itemDevs.clear();
    s.itemIndex.clear();
    s.itemId("ITEMID_NONE");
    for (auto it = s.names["items"].begin(); it != s.names["items"].end(); ++it) s.itemId(qs(it.key()));

    // Dateien des Randomizer-Ergebnisses bzw. der Mod
    s.baseFiles.clear();
    if (!baseRomfs.isEmpty()) {
        const QDir root(baseRomfs);
        if (!root.exists()) return fail("Der Ordner „romfs“ fehlt:\n" + QDir::toNativeSeparators(baseRomfs));
        QDirIterator it(root.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString file = it.next();
            const QString rel = root.relativeFilePath(file);
            if (rel == "arc/data.trpfd") continue; // wird beim Export neu erzeugt
            QFile f(file);
            if (!f.open(QIODevice::ReadOnly)) return fail("Die Datei konnte nicht gelesen werden:\n" + QDir::toNativeSeparators(file));
            s.baseFiles.insert(rel, f.readAll());
        }
    }

    // Tabellen
    s.tables.clear();
    auto add = [&s](const QString& key, const QString& title, const QString& stem, bool clean, const QString& schema,
                    const QString& outDir) {
        DataTable t;
        t.key = key;
        t.title = title;
        t.jsonPath = stem + ".json";
        t.cleanPath = clean ? stem + "_clean.json" : QString();
        t.schema = schema;
        t.outDir = outDir;
        t.binName = QFileInfo(stem).fileName() + ".bin";
        s.tables.push_back(t);
    };
    add("trainers", "Trainer", "SV_TRAINERS/trdata_array", true, "SV_TRAINERS/trdata_array.bfbs",
        "world/data/trainer/trdata/");
    add("starters", "Starter und Geschenke", "SV_STARTERS_FLATBUFFERS/eventAddPokemon_array", true,
        "SV_STARTERS_FLATBUFFERS/eventAddPokemon_array.bfbs", "world/data/event/event_add_pokemon/eventAddPokemon/");
    add("bosses", "Besondere Kämpfe", "SV_SCENES/eventBattlePokemon_array", true, "SV_SCENES/eventBattlePokemon_array.bfbs",
        "world/data/battle/eventBattlePokemon/");
    add("fixed", "Feste Begegnungen", "SV_WILDS/fixed_symbol_table_array", true, "SV_WILDS/fixed_symbol_table_array.bfbs",
        "world/data/field/fixed_symbol/fixed_symbol_table/");
    add("wild", "Wilde Pokémon (Paldea)", "SV_WILDS/pokedata_array", false, "SV_WILDS/pokedata_array.bfbs",
        "world/data/encount/pokedata/pokedata/");
    add("wild_su1", "Wilde Pokémon (Kitakami)", "SV_WILDS/pokedata_su1_array", false, "SV_WILDS/pokedata_su1_array.bfbs",
        "world/data/encount/pokedata/pokedata_su1/");
    add("wild_su2", "Wilde Pokémon (Blaubeer-Akademie)", "SV_WILDS/pokedata_su2_array", false,
        "SV_WILDS/pokedata_su2_array.bfbs", "world/data/encount/pokedata/pokedata_su2/");
    for (const RaidRegion& region : kRaidRegions) {
        for (int stars = 1; stars <= 6; stars++) {
            const QString name = raidName(region.prefix, stars);
            add("raid:" + name, QString("Tera-Raids %1 %2★").arg(region.region).arg(stars), "SV_RAIDS/" + name + "_array",
                true, "SV_RAIDS/" + name + "_array.bfbs", "world/data/raid/" + name + "/");
        }
    }
    add("hidden", "Versteckte Items (Paldea)", "SV_ITEMS/hiddenItemDataTable_array", true,
        "SV_ITEMS/hiddenItemDataTable_array.bfbs", "world/data/item/hiddenItemDataTable/");
    add("hidden_su1", "Versteckte Items (Kitakami)", "SV_ITEMS/hiddenItemDataTable_su1_array", true,
        "SV_ITEMS/hiddenItemDataTable_su1_array.bfbs", "world/data/item/hiddenItemDataTable_su1/");
    add("hidden_su2", "Versteckte Items (Blaubeer-Akademie)", "SV_ITEMS/hiddenItemDataTable_su2_array", true,
        "SV_ITEMS/hiddenItemDataTable_su2_array.bfbs", "world/data/item/hiddenItemDataTable_su2/");
    add("hidden_lc", "Versteckte Items (lc)", "SV_ITEMS/hiddenItemDataTable_lc_array", true,
        "SV_ITEMS/hiddenItemDataTable_lc_array.bfbs", "world/data/item/hiddenItemDataTable_lc/");
    add("drops", "Materialien", "SV_ITEMS/dropitemdata_array", true, "SV_ITEMS/dropitemdata_array.bfbs",
        "world/data/item/dropitemdata/");
    add("monohiroi", "Mitnahme", "SV_ITEMS/monohiroiItemData_array", true, "SV_ITEMS/monohiroiItemData_array.bfbs",
        "world/data/item/monohiroilItemData/");
    add("rummaging", "Let's-Go-Suchen", "SV_ITEMS/rummagingItemDataTable_array", true,
        "SV_ITEMS/rummagingItemDataTable_array.bfbs", "world/data/item/rummagingItemDataTable/");
    add("personal", "Pokémon-Daten", "SV_PERSONAL/personal_array", true, "SV_PERSONAL/personal_array.fbs", "avalon/data/");

    for (DataTable& t : s.tables) {
        if (!t.cleanPath.isEmpty()) {
            t.original = readJson(base.filePath(t.cleanPath), &ok);
            if (!ok) return fail("Die Spieldaten fehlen oder sind beschädigt: SV_FLATBUFFERS/" + t.cleanPath);
            t.hasOriginal = true;
        }
        auto file = s.baseFiles.constFind(t.binPath());
        if (file != s.baseFiles.constEnd()) {
            QString why;
            if (!binToJson(base.filePath(t.schema), file.value(), t.start, &why)) {
                return fail(t.title + " (" + t.binPath() + ") konnte nicht gelesen werden.\n" + why);
            }
        } else if (t.hasOriginal) {
            t.start = t.original;
        } else {
            continue; // nicht vorhanden (wilde Pokemon ohne Randomisierung)
        }
        if (!t.hasOriginal) t.original = t.start;
        const char* root = t.key == "personal" ? "entry" : "values";
        if (!t.start.contains(root) || !t.start[root].is_array() || !t.original.contains(root) ||
            t.start[root].size() != t.original[root].size()) {
            if (t.hasOriginal) return fail(t.title + ": Die Datei passt nicht zu den Spieldaten (" + t.binPath() + ").");
        }
        t.work = t.start;
        t.loaded = true;
        collectItems(s, t.work);
    }

    s.personalIndex.clear();
    if (const DataTable* p = s.table("personal"); p && p->loaded) {
        const json& entries = p->work["entry"];
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            const json& sp = entries[i]["species"];
            const qint64 key = (static_cast<qint64>(num(sp, "species")) << 16) | num(sp, "form");
            if (!s.personalIndex.contains(key)) s.personalIndex.insert(key, i);
        }
    }

    if (const DataTable* t = s.table("trainers"); t && t->loaded) s.trainerLabels = trainers.spoilerLabels(t->original["values"]);
    s.categoryOrder = svTrainers::spoilerCategoryOrder();
    return true;
}

QList<edit::Section> buildSections(EditSession& s) {
    return {trainerSection(s), encounterSection(s), wildSection(s), raidSection(s), itemSection(s), dataSection(s)};
}

edit::Catalog buildCatalog(const EditSession& s) {
    edit::Catalog c;
    for (int natdex = 1; natdex < static_cast<int>(s.mapping["pokemons"].size()); natdex++) {
        const json& p = s.mapping["pokemons"][natdex];
        if (p.value("natdex", 0) != natdex) continue;
        const json& forms = p["forms"];
        for (int f = 0; f < static_cast<int>(forms.size()); f++) {
            if (f > 0 && !forms[f].value("is_present", false)) continue;
            c.pokemon.append({edit::pokemonValue(natdex, forms[f].value("form", f)), s.pokemonName(natdex, forms[f].value("form", f))});
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
    for (auto it = s.moveDevById.constBegin(); it != s.moveDevById.constEnd(); ++it) moves.append({it.key(), s.moveName(it.key())});
    c.moves = sorted(moves, "(keine)");
    QList<QPair<int, QString>> items;
    for (int i = 1; i < s.itemDevs.size(); i++) {
        const QString name = s.itemName(s.itemDevs[i]);
        if (name.isEmpty() || name == "None" || name.startsWith("ITEMID_")) continue;
        items.append({i, name});
    }
    c.items = sorted(items, "(kein Item)");
    QList<QPair<int, QString>> abilities;
    if (s.names.contains("abilities")) {
        for (auto it = s.names["abilities"].begin(); it != s.names["abilities"].end(); ++it) {
            abilities.append({QString::fromStdString(it.key()).toInt(), qs(it.value().get<std::string>())});
        }
    }
    c.abilities = sorted(abilities, "(keine)");
    for (int t = 0; t < 18; t++) c.types.append({t, s.typeName(t)});
    const QStringList natures = edit::natureNames();
    for (int n = 0; n < natures.size(); n++) c.natures.append({n, natures[n]});
    return c;
}

bool sessionHasChanges(const EditSession& s) {
    for (const DataTable& t : s.tables) {
        if (t.loaded && t.work != t.start) return true;
    }
    return false;
}

int changedTrainerCount(const EditSession& s) {
    const DataTable* t = s.table("trainers");
    if (t == nullptr || !t->loaded) return 0;
    int count = 0;
    for (size_t i = 0; i < t->work["values"].size(); i++) count += t->work["values"][i] != t->original["values"][i];
    return count;
}

bool exportSession(EditSession& s, SVShared& tools, const QString& outFolder, bool autoPatch, const QString& spoilerPath,
                   const QString& seedText, QString* error) {
    auto fail = [error](const QString& m) { if (error) *error = m; return false; };
    const QString work = QDir::current().filePath("output");
    QDir(work).removeRecursively();

    // 1. Unveraenderte Dateien des Ergebnisses bzw. der Mod
    for (auto it = s.baseFiles.constBegin(); it != s.baseFiles.constEnd(); ++it) {
        const QString path = work + "/romfs/" + it.key();
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(it.value()) != it.value().size()) {
            return fail("Die Datei konnte nicht geschrieben werden:\n" + QDir::toNativeSeparators(path));
        }
    }

    // 2. Geaenderte Tabellen neu erzeugen
    for (DataTable& t : s.tables) {
        if (!t.loaded || t.work == t.start) continue;
        const QString target = work + "/romfs/" + t.binPath();
        QFile::remove(target);
        tools.closeFileAndDelete(t.jsonPath, t.schema, t.outDir, t.work, true);
        if (!QFileInfo::exists(target)) return fail(t.title + " konnte nicht geschrieben werden (flatc).");
    }

    // 3. Modelle in den Zwischensequenzen (Starter, Fukano)
    patchStarterScenes(s, tools);

    // 4. Dateiliste des Spiels
    if (autoPatch) {
        tools.patchFileDescriptor();
        tools.generateBinary(tools.qBaseDir.filePath("SV_DATA_FLATBUFFERS/data.fbs").toStdString(),
                             tools.qBaseDir.filePath("SV_DATA_FLATBUFFERS/data.json").toStdString(), "arc/", true);
        QDir().remove(tools.qBaseDir.filePath("SV_DATA_FLATBUFFERS/data.json"));
        if (!QFileInfo::exists(work + "/romfs/arc/data.trpfd")) return fail("Die Dateiliste (data.trpfd) konnte nicht erzeugt werden.");
    }

    // 5. In den Zielordner verschieben
    if (QFileInfo::exists(outFolder) && !QDir(outFolder).removeRecursively()) {
        return fail("Der alte Ordner konnte nicht gelöscht werden (ist eine Datei darin noch geöffnet?):\n" +
                    QDir::toNativeSeparators(outFolder));
    }
    QDir().mkpath(QFileInfo(outFolder).absolutePath());
    if (!QDir(work).exists()) QDir().mkpath(work);
    if (!QDir().rename(work, outFolder)) {
        return fail("Das Ergebnis konnte nicht verschoben werden nach:\n" + QDir::toNativeSeparators(outFolder));
    }

    // 6. Spoiler-Log
    if (!spoilerPath.isEmpty() && !writeSpoiler(s, spoilerPath, seedText)) {
        return fail("Das Spoiler-Log konnte nicht geschrieben werden:\n" + QDir::toNativeSeparators(spoilerPath));
    }
    return true;
}

} // namespace svedit
