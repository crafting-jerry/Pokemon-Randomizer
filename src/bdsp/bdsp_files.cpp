// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Spieldaten aus dem Dump
// ---------------------------------------------------------------------------

#include "headers/bdsp/bdsp_files.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QtEndian>

namespace bdsp {

// ------------------------------------------------------------- Dateien

QByteArray readFile(const QString& path, bool* ok) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        return QByteArray();
    }
    if (ok) *ok = true;
    return file.readAll();
}

bool writeFile(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return file.write(data) == data.size();
}

// ------------------------------------------------------------- Dump-Pruefung

QString versionName(Version version) {
    switch (version) {
    case Version::Diamond: return "Pokémon Strahlender Diamant";
    case Version::Pearl: return "Pokémon Leuchtende Perle";
    default: return "Unbekannt";
    }
}

QString titleId(Version version) {
    switch (version) {
    case Version::Diamond: return "0100000011D90000";
    case Version::Pearl: return "010018E011D92000";
    default: return QString();
    }
}

DumpCheck checkDump(const QString& romfs, const QString& exefs) {
    DumpCheck result;
    if (romfs.isEmpty()) {
        result.errors << "Bitte gib den RomFS-Ordner deines Dumps an.";
    } else if (!QDir(romfs).exists()) {
        result.errors << "Der RomFS-Ordner existiert nicht.";
    } else if (!QDir(romfs + "/" + path::Assets).exists()) {
        result.errors << "Im RomFS-Ordner fehlt „Data/StreamingAssets/AssetAssistant“. Ist das wirklich der Ordner "
                         "„romfs“ von Strahlender Diamant oder Leuchtende Perle?";
    } else {
        const QList<QPair<QString, QString>> required = {
            {path::Personal, "Pml/personal_masterdatas"},
            {path::Master, "Dpr/masterdatas"},
            {path::MessageCommon, "Message/common_msbt"},
        };
        for (const auto& r : required) {
            if (!QFileInfo::exists(romfs + "/" + r.first)) {
                result.errors << "Im RomFS fehlt die Datei „" + r.second + "“.";
            }
        }
        if (!QFileInfo::exists(romfs + "/" + path::MessageGerman) && !QFileInfo::exists(romfs + "/" + path::MessageEnglish)) {
            result.errors << "Im RomFS fehlen die Spieltexte (Message/german).";
        }
    }

    if (exefs.isEmpty()) {
        result.warnings << "Ohne ExeFS kann nicht erkannt werden, ob es Strahlender Diamant oder Leuchtende Perle ist.";
    } else {
        bool ok = false;
        const QByteArray npdm = readFile(exefs + "/main.npdm", &ok);
        const int aci = npdm.indexOf("ACI0");
        if (!ok) {
            result.errors << "Im ExeFS-Ordner fehlt die Datei „main.npdm“.";
        } else if (aci < 0 || aci + 0x18 > npdm.size()) {
            result.errors << "Die Datei „main.npdm“ ist ungültig.";
        } else {
            const quint64 id = qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(npdm.constData() + aci + 0x10));
            if (id == 0x0100000011D90000ULL) {
                result.version = Version::Diamond;
            } else if (id == 0x010018E011D92000ULL) {
                result.version = Version::Pearl;
            } else {
                result.errors << QString("Die ExeFS gehört zu einem anderen Spiel (Title-ID %1).")
                                     .arg(id, 16, 16, QChar('0')).toUpper();
            }
        }
    }
    result.ok = result.errors.isEmpty();
    return result;
}

// ------------------------------------------------------------- BundleData

bool BundleData::load(const QString& romfs, const QString& relPath, const QStringList& tables, QString* error) {
    rel = relPath;
    values.clear();
    objectIndex.clear();
    bool ok = false;
    raw = readFile(romfs + "/" + relPath, &ok);
    if (!ok) {
        if (error) *error = "Datei nicht gefunden: " + relPath;
        return false;
    }
    bundle = std::make_shared<UnityBundle>();
    QString err;
    if (!bundle->load(raw, &err)) {
        if (error) *error = relPath + ": " + err;
        return false;
    }
    SerializedFile* file = bundle->serialized(&err);
    if (!file) {
        if (error) *error = relPath + ": " + err;
        return false;
    }
    for (const QString& name : tables) {
        const int index = file->findObject(name);
        if (index < 0) {
            if (error) *error = QString("%1: Tabelle „%2“ nicht gefunden.").arg(relPath, name);
            return false;
        }
        UValue v = file->read(index);
        if (!v.isValid()) {
            if (error) *error = QString("%1: Tabelle „%2“ konnte nicht gelesen werden.").arg(relPath, name);
            return false;
        }
        objectIndex.insert(name, index);
        values.insert(name, v);
    }
    return true;
}

bool BundleData::loadWhere(const QString& romfs, const QString& relPath,
                           const std::function<bool(const UValue&)>& keep, QString* error) {
    rel = relPath;
    values.clear();
    objectIndex.clear();
    bool ok = false;
    raw = readFile(romfs + "/" + relPath, &ok);
    if (!ok) {
        if (error) *error = "Datei nicht gefunden: " + relPath;
        return false;
    }
    bundle = std::make_shared<UnityBundle>();
    QString err;
    if (!bundle->load(raw, &err)) {
        if (error) *error = relPath + ": " + err;
        return false;
    }
    SerializedFile* file = bundle->serialized(&err);
    if (!file) {
        if (error) *error = relPath + ": " + err;
        return false;
    }
    for (int i = 0; i < file->objectCount(); i++) {
        if (file->classId(i) != 114) continue;
        const QString name = file->objectName(i);
        if (name.isEmpty() || values.contains(name)) continue;
        UValue v = file->read(i);
        if (!v.isValid() || !keep(v)) continue;
        objectIndex.insert(name, i);
        values.insert(name, v);
    }
    return true;
}

const UValue& BundleData::table(const QString& name) const {
    static const UValue invalid;
    auto it = values.constFind(name);
    return it == values.constEnd() ? invalid : it.value();
}

QList<QPair<QString, UValue>> BundleData::readPrefixed(const QString& prefix) const {
    QList<QPair<QString, UValue>> result;
    if (!bundle) return result;
    SerializedFile* file = bundle->serialized();
    if (!file) return result;
    for (int i = 0; i < file->objectCount(); i++) {
        const QString name = file->objectName(i);
        if (name.startsWith(prefix)) {
            UValue v = file->read(i);
            if (v.isValid()) result.append(qMakePair(name, v));
        }
    }
    return result;
}

bool BundleData::save(const QString& outRomfs, const QStringList& changedTables, QString* error) const {
    UnityBundle out;
    QString err;
    if (!out.load(raw, &err)) {
        if (error) *error = rel + ": " + err;
        return false;
    }
    SerializedFile* file = out.serialized(&err);
    if (!file) {
        if (error) *error = rel + ": " + err;
        return false;
    }
    for (const QString& name : changedTables) {
        const int index = objectIndex.value(name, -1);
        if (index < 0) continue;
        file->write(index, values.value(name));
    }
    out.commit();
    if (!writeFile(outRomfs + "/" + rel, out.save())) {
        if (error) *error = "Die Datei konnte nicht geschrieben werden: " + outRomfs + "/" + rel;
        return false;
    }
    return true;
}

// ------------------------------------------------------------- Skripte

bool ScriptData::ensureLoaded(const QString& romfs, QString* error) {
    if (loaded) return true;
    QString err;
    if (!bundle.loadWhere(romfs, path::EvScript, [](const UValue& v) { return v.has("Scripts"); }, &err)) {
        if (error) *error = "Die Skripte (ev_script) konnten nicht gelesen werden:\n" + err;
        return false;
    }
    loaded = true;
    return true;
}

bool ScriptData::save(const QString& outRomfs, QString* error) const {
    if (!loaded || changed.isEmpty()) return true;
    QStringList names(changed.begin(), changed.end());
    names.sort();
    return bundle.save(outRomfs, names, error);
}

// ----------------------------------------------------------- Personal

namespace {
qint64 key(int species, int form) { return static_cast<qint64>(species) * 1000 + form; }
const char* const kStatFields[6] = {"basic_hp", "basic_atk", "basic_def", "basic_agi", "basic_spatk", "basic_spdef"};
const char* const kAbilityFields[3] = {"tokusei1", "tokusei2", "tokusei3"};
} // namespace

void PersonalTable::attach(UValue* r) {
    root = r;
    buildIndex();
}

int PersonalTable::count() const {
    return root ? static_cast<int>((*root)["Personal"].list().size()) : 0;
}

void PersonalTable::buildIndex() {
    forward.clear();
    reverse.clear();
    const int n = count();
    for (int i = 1; i < n; i++) {
        const int species = get(i, "monsno");
        if (species <= 0 || get(i, "id") != species) continue; // nur Grundformen
        forward.insert(key(species, 0), i);
        reverse.insert(i, qMakePair(species, 0));
        const int forms = get(i, "form_max");
        const int first = get(i, "form_index");
        for (int f = 1; f < forms; f++) {
            const int index = first + f - 1;
            if (index <= 0 || index >= n || get(index, "monsno") != species) continue;
            forward.insert(key(species, f), index);
            reverse.insert(index, qMakePair(species, f));
        }
    }
}

int PersonalTable::indexOf(int species, int form) const {
    return forward.value(key(species, form), -1);
}

int PersonalTable::get(int index, const char* field) const {
    if (!root || index < 0 || index >= count()) return 0;
    return static_cast<int>((*root)["Personal"].list()[static_cast<size_t>(index)][field].i);
}

void PersonalTable::set(int index, const char* field, int value) {
    if (!root || index < 0 || index >= count()) return;
    UValue& v = (*root)["Personal"].list()[static_cast<size_t>(index)][field];
    if (v.isValid()) v.i = value;
}

int PersonalTable::stat(int index, int which) const { return get(index, kStatFields[qBound(0, which, 5)]); }
void PersonalTable::setStat(int index, int which, int value) { set(index, kStatFields[qBound(0, which, 5)], value); }
int PersonalTable::ability(int index, int slot) const { return get(index, kAbilityFields[qBound(0, slot, 2)]); }
void PersonalTable::setAbility(int index, int slot, int value) { set(index, kAbilityFields[qBound(0, slot, 2)], value); }

bool PersonalTable::canLearnTM(int index, int machineNo) const {
    if (machineNo < 1 || machineNo > 128 || !root || index < 0 || index >= count()) return false;
    const int bit = machineNo - 1;
    const QByteArray field = "machine" + QByteArray::number(bit / 32 + 1);
    const quint32 bits = static_cast<quint32>((*root)["Personal"].list()[static_cast<size_t>(index)][field.constData()].i);
    return (bits >> (bit % 32)) & 1;
}

void PersonalTable::setTM(int index, int machineNo, bool value) {
    if (machineNo < 1 || machineNo > 128 || !root || index < 0 || index >= count()) return;
    const int bit = machineNo - 1;
    const QByteArray field = "machine" + QByteArray::number(bit / 32 + 1);
    UValue& v = (*root)["Personal"].list()[static_cast<size_t>(index)][field.constData()];
    quint32 bits = static_cast<quint32>(v.i);
    if (value) bits |= (1u << (bit % 32));
    else bits &= ~(1u << (bit % 32));
    v.i = bits;
}

// --------------------------------------------------------- Entwicklungen

int EvolutionTable::count() const { return root ? static_cast<int>((*root)["Evolve"].list().size()) : 0; }

QList<Evolution> EvolutionTable::get(int index) const {
    QList<Evolution> result;
    if (!root || index < 0 || index >= count()) return result;
    const std::vector<UValue>& ar = (*root)["Evolve"].list()[static_cast<size_t>(index)]["ar"].list();
    for (size_t k = 0; k + 4 < ar.size(); k += 5) {
        Evolution e;
        e.method = static_cast<int>(ar[k].i);
        e.argument = static_cast<int>(ar[k + 1].i);
        e.species = static_cast<int>(ar[k + 2].i);
        e.form = static_cast<int>(ar[k + 3].i);
        e.level = static_cast<int>(ar[k + 4].i);
        if (e.method > 0 && e.species > 0) result.append(e);
    }
    return result;
}

void EvolutionTable::set(int index, const QList<Evolution>& evolutions) {
    if (!root || index < 0 || index >= count()) return;
    UValue& arValue = (*root)["Evolve"].list()[static_cast<size_t>(index)]["ar"];
    const UValue proto = arValue.newElement();
    std::vector<UValue>& ar = arValue.list();
    ar.clear();
    for (const Evolution& e : evolutions) {
        for (int v : {e.method, e.argument, e.species, e.form, e.level}) {
            UValue x = proto;
            x.i = v;
            ar.push_back(x);
        }
    }
}

// --------------------------------------------------------------- Lernsets

int LearnsetTable::count() const { return root ? static_cast<int>((*root)["WazaOboe"].list().size()) : 0; }

QList<QPair<int,int>> LearnsetTable::get(int index) const {
    QList<QPair<int,int>> result;
    if (!root || index < 0 || index >= count()) return result;
    const std::vector<UValue>& ar = (*root)["WazaOboe"].list()[static_cast<size_t>(index)]["ar"].list();
    for (size_t k = 0; k + 1 < ar.size(); k += 2) {
        const int level = static_cast<int>(ar[k].i);
        const int move = static_cast<int>(ar[k + 1].i);
        if (move > 0) result.append(qMakePair(move, level));
    }
    return result;
}

void LearnsetTable::set(int index, const QList<QPair<int,int>>& moves) {
    if (!root || index < 0 || index >= count()) return;
    UValue& arValue = (*root)["WazaOboe"].list()[static_cast<size_t>(index)]["ar"];
    const UValue proto = arValue.newElement();
    std::vector<UValue>& ar = arValue.list();
    ar.clear();
    for (const auto& m : moves) {
        UValue level = proto;
        level.i = m.second;
        UValue move = proto;
        move.i = m.first;
        ar.push_back(level);
        ar.push_back(move);
    }
}

// --------------------------------------------------------------- Attacken

void MoveTable::load(const UValue& root) {
    moves.clear();
    for (const UValue& w : root["Waza"].list()) {
        Move m;
        m.id = w["wazaNo"].toInt();
        m.canUse = w["isValid"].i != 0;
        m.type = w["type"].toInt();
        m.category = w["damageType"].toInt();
        m.power = w["power"].toInt();
        m.accuracy = w["hitPer"].toInt();
        m.priority = w["priority"].toInt();
        m.hitMin = w["hitCountMin"].toInt();
        m.hitMax = w["hitCountMax"].toInt();
        const quint32 flags = static_cast<quint32>(w["flags"].i);
        m.charge = (flags >> 1) & 1;
        m.recharge = (flags >> 2) & 1;
        moves.append(m);
    }
}

Move MoveTable::get(int id) const {
    if (id >= 0 && id < moves.size() && moves[id].id == id) return moves[id];
    for (const Move& m : moves) {
        if (m.id == id) return m;
    }
    return Move();
}

// ----------------------------------------------------------------- Trainer

namespace {
const char* const kIvFields[6] = {"TalentHp", "TalentAtk", "TalentDef", "TalentAgi", "TalentSpAtk", "TalentSpDef"};
const char* const kEvFields[6] = {"EffortHp", "EffortAtk", "EffortDef", "EffortAgi", "EffortSpAtk", "EffortSpDef"};

int pokeField(const UValue& poke, int slot, const char* name) {
    const QByteArray field = "P" + QByteArray::number(slot + 1) + name;
    return static_cast<int>(poke[field.constData()].i);
}
void setPokeField(UValue& poke, int slot, const char* name, int value) {
    const QByteArray field = "P" + QByteArray::number(slot + 1) + name;
    UValue& v = poke[field.constData()];
    if (v.isValid()) v.i = value;
}
} // namespace

bool TrainerTable::load(const UValue& root) {
    trainers.clear();
    const std::vector<UValue>& types = root["TrainerType"].list();
    const std::vector<UValue>& data = root["TrainerData"].list();
    const std::vector<UValue>& pokes = root["TrainerPoke"].list();
    if (data.empty() || data.size() != pokes.size()) return false;
    for (size_t i = 0; i < data.size(); i++) {
        Trainer t;
        t.index = static_cast<int>(i);
        t.typeId = data[i]["TypeID"].toInt();
        t.fightType = data[i]["FightType"].toInt();
        t.aiBit = data[i]["AIBit"].toInt();
        t.nameLabel = data[i]["NameLabel"].toString();
        if (t.typeId >= 0 && static_cast<size_t>(t.typeId) < types.size()) {
            t.classLabel = types[static_cast<size_t>(t.typeId)]["LabelTrType"].toString();
        }
        const UValue& p = pokes[i];
        for (int slot = 0; slot < 6; slot++) {
            const int species = pokeField(p, slot, "MonsNo");
            if (species <= 0) break;
            TrainerMon m;
            m.species = species;
            m.form = pokeField(p, slot, "FormNo");
            m.shiny = pokeField(p, slot, "IsRare") != 0;
            m.level = pokeField(p, slot, "Level");
            m.sex = pokeField(p, slot, "Sex");
            m.nature = pokeField(p, slot, "Seikaku");
            m.ability = pokeField(p, slot, "Tokusei");
            for (int k = 0; k < 4; k++) m.moves[k] = pokeField(p, slot, QByteArray("Waza" + QByteArray::number(k + 1)).constData());
            m.item = pokeField(p, slot, "Item");
            m.ball = pokeField(p, slot, "Ball");
            m.seal = pokeField(p, slot, "Seal");
            for (int k = 0; k < 6; k++) {
                m.ivs[k] = pokeField(p, slot, kIvFields[k]);
                m.evs[k] = pokeField(p, slot, kEvFields[k]);
            }
            t.team.append(m);
        }
        trainers.append(t);
    }
    return true;
}

bool TrainerTable::anyChanged() const {
    for (const Trainer& t : trainers) {
        if (t.changed) return true;
    }
    return false;
}

void TrainerTable::store(UValue& root) const {
    std::vector<UValue>& data = root["TrainerData"].list();
    std::vector<UValue>& pokes = root["TrainerPoke"].list();
    for (const Trainer& t : trainers) {
        if (!t.changed || t.index < 0 || static_cast<size_t>(t.index) >= data.size()) continue;
        UValue& d = data[static_cast<size_t>(t.index)];
        d["AIBit"].i = t.aiBit;
        UValue& p = pokes[static_cast<size_t>(t.index)];
        for (int slot = 0; slot < 6; slot++) {
            TrainerMon m;
            const bool used = slot < t.team.size();
            if (used) {
                m = t.team[slot];
            } else {
                m.level = 0;
                m.ball = 28;
            }
            setPokeField(p, slot, "MonsNo", used ? m.species : 0);
            setPokeField(p, slot, "FormNo", m.form);
            setPokeField(p, slot, "IsRare", m.shiny ? 1 : 0);
            setPokeField(p, slot, "Level", m.level);
            setPokeField(p, slot, "Sex", m.sex);
            setPokeField(p, slot, "Seikaku", m.nature);
            setPokeField(p, slot, "Tokusei", m.ability);
            for (int k = 0; k < 4; k++) {
                setPokeField(p, slot, QByteArray("Waza" + QByteArray::number(k + 1)).constData(), m.moves[k]);
            }
            setPokeField(p, slot, "Item", m.item);
            setPokeField(p, slot, "Ball", m.ball);
            setPokeField(p, slot, "Seal", m.seal);
            for (int k = 0; k < 6; k++) {
                setPokeField(p, slot, kIvFields[k], m.ivs[k]);
                setPokeField(p, slot, kEvFields[k], m.evs[k]);
            }
        }
    }
}

// ------------------------------------------------------------ Gesamtpaket

GameFiles& GameFiles::operator=(const GameFiles& other) {
    if (this == &other) return *this;
    personalBundle = other.personalBundle;
    masterBundle = other.masterBundle;
    moves = other.moves;
    trainers = other.trainers;
    tmMoves = other.tmMoves;
    trainerZones = other.trainerZones;
    personalChanged = other.personalChanged;
    learnsetsChanged = other.learnsetsChanged;
    evolutionsChanged = other.evolutionsChanged;
    tradesChanged = other.tradesChanged;
    shopsChanged = other.shopsChanged;
    attachTables();
    return *this;
}

void GameFiles::attachTables() {
    personal.attach(&personalBundle.table("PersonalTable"));
    evolutions.attach(&personalBundle.table("EvolveTable"));
    learnsets.attach(&personalBundle.table("WazaOboeTable"));
}

bool GameFiles::load(const QString& romfs, QStringList* errors) {
    QString err;
    if (!personalBundle.load(romfs, path::Personal,
                             {"PersonalTable", "EvolveTable", "WazaOboeTable", "WazaTable", "ItemTable"}, &err) ||
        !masterBundle.load(romfs, path::Master, {"TrainerTable", "LocalKoukanData", "ShopTable"}, &err)) {
        if (errors) *errors << err;
        return false;
    }
    attachTables();
    moves.load(personalBundle.table("WazaTable"));
    if (!trainers.load(masterBundle.table("TrainerTable"))) {
        if (errors) *errors << "Die Trainer-Tabelle konnte nicht gelesen werden.";
        return false;
    }
    tmMoves.clear();
    for (const UValue& m : personalBundle.table("ItemTable")["WazaMachine"].list()) {
        const int no = m["machineNo"].toInt();
        if (no >= 1 && no <= 100) tmMoves.insert(no, m["wazaNo"].toInt());
    }
    // Wo steht welcher Trainer? (PlaceData_<Karte>)
    trainerZones.clear();
    for (const auto& place : masterBundle.readPrefixed("PlaceData_")) {
        const QString zone = place.first.mid(10);
        for (const UValue& e : place.second["Data"].list()) {
            const int id = e["TrainerID"].toInt();
            if (id > 0 && !trainerZones.value(id).contains(zone)) trainerZones[id].append(zone);
        }
    }
    if (personal.count() < 494 || moves.all().size() < 400 || trainers.all().size() < 100) {
        if (errors) *errors << "Die Spieldaten sind unvollständig.";
        return false;
    }
    return true;
}

bool GameFiles::save(const QString& outRomfs, QString* error) {
    QStringList personalTables;
    if (personalChanged) personalTables << "PersonalTable";
    if (evolutionsChanged) personalTables << "EvolveTable";
    if (learnsetsChanged) personalTables << "WazaOboeTable";
    if (!personalTables.isEmpty() && !personalBundle.save(outRomfs, personalTables, error)) return false;
    QStringList masterTables;
    if (trainers.anyChanged()) {
        trainers.store(masterBundle.table("TrainerTable"));
        masterTables << "TrainerTable";
    }
    if (tradesChanged) masterTables << "LocalKoukanData";
    if (shopsChanged) masterTables << "ShopTable";
    if (!masterTables.isEmpty() && !masterBundle.save(outRomfs, masterTables, error)) return false;
    return true;
}

GameData GameFiles::toGameData() const {
    GameData data;
    // Entwicklungsstufe: Tiefe in der Entwicklungskette
    QHash<qint64, qint64> parent;
    for (int index = 1; index < personal.count(); index++) {
        const QPair<int,int> sf = personal.speciesForm(index);
        if (sf.first <= 0) continue;
        for (const Evolution& e : evolutions.get(index)) {
            parent.insert(key(e.species, e.form), key(sf.first, sf.second));
        }
    }
    for (int index = 1; index < personal.count(); index++) {
        const QPair<int,int> sf = personal.speciesForm(index);
        if (sf.first <= 0) continue;
        const QList<Evolution> evos = evolutions.get(index);

        GameData::Form form;
        form.natdex = sf.first;
        form.form = sf.second;
        int stage = 1;
        qint64 k = key(sf.first, sf.second);
        while (parent.contains(k) && stage < 4) {
            k = parent.value(k);
            stage++;
        }
        form.stage = stage;
        form.fullyEvolved = evos.isEmpty();
        data.forms.append(form);

        GameData::Stats stats;
        stats.natdex = sf.first;
        stats.form = sf.second;
        stats.type1 = personal.type1(index);
        stats.type2 = personal.type2(index);
        stats.atk = personal.stat(index, 1);
        stats.spa = personal.stat(index, 4);
        stats.levelMoves = learnsets.get(index);
        for (auto it = tmMoves.constBegin(); it != tmMoves.constEnd(); ++it) {
            if (personal.canLearnTM(index, it.key())) stats.tmMoves.append(it.value());
        }
        data.stats.append(stats);

        for (const Evolution& e : evos) {
            GameData::Evolution ge;
            ge.fromNatdex = sf.first;
            ge.fromForm = sf.second;
            ge.toNatdex = e.species;
            ge.toForm = e.form;
            ge.level = e.level;
            ge.condition = e.method;
            data.evolutions.append(ge);
        }
    }
    for (const Move& m : moves.all()) {
        GameData::Move gm;
        gm.id = m.id;
        gm.type = m.type;
        gm.category = m.category;
        gm.power = m.power;
        gm.accuracy = m.accuracy;
        gm.hitMin = m.hitMin;
        gm.hitMax = m.hitMax;
        gm.priority = m.priority;
        gm.canUse = m.canUse;
        gm.charge = m.charge;
        gm.recharge = m.recharge;
        data.moves.append(gm);
    }
    return data;
}

// ------------------------------------------------------------- Spieltexte

namespace {
QString joinWords(const UValue& label) {
    QString text;
    for (const UValue& w : label["wordDataArray"].list()) text += w["str"].toString();
    return text;
}
} // namespace

QStringList messageLines(const UValue& msg) {
    QStringList lines;
    for (const UValue& label : msg["labelDataArray"].list()) {
        const int index = label["arrayIndex"].toInt();
        if (index < 0 || index > 100000) continue;
        while (lines.size() <= index) lines.append(QString());
        lines[index] = joinWords(label);
    }
    return lines;
}

QHash<QString, QString> messageLabels(const UValue& msg) {
    QHash<QString, QString> result;
    for (const UValue& label : msg["labelDataArray"].list()) {
        result.insert(label["labelName"].toString(), joinWords(label));
    }
    return result;
}

bool GameTexts::load(const QString& romfs) {
    QString err;
    BundleData common;
    if (common.load(romfs, path::MessageCommon, {"german_ss_monsname"}, &err)) {
        pokemon = messageLines(common.table("german_ss_monsname"));
    } else if (common.load(romfs, path::MessageCommon, {"english_ss_monsname"}, &err)) {
        pokemon = messageLines(common.table("english_ss_monsname"));
    }
    const bool german = QFileInfo::exists(romfs + "/" + path::MessageGerman);
    const QString lang = german ? "german" : "english";
    const QStringList tables = {lang + "_ss_wazaname", lang + "_ss_itemname", lang + "_ss_typename",
                                lang + "_ss_tokusei", lang + "_dp_trainers_name", lang + "_dp_trainers_type"};
    BundleData msg;
    if (!msg.load(romfs, german ? path::MessageGerman : path::MessageEnglish, tables, &err)) return false;
    moves = messageLines(msg.table(tables[0]));
    items = messageLines(msg.table(tables[1]));
    types = messageLines(msg.table(tables[2]));
    abilities = messageLines(msg.table(tables[3]));
    trainerNames = messageLabels(msg.table(tables[4]));
    trainerClasses = messageLabels(msg.table(tables[5]));
    return !pokemon.isEmpty();
}

namespace {
QString lineOr(const QStringList& list, int index, const QString& fallback) {
    if (index >= 0 && index < list.size() && !list[index].isEmpty()) return list[index];
    return fallback;
}
} // namespace

QString GameTexts::pokemonName(int species, int form) const {
    const QString name = lineOr(pokemon, species, QString("#%1").arg(species));
    if (form <= 0) return name;
    QString suffix;
    switch (species) {
    case 201: {
        static const QString letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!?";
        suffix = form < letters.size() ? QString(letters[form]) : QString::number(form);
        break;
    }
    case 351: suffix = QStringList{"", "Sonnenform", "Regenform", "Schneeform"}.value(form); break;
    case 386: suffix = QStringList{"", "Angriffsform", "Verteidigungsform", "Initiativeform"}.value(form); break;
    case 412: case 413: case 414: suffix = QStringList{"", "Sandumhang", "Lumpenumhang"}.value(form); break;
    case 421: suffix = "Sonnenform"; break;
    case 422: case 423: suffix = "Östliches Meer"; break;
    case 479: return QStringList{"", "Hitze-Rotom", "Wasch-Rotom", "Frost-Rotom", "Wirbel-Rotom", "Schneid-Rotom"}.value(form, name);
    case 487: suffix = "Urform"; break;
    case 492: suffix = "Zenitform"; break;
    case 493: suffix = typeName(form); break;
    default: suffix = QString("Form %1").arg(form); break;
    }
    return suffix.isEmpty() ? name : name + " (" + suffix + ")";
}

QString GameTexts::moveName(int id) const { return lineOr(moves, id, QString("Attacke %1").arg(id)); }
QString GameTexts::itemName(int id) const { return lineOr(items, id, QString("Item %1").arg(id)); }
QString GameTexts::typeName(int id) const {
    static const QStringList fallback = {"Normal", "Kampf", "Flug", "Gift", "Boden", "Gestein", "Käfer", "Geist", "Stahl",
                                         "Feuer", "Wasser", "Pflanze", "Elektro", "Psycho", "Eis", "Drache", "Unlicht", "Fee"};
    return lineOr(types, id, lineOr(fallback, id, "?"));
}

QString GameTexts::trainerLabel(const Trainer& trainer) const {
    const QString cls = trainerClasses.value(trainer.classLabel);
    const QString name = trainerNames.value(trainer.nameLabel);
    const QString label = (cls + " " + name).trimmed();
    return label.isEmpty() ? QString("Trainer %1").arg(trainer.index) : label;
}

QHash<int, QString> zoneNames(const QString& romfs, const UValue* mapInfo) {
    QHash<int, QString> names;
    QString err;
    BundleData settings;
    if (mapInfo == nullptr) {
        if (!settings.load(romfs, path::GameSettings, {"MapInfo"}, &err)) return names;
        mapInfo = &settings.table("MapInfo");
    }
    BundleData msg;
    const bool german = QFileInfo::exists(romfs + "/" + path::MessageGerman);
    const QString table = german ? "german_dp_fld_areaname" : "english_dp_fld_areaname";
    QHash<QString, QString> labels;
    if (msg.load(romfs, german ? path::MessageGerman : path::MessageEnglish, {table}, &err)) {
        labels = messageLabels(msg.table(table));
    }
    for (const UValue& z : (*mapInfo)["ZoneData"].list()) {
        const QString name = labels.value(z["PokePlaceName"].toString());
        if (!name.isEmpty()) names.insert(z["ZoneID"].toInt(), name);
    }
    return names;
}

} // namespace bdsp
