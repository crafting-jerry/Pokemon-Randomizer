#include "headers/sv_randomizer_headers/sv_fixed.h"
#include <stdexcept>

svFixed::svFixed(){

}

svFixed::~svFixed(){

}

void svFixed::randomize(){
    fixedEncounters = readJsonQFile("SV_FLATBUFFERS/SV_WILDS/fixed_symbol_table_array_clean.json");

    bool sizeCheck = getAllowedPokemon(fixedEncountersPokemon, allowedPokemon, "fixedEncounters");
    if(sizeCheck == false){
        throw std::runtime_error("Zu wenige erlaubte Pokémon für die statischen Begegnungen. Bitte unter „Erlaubte Pokémon“ mehr auswählen.");
    }
    int devid = 0;
    int form = 0;
    int gender = 0;
    bool rare = 0;
    for(unsigned long long i =0; i<fixedEncounters["values"].size(); i++){
        qDebug()<<"Randomizing "<<std::string(pokemonMapping["pokemons_dev"][std::string(fixedEncounters["values"][i]["pokeDataSymbol"]["devId"])]["name"]) << " fixedEncounters";
        randomizePokemon(allowedPokemon, devid, form, gender, rare, shinyRate, fixedEncounters["values"][i]["pokeDataSymbol"]);
        randomizeTeraType(fixedEncounters["values"][i]["pokeDataSymbol"], keepSameTera, devid, form);
    }

    closeFileAndDelete("SV_WILDS/fixed_symbol_table_array.json",
                       "SV_WILDS/fixed_symbol_table_array.bfbs",
                       "world/data/field/fixed_symbol/fixed_symbol_table/",
                       fixedEncounters, true);
}
