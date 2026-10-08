// ---------------------------------------------------------------------------
// Spoiler-Log: schreibt nach dem Randomisieren eine HTML-Datei mit allen
// Trainer-Teams (deutsche Namen, durchsuchbar, nach Kategorien sortiert).
// Ausgabe: output/Spoiler-Log.html
// ---------------------------------------------------------------------------

#include "headers/sv_randomizer_headers/sv_trainers.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

namespace {

struct TrainerLabel {
    QString category;
    QString label;
};

struct LabelRule {
    QList<int> indexes;
    QString category;
    QString label;
    QString bossLabel; // falls trid "boss" oder "leader" enthaelt
};

QString esc(const QString& text) {
    return text.toHtmlEscaped();
}

// Reihenfolge der Kategorien im Log
const QStringList kCategoryOrder = {
    "Arenen",
    "Top Vier & Champ",
    "Rivalen & Freunde",
    "Team Star",
    "Akademie",
    "Kitakami (DLC 1)",
    "Blaubeer-Akademie (DLC 2)",
    "Sonstige Trainer"
};

} // namespace

void svTrainers::writeSpoilerLog() {
    // --- Deutsche Namen laden ---
    if(!QFile::exists("SV_FLATBUFFERS/german_names.json")){
        qDebug() << "german_names.json fehlt - Spoiler-Log wird uebersprungen";
        return;
    }
    json names = readJsonQFile("SV_FLATBUFFERS/german_names.json");
    json moveList = readJsonQFile("SV_FLATBUFFERS/SV_PERSONAL/sorted_move_list.json");

    QStringList typeNames;
    for (const json& t : names["types"]) {
        typeNames.append(QString::fromStdString(t.get<std::string>()));
    }
    auto typeName = [&](int type) -> QString {
        return (type >= 0 && type < typeNames.size()) ? typeNames[type] : QString("?");
    };

    auto pokemonName = [&](int natdex) -> QString {
        std::string k = std::to_string(natdex);
        if (names["pokemon"].contains(k)) {
            return QString::fromStdString(names["pokemon"][k].get<std::string>());
        }
        return QString("#%1").arg(natdex);
    };

    QHash<QString, int> moveIdByDev;
    for (const json& m : moveList["moves"]) {
        moveIdByDev[QString::fromStdString(m.value("devName", std::string()))] = m.value("id", 0);
    }
    auto moveName = [&](int id) -> QString {
        std::string k = std::to_string(id);
        if (names["moves"].contains(k)) {
            return QString::fromStdString(names["moves"][k].get<std::string>());
        }
        return QString("Attacke %1").arg(id);
    };

    auto itemName = [&](const std::string& dev) -> QString {
        if (names["items"].contains(dev)) {
            return QString::fromStdString(names["items"][dev].get<std::string>());
        }
        return QString::fromStdString(dev);
    };

    // devName -> natdex und Formnamen
    QHash<QString, int> natdexByDev;
    for (const json& p : pokemonMapping["pokemons"]) {
        natdexByDev[QString::fromStdString(p.value("devName", std::string()))] = p.value("natdex", 0);
    }
    auto formName = [&](int natdex, int form) -> QString {
        if (form == 0 || natdex <= 0 || natdex >= static_cast<int>(pokemonMapping["pokemons"].size())) {
            return QString();
        }
        const json& forms = pokemonMapping["pokemons"][natdex]["forms"];
        if (form >= static_cast<int>(forms.size())) {
            return QString("Form %1").arg(form);
        }
        std::string raw = forms[form].value("formName", std::string());
        if (raw.empty()) {
            return QString("Form %1").arg(form);
        }
        if (names["forms"].contains(raw)) {
            return QString::fromStdString(names["forms"][raw].get<std::string>());
        }
        return QString::fromStdString(raw);
    };

    // --- Trainer beschriften ---
    QList<LabelRule> rules = {
        // Arenen
        {bugGym, "Arenen", "Arenatrainer (Käfer)", "Ronah · Arenaleiter (Käfer)"},
        {grassGym, "Arenen", "Arenatrainer (Pflanze)", "Colzo · Arenaleiter (Pflanze)"},
        {electricGym, "Arenen", "Arenatrainer (Elektro)", "Enigmara · Arenaleiterin (Elektro)"},
        {waterGym, "Arenen", "Arenatrainer (Wasser)", "Kombu · Arenaleiter (Wasser)"},
        {normalGym, "Arenen", "Arenatrainer (Normal)", "Aoki · Arenaleiter (Normal)"},
        {ghostGym, "Arenen", "Arenatrainer (Geist)", "Etta · Arenaleiterin (Geist)"},
        {psychicGym, "Arenen", "Arenatrainer (Psycho)", "Tulia · Arenaleiterin (Psycho)"},
        {iceGym, "Arenen", "Arenatrainer (Eis)", "Grusha · Arenaleiter (Eis)"},
        // Top Vier
        {e4Ground, "Top Vier & Champ", "Cay · Top Vier (Boden)", ""},
        {e4Steel, "Top Vier & Champ", "Poppy · Top Vier (Stahl)", ""},
        {e4Flying, "Top Vier & Champ", "Aoki · Top Vier (Flug)", ""},
        {e4Dragon, "Top Vier & Champ", "Sinius · Top Vier (Drache)", ""},
        {geeta, "Top Vier & Champ", "Sagaria · Top-Champ", ""},
        // Rivalen
        {nemona_fire, "Rivalen & Freunde", "Nemila (Variante Feuer-Starter)", ""},
        {nemona_grass, "Rivalen & Freunde", "Nemila (Variante Pflanzen-Starter)", ""},
        {nemona_water, "Rivalen & Freunde", "Nemila (Variante Wasser-Starter)", ""},
        {arven, "Rivalen & Freunde", "Pepper", ""},
        {penny, "Rivalen & Freunde", "Cosima", ""},
        {clavell_fire, "Rivalen & Freunde", "Clavel (Variante Feuer)", ""},
        {clavell_grass, "Rivalen & Freunde", "Clavel (Variante Pflanze)", ""},
        {clavell_water, "Rivalen & Freunde", "Clavel (Variante Wasser)", ""},
        {professors, "Rivalen & Freunde", "Professor", ""},
        // Team Star
        {starDark, "Team Star", "Rüpel der Segin-Bande (Unlicht)", "Pinio · Boss der Segin-Bande (Unlicht)"},
        {starFire, "Team Star", "Rüpel der Schedir-Bande (Feuer)", "Irsa · Boss der Schedir-Bande (Feuer)"},
        {starPoison, "Team Star", "Rüpel der Tsih-Bande (Gift)", "Shugi · Boss der Tsih-Bande (Gift)"},
        {starFairy, "Team Star", "Rüpel der Rukbat-Bande (Fee)", "Otis · Boss der Rukbat-Bande (Fee)"},
        {starFight, "Team Star", "Rüpel der Caph-Bande (Kampf)", "Boss der Caph-Bande (Kampf)"},
        // Akademie
        {raifort, "Akademie", "Lehrkraft", ""},
        {saguaro, "Akademie", "Lehrkraft", ""},
        {salvatore, "Akademie", "Lehrkraft", ""},
        {ryme, "Akademie", "Lehrkraft", ""},
        {zinia, "Akademie", "Lehrkraft", ""},
        {dendra, "Akademie", "Lehrkraft", ""},
        {miriam, "Akademie", "Lehrkraft", ""},
        // DLC 1
        {kieran_dlc1, "Kitakami (DLC 1)", "Rivale", ""},
        {kieran_stront_dlc1, "Kitakami (DLC 1)", "Rivale (stark)", ""},
        {carmine_dlc1, "Kitakami (DLC 1)", "Rivalin", ""},
        {carmine_strong_dlc1, "Kitakami (DLC 1)", "Rivalin (stark)", ""},
        {ogreClan, "Kitakami (DLC 1)", "Bande", ""},
        {ogreClanBoss, "Kitakami (DLC 1)", "Bandenboss", ""},
        {perrin, "Kitakami (DLC 1)", "Trainerin", ""},
        {oNareFamily, "Kitakami (DLC 1)", "Trainer", ""},
        // DLC 2
        {bb4Dragon, "Blaubeer-Akademie (DLC 2)", "Blaubeer-Top-Vier (Drache)", ""},
        {bb4DragonTrainers, "Blaubeer-Akademie (DLC 2)", "Blaubeer-Trainer (Drache)", ""},
        {bb4Farity, "Blaubeer-Akademie (DLC 2)", "Blaubeer-Top-Vier (Fee)", ""},
        {bb4FairyTrainers, "Blaubeer-Akademie (DLC 2)", "Blaubeer-Trainer (Fee)", ""},
        {bb4Steel, "Blaubeer-Akademie (DLC 2)", "Blaubeer-Top-Vier (Stahl)", ""},
        {bb4Fire, "Blaubeer-Akademie (DLC 2)", "Blaubeer-Top-Vier (Feuer)", ""},
        {bb4FireTrainers, "Blaubeer-Akademie (DLC 2)", "Blaubeer-Trainer (Feuer)", ""},
        {kieran, "Blaubeer-Akademie (DLC 2)", "Rivale", ""},
        {carmine, "Blaubeer-Akademie (DLC 2)", "Rivalin", ""},
        {schoolwars, "Blaubeer-Akademie (DLC 2)", "Schulduell", ""},
        {epilogue, "Blaubeer-Akademie (DLC 2)", "Epilog", ""},
        {terapagos, "Blaubeer-Akademie (DLC 2)", "Terapagos-Kampf", ""},
        {shiano, "Blaubeer-Akademie (DLC 2)", "Trainer", ""}
    };

    QHash<int, TrainerLabel> labels;
    for (const LabelRule& rule : rules) {
        for (int index : rule.indexes) {
            if (labels.contains(index) || index < 0 || index >= static_cast<int>(trainersData["values"].size())) {
                continue;
            }
            QString trid = QString::fromStdString(trainersData["values"][index].value("trid", std::string()));
            bool isBoss = trid.contains("leader") || trid.contains("boss");
            labels[index] = {rule.category, (isBoss && !rule.bossLabel.isEmpty()) ? rule.bossLabel : rule.label};
        }
    }

    // --- Trainer-Karten erzeugen ---
    QHash<QString, QString> sectionHtml;
    QHash<QString, int> sectionCount;
    int totalTrainers = 0;

    const json& values = trainersData["values"];
    for (int index = 0; index < static_cast<int>(values.size()); index++) {
        const json& entry = values[index];
        TrainerLabel label = labels.value(index, {"Sonstige Trainer", "Trainer"});
        QString trid = QString::fromStdString(entry.value("trid", std::string()));

        QString cards;
        QString searchText = label.label + " " + trid;
        int highestLevel = 0;
        int count = 0;

        for (int slot = 1; slot <= 6; slot++) {
            const json& poke = entry["poke" + std::to_string(slot)];
            QString dev = QString::fromStdString(poke.value("devId", std::string("DEV_NULL")));
            if (dev == "DEV_NULL" || !natdexByDev.contains(dev)) {
                continue;
            }
            count++;

            int natdex = natdexByDev.value(dev);
            int form = poke.value("formId", 0);
            int level = poke.value("level", 0);
            highestLevel = std::max(highestLevel, level);

            QString name = pokemonName(natdex);
            QString formText = formName(natdex, form);
            QPair<int,int> types = smartData.types(natdex, form);

            // Attacken
            QStringList moveTexts;
            bool isDefault = poke.value("wazaType", std::string("DEFAULT")) != "MANUAL";
            if (isDefault) {
                for (int id : smartData.defaultMoveset(natdex, form, level)) {
                    moveTexts.append(moveName(id));
                }
            } else {
                for (int k = 1; k <= 4; k++) {
                    QString waza = QString::fromStdString(poke["waza" + std::to_string(k)].value("wazaId", std::string("WAZA_NULL")));
                    if (waza != "WAZA_NULL" && moveIdByDev.contains(waza)) {
                        moveTexts.append(moveName(moveIdByDev.value(waza)));
                    }
                }
            }

            // Item und Tera-Typ
            std::string item = poke.value("item", std::string("ITEMID_NONE"));
            QString itemText = (item == "ITEMID_NONE") ? QString() : itemName(item);

            QString gem = QString::fromStdString(poke.value("gemType", std::string("DEFAULT")));
            int teraIndex = teraTypes.indexOf(gem);
            bool canTera = entry.value("changeGem", false) == true;

            searchText += " " + name + " " + formText + " " + moveTexts.join(" ") + " " + itemText;

            QString card;
            card += "<div class=\"mon\">";
            card += "<div class=\"mon-head\"><span class=\"mon-name\">" + esc(name) + "</span>";
            if (poke.value("rareType", std::string()) == "RARE") {
                card += "<span class=\"shiny\" title=\"Schillernd\">✦</span>";
            }
            card += "<span class=\"lvl\">Lv. " + QString::number(level) + "</span></div>";
            if (!formText.isEmpty()) {
                card += "<div class=\"form\">" + esc(formText) + "</div>";
            }
            card += "<div class=\"types\"><span class=\"type t" + QString::number(types.first) + "\">" + esc(typeName(types.first)) + "</span>";
            if (types.second != types.first) {
                card += "<span class=\"type t" + QString::number(types.second) + "\">" + esc(typeName(types.second)) + "</span>";
            }
            card += "</div>";
            if (teraIndex >= 0) {
                card += "<div class=\"meta\">Tera: <span class=\"type t" + QString::number(teraIndex) + "\">" + esc(typeName(teraIndex)) + "</span>";
                if (canTera) {
                    card += " <span class=\"hint\">(terakristallisiert)</span>";
                }
                card += "</div>";
            }
            if (!itemText.isEmpty()) {
                card += "<div class=\"meta\">Item: " + esc(itemText) + "</div>";
            }
            card += "<ul class=\"moves\">";
            for (const QString& mv : moveTexts) {
                card += "<li>" + esc(mv) + "</li>";
            }
            if (moveTexts.isEmpty()) {
                card += "<li class=\"hint\">keine Daten</li>";
            }
            card += "</ul>";
            if (isDefault && !moveTexts.isEmpty()) {
                card += "<div class=\"hint\">Spiel-Standard</div>";
            }
            card += "</div>";
            cards += card;
        }

        if (count == 0) {
            continue;
        }
        totalTrainers++;

        QString battle = (entry.value("battleType", std::string()) == "_2vs2") ? "Doppelkampf" : "Einzelkampf";
        QString block;
        block += "<article class=\"trainer\" data-search=\"" + esc(searchText.toLower()) + "\">";
        block += "<header><h3>" + esc(label.label) + "</h3>";
        block += "<div class=\"sub\">" + battle + " · max. Lv. " + QString::number(highestLevel) +
                 " · <code>" + esc(trid) + "</code> · #" + QString::number(index) + "</div></header>";
        block += "<div class=\"team\">" + cards + "</div></article>";

        sectionHtml[label.category] += block;
        sectionCount[label.category] += 1;
    }

    // --- Seite zusammensetzen ---
    QString html;
    html += R"(<!DOCTYPE html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Spoiler-Log · Pokémon Purpur Randomizer</title>
<style>
:root{--bg:#0f1117;--panel:#171a23;--card:#1e2230;--line:#2a2f40;--text:#e8eaf0;--muted:#8b91a5;--accent:#8b5cf6}
*{box-sizing:border-box}body{margin:0;font-family:"Segoe UI",system-ui,sans-serif;background:var(--bg);color:var(--text)}
.top{position:sticky;top:0;z-index:5;background:rgba(15,17,23,.95);backdrop-filter:blur(6px);border-bottom:1px solid var(--line);padding:16px 24px}
.top h1{margin:0 0 4px;font-size:22px}.top .info{color:var(--muted);font-size:13px;margin-bottom:12px}
.controls{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
#search{flex:1;min-width:220px;padding:10px 14px;border-radius:10px;border:1px solid var(--line);background:var(--panel);color:var(--text);font-size:15px}
#search:focus{outline:2px solid var(--accent);border-color:transparent}
.chip{padding:7px 12px;border-radius:999px;border:1px solid var(--line);background:var(--panel);color:var(--text);cursor:pointer;font-size:13px}
.chip.active{background:var(--accent);border-color:var(--accent)}
main{padding:20px 24px;max-width:1500px;margin:0 auto}
details.section{margin-bottom:18px;background:var(--panel);border:1px solid var(--line);border-radius:14px}
details.section>summary{cursor:pointer;padding:14px 18px;font-size:18px;font-weight:600;list-style:none}
details.section>summary::-webkit-details-marker{display:none}
details.section>summary .count{color:var(--muted);font-weight:400;font-size:14px;margin-left:8px}
.section-body{padding:0 18px 18px}
.trainer{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-top:14px}
.trainer h3{margin:0;font-size:16px}.trainer .sub{color:var(--muted);font-size:12px;margin-top:3px}
.trainer code{color:#b9a7f7}
.team{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:10px;margin-top:12px}
.mon{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:10px}
.mon-head{display:flex;align-items:baseline;gap:6px}.mon-name{font-weight:600;flex:1}
.lvl{color:var(--muted);font-size:13px}.shiny{color:#facc15}
.form{color:var(--muted);font-size:12px}
.types{margin:6px 0;display:flex;gap:4px;flex-wrap:wrap}
.type{display:inline-block;padding:2px 8px;border-radius:6px;font-size:11px;font-weight:600;color:#fff;text-shadow:0 1px 1px rgba(0,0,0,.4)}
.meta{font-size:12px;margin-top:4px;color:#cfd3df}
.moves{margin:8px 0 0;padding-left:16px;font-size:13px}.moves li{margin:2px 0}
.hint{color:var(--muted);font-size:11px}
.hidden{display:none!important}
.t0{background:#9fa19f}.t1{background:#ff8000}.t2{background:#81b9ef}.t3{background:#9141cb}.t4{background:#915121}
.t5{background:#afa981}.t6{background:#91a119}.t7{background:#704170}.t8{background:#60a1b8}.t9{background:#e62829}
.t10{background:#2980ef}.t11{background:#3fa129}.t12{background:#d4b000}.t13{background:#ef4179}.t14{background:#3dcef3}
.t15{background:#5060e1}.t16{background:#624d4e}.t17{background:#ef70ef}.t18{background:linear-gradient(90deg,#e62829,#d4b000,#3fa129,#2980ef,#9141cb)}
#empty{color:var(--muted);text-align:center;padding:40px}
</style></head><body>
<div class="top"><h1>Spoiler-Log</h1>)";

    html += "<div class=\"info\">Erstellt am " + QDateTime::currentDateTime().toString("dd.MM.yyyy 'um' HH:mm") +
            " · " + QString::number(totalTrainers) + " Trainer</div>";
    html += R"(<div class="controls"><input id="search" type="search" placeholder="Suchen: Trainer, Pokémon, Attacke, Item …" autocomplete="off">
<button class="chip" id="expand">Alle aufklappen</button><button class="chip" id="collapse">Alle zuklappen</button></div>
<div class="controls" id="chips" style="margin-top:10px"></div></div><main>)";

    for (const QString& category : kCategoryOrder) {
        if (!sectionHtml.contains(category)) {
            continue;
        }
        bool open = category != "Sonstige Trainer";
        html += "<details class=\"section\" data-cat=\"" + esc(category) + "\"" + (open ? " open" : "") + ">";
        html += "<summary>" + esc(category) + "<span class=\"count\">" + QString::number(sectionCount.value(category)) + " Trainer</span></summary>";
        html += "<div class=\"section-body\">" + sectionHtml.value(category) + "</div></details>";
    }

    html += R"(<div id="empty" class="hidden">Keine Treffer.</div></main>
<script>
(function(){
  var sections=[].slice.call(document.querySelectorAll('details.section'));
  var trainers=[].slice.call(document.querySelectorAll('.trainer'));
  var search=document.getElementById('search');
  var chips=document.getElementById('chips');
  var activeCats=new Set();
  sections.forEach(function(s){
    var b=document.createElement('button');b.className='chip';b.textContent=s.dataset.cat;
    b.onclick=function(){if(activeCats.has(s.dataset.cat)){activeCats.delete(s.dataset.cat);b.classList.remove('active');}else{activeCats.add(s.dataset.cat);b.classList.add('active');}apply();};
    chips.appendChild(b);
  });
  function apply(){
    var q=search.value.trim().toLowerCase();var terms=q?q.split(/\s+/):[];var any=false;
    sections.forEach(function(s){
      var catOk=activeCats.size===0||activeCats.has(s.dataset.cat);var visible=0;
      s.querySelectorAll('.trainer').forEach(function(t){
        var ok=catOk&&terms.every(function(w){return t.dataset.search.indexOf(w)!==-1;});
        t.classList.toggle('hidden',!ok);if(ok)visible++;
      });
      s.classList.toggle('hidden',visible===0);if(visible>0)any=true;
      if(terms.length&&visible>0)s.open=true;
    });
    document.getElementById('empty').classList.toggle('hidden',any);
  }
  search.addEventListener('input',apply);
  document.getElementById('expand').onclick=function(){sections.forEach(function(s){s.open=true;});};
  document.getElementById('collapse').onclick=function(){sections.forEach(function(s){s.open=false;});};
})();
</script></body></html>)";

    QDir().mkpath("output");
    QFile file("output/Spoiler-Log.html");
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << html;
        file.close();
        qDebug() << "Spoiler-Log geschrieben:" << QFileInfo(file).absoluteFilePath();
    } else {
        qDebug() << "Spoiler-Log konnte nicht geschrieben werden";
    }
}
