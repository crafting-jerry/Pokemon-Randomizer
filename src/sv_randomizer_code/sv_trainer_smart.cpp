#include "headers/sv_randomizer_headers/sv_trainer_smart.h"

#include <QHash>

GameData svGameData(const json& mapping, const json& personal, const json& wazaTable, const json& moveNames) {
    GameData data;

    // --- Alle Formen aus pokemon_mapping.json; devid -> natdex fuer die Entwicklungen ---
    QHash<int,int> devToNat;
    const json& pokemons = mapping["pokemons"];
    for (size_t i = 0; i < pokemons.size(); i++) {
        int natdex = pokemons[i].value("natdex", static_cast<int>(i));
        int devid = pokemons[i].value("devid", natdex);
        devToNat[devid] = natdex;

        const json& forms = pokemons[i]["forms"];
        for (size_t f = 0; f < forms.size(); f++) {
            GameData::Form form;
            form.natdex = natdex;
            form.form = forms[f].value("form", static_cast<int>(f));
            form.stage = forms[f].value("evolutionary_stage", 0);
            form.fullyEvolved = (form.stage == 0 || form.stage == 3);
            data.forms.append(form);
        }
    }

    // --- Personal-Daten: Typen, Werte, Attacken, Entwicklungen ---
    for (const json& entry : personal["entry"]) {
        int natdex = entry["species"]["model"];
        int form = entry["species"]["form"];
        if (natdex <= 0) {
            continue;
        }

        GameData::Stats s;
        s.natdex = natdex;
        s.form = form;
        s.type1 = entry.value("type_1", 0);
        s.type2 = entry.value("type_2", s.type1);
        s.atk = entry["base_stats"].value("atk", 0);
        s.spa = entry["base_stats"].value("spa", 0);
        for (const json& lm : entry["levelup_moves"]) {
            s.levelMoves.append(qMakePair(int(lm["move"]), int(lm["level"])));
        }
        for (const json& tm : entry["tm_moves"]) {
            s.tmMoves.append(int(tm));
        }
        data.stats.append(s);

        for (const json& evo : entry["evolutions"]) {
            int targetDev = evo.value("species", 0);
            if (targetDev <= 0 || !devToNat.contains(targetDev)) {
                continue;
            }
            GameData::Evolution e;
            e.fromNatdex = natdex;
            e.fromForm = form;
            e.toNatdex = devToNat.value(targetDev);
            e.toForm = evo.value("form", 0);
            e.level = evo.value("level", 0);
            e.condition = evo.value("condition", 0);
            data.evolutions.append(e);
        }
    }

    // --- Attacken ---
    QHash<int, std::string> devNames;
    for (const json& m : moveNames["moves"]) {
        devNames[int(m["id"])] = m.value("devName", std::string());
    }
    for (const json& w : wazaTable["Table"]) {
        GameData::Move m;
        m.id = w.value("MoveID", 0);
        m.type = w.value("Type", 0);
        m.category = w.value("Category", 0);
        m.power = w.value("Power", 0);
        m.accuracy = w.value("Accuracy", 100);
        m.hitMin = w.value("HitMin", 0);
        m.hitMax = w.value("HitMax", 0);
        m.priority = w.value("Priority", 0);
        m.devName = devNames.value(m.id);
        m.canUse = w.value("CanUseMove", false) && !m.devName.empty();
        m.charge = w.value("Flag_Charge", false);
        m.recharge = w.value("Flag_Recharge", false);
        m.futureAttack = w.value("Flag_FutureAttack", false);
        data.moves.append(m);
    }

    return data;
}
