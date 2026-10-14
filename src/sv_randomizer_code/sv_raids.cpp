// ---------------------------------------------------------------------------
// Karmesin/Purpur: Tera-Raids (siehe sv_raids.h)
// ---------------------------------------------------------------------------

#include "headers/sv_randomizer_headers/sv_raids.h"
#include "headers/sv_randomizer_headers/sv_trainer_smart.h"

#include <QHash>
#include <QSet>
#include <functional>

namespace {

struct RaidFile {
    QString region;   // Anzeige
    QString prefix;   // "", "su1_", "su2_"
};

// Wetter und Felder der Zusatzaktionen: passend zum Typ des neuen Pokemon
const QSet<QString> kFieldMoves = {"WAZA_AMAGOI", "WAZA_NIHONBARE", "WAZA_SUNAARASI", "WAZA_YUKIGESIKI",
                                   "WAZA_EREKIFIIRUDO", "WAZA_GURASUFIIRUDO", "WAZA_SAIKOFIIRUDO",
                                   "WAZA_MISUTOFIIRUDO"};
QString fieldMoveForType(int type) {
    switch (type) {
    case 10: return "WAZA_AMAGOI";          // Wasser: Regentanz
    case 9: return "WAZA_NIHONBARE";        // Feuer: Sonnentag
    case 4:
    case 5: return "WAZA_SUNAARASI";        // Boden, Gestein: Sandsturm
    case 14: return "WAZA_YUKIGESIKI";      // Eis: Schneelandschaft
    case 12: return "WAZA_EREKIFIIRUDO";    // Elektro: Elektrofeld
    case 11: return "WAZA_GURASUFIIRUDO";   // Pflanze: Grasfeld
    case 13: return "WAZA_SAIKOFIIRUDO";    // Psycho: Psychofeld
    case 17: return "WAZA_MISUTOFIIRUDO";   // Fee: Nebelfeld
    default: return QString();
    }
}

// Tera-Typ von Ogerpon haengt an der Maske (Form)
const char* const kOgerponTera[] = {"KUSA", "MIZU", "HONOO", "IWA"};

constexpr int kTerapagos = 1024;
constexpr int kOgerpon = 1017;

} // namespace

svRaids::svRaids(){
    // Legendaere standardmaessig nicht in normalen Raids (in der Oberflaeche einstellbar)
    for (int i = 0; i < raidPokemon.genLegends.size(); i++) raidPokemon.genLegends[i] = false;
}

svRaids::~svRaids(){

}

bool svRaids::randomize(QString* error){
    spoilerTables.clear();
    if (!anyEnabled()) return true;

    // --- Erlaubte Pokemon ---
    allowedPokemon.clear();
    if (!getAllowedPokemon(raidPokemon, allowedPokemon, "Raids") || allowedPokemon.isEmpty()) {
        if (error) *error = "Für die Tera-Raids sind zu wenige Pokémon erlaubt. Bitte erlaube auf der Seite "
                            "„Raids & Bosse“ mehr Generationen oder Entwicklungsstufen.";
        return false;
    }

    // --- Daten: Typen, Werte, Attacken (randomisierte Pokemon-Daten haben Vorrang) ---
    const json& personal = personalOverride != nullptr ? *personalOverride : pokemonPersonalData;
    json wazaTable = readJsonQFile("SV_FLATBUFFERS/SV_PERSONAL/waza_array_clean.json");
    json moveNames = readJsonQFile("SV_FLATBUFFERS/SV_PERSONAL/sorted_move_list.json");
    TrainerSmartData smart;
    smart.build(svGameData(pokemonMapping, personal, wazaTable, moveNames));

    struct Info { int type1 = 0, type2 = 0, bst = 0, atk = 0, spa = 0; bool present = false; };
    QHash<qint64, Info> infos;
    auto key = [](int natdex, int form) { return static_cast<qint64>(natdex) * 100 + form; };
    for (const json& entry : personal["entry"]) {
        Info info;
        info.present = entry.value("is_present", false);
        info.type1 = entry.value("type_1", 0);
        info.type2 = entry.value("type_2", info.type1);
        const json& b = entry["base_stats"];
        info.atk = b.value("atk", 0);
        info.spa = b.value("spa", 0);
        info.bst = b.value("hp", 0) + info.atk + b.value("def", 0) + info.spa + b.value("spd", 0) + b.value("spe", 0);
        const int natdex = entry["species"].value("model", 0);
        const int form = entry["species"].value("form", 0);
        if (natdex > 0 && (!infos.contains(key(natdex, form)) || info.present)) infos.insert(key(natdex, form), info);
    }

    // Attacken: Name <-> ID, Schaden ja/nein
    QHash<QString, int> moveIdByName;
    QHash<int, QString> moveNameById;
    QHash<int, int> movePower;
    for (const json& m : moveNames["moves"]) {
        const QString dev = QString::fromStdString(m.value("devName", std::string()));
        const int id = m.value("id", 0);
        moveIdByName.insert(dev, id);
        moveNameById.insert(id, dev);
        movePower.insert(id, m["power"].is_number() ? int(m["power"]) : 0);
    }

    // Kandidaten (Art + Form), feste Reihenfolge
    struct Mon { int natdex; int form; };
    QList<Mon> candidates;
    for (auto it = allowedPokemon.cbegin(); it != allowedPokemon.cend(); ++it) {
        const int natdex = it.key();
        if (natdex <= 0 || natdex == kTerapagos) continue;
        QList<int> forms = it.value();
        std::sort(forms.begin(), forms.end());
        for (int form : forms) {
            if (natdex == kOgerpon && form > 3) continue; // Terakristall-Formen nur im Kampf
            if (!infos.value(key(natdex, form)).present) continue;
            candidates.append(Mon{natdex, form});
        }
    }
    if (candidates.size() < 10) {
        if (error) *error = "Für die Tera-Raids sind zu wenige Pokémon erlaubt.";
        return false;
    }

    auto sharesType = [&](const Info& a, const Info& b) {
        return a.type1 == b.type1 || a.type1 == b.type2 || a.type2 == b.type1 || a.type2 == b.type2;
    };

    // Waehlt ein Pokemon; die Regeln werden schrittweise gelockert (Staerke, Typ, Level, Doppelte)
    auto pick = [&](const Info& original, int level, const QSet<qint64>& used, Mon& out) {
        struct Stage { bool strength, type, levelRule, unique; };
        const Stage stages[] = {
            {similarStrength, sameType, levelAppropriate, true},
            {false, sameType, levelAppropriate, true},
            {false, false, levelAppropriate, true},
            {false, false, levelAppropriate, false},
            {false, false, false, false},
        };
        for (const Stage& st : stages) {
            QList<Mon> pool;
            for (const Mon& m : candidates) {
                if (st.unique && used.contains(key(m.natdex, 0))) continue;
                const Info info = infos.value(key(m.natdex, m.form));
                if (st.type && !sharesType(original, info)) continue;
                // Ab Level 60 (5★ und 6★) nur Endstufen, darunter keine Entwicklungen vor ihrem Level
                if (st.levelRule && !smart.isAllowedAtLevel(m.natdex, m.form, level, 60, true)) continue;
                if (st.strength && original.bst > 0 && qAbs(info.bst - original.bst) * 100 > original.bst * 15) continue;
                pool.append(m);
            }
            if (!pool.isEmpty()) {
                out = pool[randNum.bounded(static_cast<int>(pool.size()))];
                return true;
            }
        }
        return false;
    };

    QList<RaidFile> files;
    if (randomizePaldea) files.append(RaidFile{"Paldea", ""});
    if (randomizeKitakami) files.append(RaidFile{"Kitakami", "su1_"});
    if (randomizeBlueberry) files.append(RaidFile{"Blaubeer-Akademie", "su2_"});

    for (const RaidFile& file : files) {
        for (int stars = 1; stars <= 6; stars++) {
            const QString name = QString("%1raid_enemy_%2").arg(file.prefix).arg(stars, 2, 10, QChar('0'));
            json raids = readJsonQFile("SV_FLATBUFFERS/SV_RAIDS/" + name + "_array_clean.json");
            if (!raids.contains("values")) {
                if (error) *error = "Die Raid-Daten fehlen: SV_RAIDS/" + name + "_array_clean.json";
                return false;
            }
            SvRaidSpoilerTable table;
            table.region = file.region;
            table.stars = stars;

            // Doppelte nur innerhalb einer Edition vermeiden (Karmesin / Purpur getrennt)
            QSet<qint64> usedBoth, usedScarlet, usedViolet;
            for (json& raid : raids["values"]) {
                json& info = raid["raidEnemyInfo"];
                json& boss = info["bossPokePara"];
                const std::string devName = boss.value("devId", std::string());
                if (!pokemonMapping["pokemons_dev"].contains(devName)) continue;
                const int oldNatdex = pokemonMapping["pokemons_dev"][devName].value("natdex", 0);
                const int oldForm = boss.value("formId", 0);
                const int level = boss.value("level", 1);
                table.level = level;
                const std::string romVer = info.value("romVer", std::string("BOTH"));

                QSet<qint64> used = usedBoth;
                if (romVer != "TYPE_B") used += usedScarlet;
                if (romVer != "TYPE_A") used += usedViolet;
                Mon m{0, 0};
                if (!pick(infos.value(key(oldNatdex, oldForm)), level, used, m)) continue;
                (romVer == "TYPE_A" ? usedScarlet : romVer == "TYPE_B" ? usedViolet : usedBoth).insert(key(m.natdex, 0));

                // Art, Form, Geschlecht
                const json& mapped = pokemonMapping["pokemons"][m.natdex];
                boss["devId"] = mapped["devName"];
                boss["formId"] = m.form;
                boss["sex"] = "DEFAULT";
                boss["item"] = "ITEMID_NONE";
                if (forceShiny) boss["rareType"] = "RARE";
                else if (boss.value("rareType", std::string()) == "NO_RARE") boss["rareType"] = "DEFAULT";
                if (m.natdex == kOgerpon && m.form >= 0 && m.form <= 3) boss["gemType"] = kOgerponTera[m.form];

                // Attacken passend zum neuen Pokemon (starke Movesets wie bei den Trainern)
                const QStringList moves = smart.buildMoveset(m.natdex, m.form, level, true, randNum);
                QList<int> moveIds;
                if (moves.isEmpty()) {
                    boss["wazaType"] = "DEFAULT";
                    for (int k = 1; k <= 4; k++) boss["waza" + std::to_string(k)]["wazaId"] = "WAZA_NULL";
                } else {
                    boss["wazaType"] = "MANUAL";
                    for (int k = 1; k <= 4; k++) {
                        const QString move = k <= moves.size() ? moves[k - 1] : QString("WAZA_NULL");
                        boss["waza" + std::to_string(k)]["wazaId"] = move.toStdString();
                        if (moveIdByName.value(move) > 0) moveIds.append(moveIdByName.value(move));
                    }
                }
                // Staerkste Schadensattacke des neuen Movesets (sonst Tera-Explosion)
                QString strongest = "WAZA_TERABAASUTO";
                int bestPower = 0;
                for (int id : moveIds) {
                    if (movePower.value(id) > bestPower) {
                        bestPower = movePower.value(id);
                        strongest = moveNameById.value(id);
                    }
                }

                // Zusatzaktionen: Wetter/Felder passend zum Typ, Schadensattacken aus dem neuen Moveset;
                // allgemeine Statusattacken (Schwerttanz, Lichtschild ...) bleiben
                const Info newInfo = infos.value(key(m.natdex, m.form));
                QString field = fieldMoveForType(newInfo.type1);
                if (field.isEmpty()) field = fieldMoveForType(newInfo.type2);
                if (field.isEmpty()) field = newInfo.atk >= newInfo.spa ? "WAZA_TURUGINOMAI" : "WAZA_WARUDAKUMI";
                json& desc = info["bossDesc"];
                for (int k = 1; k <= 6; k++) {
                    json& action = desc["extraAction" + std::to_string(k)];
                    if (action.value("action", std::string()) != "WAZA") continue;
                    const QString move = QString::fromStdString(action.value("wazano", std::string("WAZA_NULL")));
                    if (kFieldMoves.contains(move)) action["wazano"] = field.toStdString();
                    else if (movePower.value(moveIdByName.value(move)) > 0) action["wazano"] = strongest.toStdString();
                }
                if (desc.value("powerChargePenaltyAction", std::string("WAZA_NULL")) != "WAZA_NULL") {
                    desc["powerChargePenaltyAction"] = strongest.toStdString();
                }

                SvRaidSpoilerMon logMon;
                logMon.natdex = m.natdex;
                logMon.form = m.form;
                logMon.oldNatdex = oldNatdex;
                logMon.oldForm = oldForm;
                logMon.shiny = boss.value("rareType", std::string()) == "RARE";
                logMon.tera = QString::fromStdString(boss.value("gemType", std::string("RANDOM")));
                logMon.moves = moveIds;
                logMon.edition = romVer == "TYPE_A" ? "Karmesin" : romVer == "TYPE_B" ? "Purpur" : QString();
                table.mons.append(logMon);
            }

            closeFileAndDelete("SV_RAIDS/" + name + "_array.json",
                               "SV_RAIDS/" + name + "_array.bfbs",
                               "world/data/raid/" + name + "/",
                               raids, true);
            spoilerTables.append(table);
        }
    }
    return true;
}
