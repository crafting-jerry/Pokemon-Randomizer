#ifndef SV_TRAINER_SMART_H
#define SV_TRAINER_SMART_H

// ---------------------------------------------------------------------------
// Karmesin/Purpur-Anbindung der spielneutralen Team-Logik:
// uebersetzt die JSON-Daten des Spiels in das GameData-Modell.
// ---------------------------------------------------------------------------

#include <nlohmann/json.hpp>
#include "../common/trainer_smart.h"

using json = nlohmann::json;

// mapping:   pokemon_mapping.json
// personal:  personal_array (clean oder bereits randomisiert)
// wazaTable: waza_array_clean.json
// moveNames: sorted_move_list.json (fuer die WAZA_-Namen)
GameData svGameData(const json& mapping, const json& personal, const json& wazaTable, const json& moveNames);

#endif // SV_TRAINER_SMART_H
