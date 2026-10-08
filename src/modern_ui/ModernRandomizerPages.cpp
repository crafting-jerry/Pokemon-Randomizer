// ---------------------------------------------------------------------------
// Seiten der neuen Oberflaeche (ausser Start und Trainer):
// Starter & Geschenke, Wilde Pokemon, Statische Begegnungen, Pokemon-Daten,
// Items, Raids & Bosse. Alle Schalter schreiben direkt in die Felder des
// Randomizers und werden in Vorlagen mitgespeichert.
// ---------------------------------------------------------------------------

#include "headers/modern_ui/ModernRandomizerWindow.h"

#include <QCompleter>
#include <QFile>
#include <QJsonDocument>
#include <QSignalBlocker>

using namespace modernui;

namespace {

// Deutsche Namen der Baelle (interne Namen sind englisch)
const QList<QPair<QString, QString>> kBalls = {
    {"Poke Ball", "Pokéball"}, {"Great Ball", "Superball"}, {"Ultra Ball", "Hyperball"},
    {"Master Ball", "Meisterball"}, {"Beast Ball", "Ultraball"}, {"Cherish Ball", "Jubelball"},
    {"Luxury Ball", "Luxusball"}, {"Timer Ball", "Timerball"}, {"Net Ball", "Netzball"},
    {"Nest Ball", "Nestball"}, {"Dive Ball", "Tauchball"}, {"Dusk Ball", "Finsterball"},
    {"Repeat Ball", "Wiederball"}, {"Premier Ball", "Premierball"}, {"Heal Ball", "Heilball"},
    {"Quick Ball", "Flottball"}, {"Fast Ball", "Turboball"}, {"Level Ball", "Levelball"},
    {"Lure Ball", "Köderball"}, {"Heavy Ball", "Schwerball"}, {"Love Ball", "Sympaball"},
    {"Friend Ball", "Freundesball"}, {"Moon Ball", "Mondball"}, {"Sport Ball", "Turnierball"},
    {"Safari Ball", "Safariball"}, {"Dream Ball", "Traumball"}
};

const QList<QPair<QString, QString>> kGenders = {
    {"MALE", "Männlich"}, {"FEMALE", "Weiblich"}, {"DEFAULT", "Standard"}, {"GENDERLESS", "Geschlechtslos"}
};

QString germanForm(QString form) {
    static const QList<QPair<QString, QString>> words = {
        {"Alolan", "Alola"}, {"Galarian", "Galar"}, {"Hisuian", "Hisui"}, {"Paldean", "Paldea"},
        {"Combat", "Kampf"}, {"Blaze", "Feuer"}, {"Aqua", "Wasser"}, {"Female", "weiblich"}, {"Male", "männlich"},
        {"Original", "Original"}, {"Partner", "Partner"}, {"World", "Welt"}
    };
    for (const auto& w : words) {
        form.replace(w.first, w.second);
    }
    if (form == "—" || form.isEmpty()) {
        return "Normal";
    }
    return form;
}

const QStringList kStarterTitles = {"Felori (Pflanze)", "Krokel (Feuer)", "Kwaks (Wasser)"};

} // namespace

// ------------------------------------------------------------- Verknuepfung

QCheckBox* ModernRandomizerWindow::bindCheck(QBoxLayout* layout, const QString& key, const QString& text,
                                             const QString& info, bool* field, std::function<void(bool)> extra) {
    auto* box = new QCheckBox(text);
    box->setChecked(*field);
    layout->addLayout(rowWithInfo(box, info));
    boolBindings.push_back({key, field, box, extra});
    connect(box, &QCheckBox::toggled, this, [this, field, extra](bool checked) {
        *field = checked;
        if (extra) {
            extra(checked);
        }
        updateStatus();
    });
    return box;
}

void ModernRandomizerWindow::bindLimiter(QBoxLayout* layout, const QString& key, allowedPokemonLimiter* field) {
    auto* more = new Collapsible("Erlaubte Pokémon einschränken");
    auto* editor = new LimiterEditor(field);
    more->body()->addWidget(editor);
    layout->addWidget(more);
    limiterBindings.push_back({key, field, editor});
}

Card* ModernRandomizerWindow::sectionCard(QBoxLayout* parentLayout, const QString& title, const QString& key,
                                          const QString& switchText, const QString& info, bool* field,
                                          QVBoxLayout*& body, std::function<void(bool)> extra) {
    auto* card = new Card(title);
    auto* bodyWidget = new QWidget(card);
    auto* master = bindCheck(card->body(), key, switchText, info, field, [bodyWidget, extra](bool on) {
        bodyWidget->setEnabled(on);
        if (extra) {
            extra(on);
        }
    });
    master->setObjectName("masterSwitch");
    body = new QVBoxLayout(bodyWidget);
    body->setContentsMargins(26, 2, 0, 0);
    body->setSpacing(6);
    card->body()->addWidget(bodyWidget);
    bodyWidget->setEnabled(*field);
    parentLayout->addWidget(card);
    return card;
}

// --------------------------------------------------------- Starter & Geschenke

void ModernRandomizerWindow::loadPokemonNames() {
    QFile file("SV_FLATBUFFERS/german_names.json");
    QJsonObject german;
    if (file.open(QIODevice::ReadOnly)) {
        german = QJsonDocument::fromJson(file.readAll()).object()["pokemon"].toObject();
    }

    // Englischer Name -> Nationaldex ueber pokemon_mapping.json
    QHash<QString, int> natdexByName;
    const json& pokemons = code.pokemonMapping["pokemons"];
    for (size_t i = 0; i < pokemons.size(); i++) {
        natdexByName[QString::fromStdString(pokemons[i].value("name", std::string()))] = pokemons[i].value("natdex", 0);
    }

    for (const QString& english : code.pokemonInGame) {
        QString name = english;
        int natdex = natdexByName.value(english, 0);
        if (natdex > 0 && german.contains(QString::number(natdex))) {
            name = german[QString::number(natdex)].toString();
        }
        englishToGerman[english] = name;
        germanToEnglish[name.toLower()] = english;
        germanToEnglish[english.toLower()] = english; // englische Eingabe klappt auch
    }
}

void ModernRandomizerWindow::updateStarterForms(int index) {
    StarterRow& row = starterRows[index];
    QSignalBlocker block(row.form);
    row.form->clear();
    QString english = code.svRandomizerStarters.starters[index];
    QStringList forms = code.nationalDexPokemonNamesAndForms.value(english);
    for (const QString& f : forms) {
        row.form->addItem(germanForm(f));
    }
    if (forms.isEmpty()) {
        row.form->addItem("–");
    }
    int current = code.svRandomizerStarters.startersForms[index];
    if (current < 0 || current >= row.form->count()) {
        current = 0;
        code.svRandomizerStarters.startersForms[index] = 0;
    }
    row.form->setCurrentIndex(current);
    row.form->setEnabled(forms.size() > 1);
}

void ModernRandomizerWindow::updateStarterGenders(int index) {
    StarterRow& row = starterRows[index];
    QString english = code.svRandomizerStarters.starters[index];
    int form = code.svRandomizerStarters.startersForms[index];

    QStringList options;
    if (english.isEmpty()) {
        options = {"MALE", "FEMALE"};
    } else if (code.genderForms.contains(english)) {
        options = {"DEFAULT"};
    } else if (code.femaleOnlyPokemon.contains(english)) {
        options = {"FEMALE"};
    } else if (code.maleOnlyPokemon.contains(english) || code.formsMaleOnly.value(english).contains(form)) {
        options = {"MALE"};
    } else if (code.genderlessPokemon.contains(english)) {
        options = {"GENDERLESS"};
    } else {
        options = {"MALE", "FEMALE"};
    }

    QSignalBlocker block(row.gender);
    row.gender->clear();
    for (const QString& o : options) {
        for (const auto& g : kGenders) {
            if (g.first == o) {
                row.gender->addItem(g.second, g.first);
            }
        }
    }
    int current = row.gender->findData(code.svRandomizerStarters.startersGenders[index]);
    if (current < 0) {
        current = 0;
    }
    row.gender->setCurrentIndex(current);
    code.svRandomizerStarters.startersGenders[index] = row.gender->currentData().toString();
    row.gender->setEnabled(options.size() > 1);
}

void ModernRandomizerWindow::refreshStarters() {
    svStarters& st = code.svRandomizerStarters;
    for (int i = 0; i < 3; i++) {
        StarterRow& row = starterRows[i];
        if (row.name == nullptr) {
            continue;
        }
        {
            QSignalBlocker b1(row.name), b2(row.shiny), b3(row.ball);
            row.name->setText(st.starters[i].isEmpty() ? QString() : englishToGerman.value(st.starters[i], st.starters[i]));
            row.shiny->setChecked(st.startersShiny[i]);
            int ball = row.ball->findData(st.startersPokeballs[i]);
            row.ball->setCurrentIndex(ball < 0 ? 0 : ball);
        }
        row.hint->clear();
        row.hint->setVisible(false);
        updateStarterForms(i);
        updateStarterGenders(i);
    }
}

QJsonArray ModernRandomizerWindow::startersToJson() const {
    const svStarters& st = code.svRandomizerStarters;
    QJsonArray array;
    for (int i = 0; i < 3; i++) {
        QJsonObject o;
        o["pokemon"] = st.starters[i];
        o["form"] = st.startersForms[i];
        o["gender"] = st.startersGenders[i];
        o["ball"] = st.startersPokeballs[i];
        o["shiny"] = st.startersShiny[i];
        array.append(o);
    }
    return array;
}

void ModernRandomizerWindow::startersFromJson(const QJsonArray& array) {
    svStarters& st = code.svRandomizerStarters;
    for (int i = 0; i < 3 && i < array.size(); i++) {
        QJsonObject o = array[i].toObject();
        QString pokemon = o["pokemon"].toString();
        st.starters[i] = code.pokemonInGame.contains(pokemon) ? pokemon : QString();
        st.startersForms[i] = o["form"].toInt(0);
        st.startersGenders[i] = o["gender"].toString("MALE");
        st.startersPokeballs[i] = o["ball"].toString("Poke Ball");
        st.startersShiny[i] = o["shiny"].toBool(false);
    }
}

QWidget* ModernRandomizerWindow::buildStartersPage() {
    svStarters& st = code.svRandomizerStarters;
    loadPokemonNames();

    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // --- Starter ---
    QVBoxLayout* body = nullptr;
    sectionCard(layout, "Starter", "starters.randomize", "Starter randomisieren",
                "Ersetzt Felori, Krokel und Kwaks durch zufällige Pokémon. Einzelne Starter kannst du unten auch "
                "selbst festlegen.", &st.randomizeStarters, body);

    bindCheck(body, "starters.tera", "Tera-Typen zufällig",
              "Die Starter bekommen einen zufälligen Tera-Typ statt ihres eigenen Typs.", &st.randomizeStartersTeraTypes);
    bindCheck(body, "starters.oneShiny", "Ein Starter garantiert schillernd",
              "Einer der drei Starter ist garantiert schillernd.", &st.forceShinyStarter);
    bindCheck(body, "starters.allShiny", "Alle Starter schillernd",
              "Alle drei Starter sind schillernd.", &st.allStartersShiny);

    auto* title = new QLabel("Wunsch-Starter", content);
    title->setObjectName("sectionLabel");
    auto* titleRow = new QHBoxLayout();
    titleRow->addWidget(title);
    titleRow->addWidget(new InfoButton("Trag ein Pokémon ein, um diesen Starter festzulegen. Leer lassen für ein zufälliges "
                                       "Pokémon. Deutsche und englische Namen funktionieren.", content));
    titleRow->addStretch();
    body->addLayout(titleRow);

    QStringList completions = englishToGerman.values();
    completions.sort(Qt::CaseInsensitive);

    for (int i = 0; i < 3; i++) {
        StarterRow& row = starterRows[i];
        auto* line = new QHBoxLayout();
        line->setSpacing(8);

        auto* label = new QLabel(kStarterTitles[i], content);
        label->setFixedWidth(120);
        line->addWidget(label);

        row.name = new QLineEdit(content);
        row.name->setPlaceholderText("zufällig");
        row.name->setFixedWidth(170);
        auto* completer = new QCompleter(completions, row.name);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        row.name->setCompleter(completer);
        line->addWidget(row.name);

        row.form = new QComboBox(content);
        row.form->setFixedWidth(150);
        row.form->setToolTip("Form");
        line->addWidget(row.form);

        row.gender = new QComboBox(content);
        row.gender->setFixedWidth(120);
        row.gender->setToolTip("Geschlecht");
        line->addWidget(row.gender);

        row.ball = new QComboBox(content);
        for (const auto& b : kBalls) {
            row.ball->addItem(b.second, b.first);
        }
        row.ball->setFixedWidth(120);
        row.ball->setToolTip("Ball");
        line->addWidget(row.ball);

        row.shiny = new QCheckBox("Schillernd", content);
        line->addWidget(row.shiny);
        line->addStretch();
        body->addLayout(line);

        row.hint = mutedLabel("", content);
        row.hint->setContentsMargins(128, 0, 0, 0);
        row.hint->setVisible(false);
        body->addWidget(row.hint);

        connect(row.name, &QLineEdit::textChanged, this, [this, i](const QString& text) {
            StarterRow& r = starterRows[i];
            QString key = text.trimmed().toLower();
            QString english = germanToEnglish.value(key);
            code.svRandomizerStarters.starters[i] = english;
            code.svRandomizerStarters.startersForms[i] = 0;
            if (!key.isEmpty() && english.isEmpty()) {
                r.hint->setText("Unbekanntes Pokémon – dieser Starter wird zufällig gewählt.");
            } else {
                r.hint->clear();
            }
            r.hint->setVisible(!r.hint->text().isEmpty());
            updateStarterForms(i);
            updateStarterGenders(i);
        });
        connect(row.form, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i](int index) {
            code.svRandomizerStarters.startersForms[i] = index < 0 ? 0 : index;
            updateStarterGenders(i);
        });
        connect(row.gender, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i]() {
            code.svRandomizerStarters.startersGenders[i] = starterRows[i].gender->currentData().toString();
        });
        connect(row.ball, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i]() {
            code.svRandomizerStarters.startersPokeballs[i] = starterRows[i].ball->currentData().toString();
        });
        connect(row.shiny, &QCheckBox::toggled, this, [this, i](bool on) {
            code.svRandomizerStarters.startersShiny[i] = on;
        });
    }
    bindLimiter(body, "starters.allowed", &st.startersPokemon);
    refreshStarters();

    // --- Geschenke ---
    sectionCard(layout, "Geschenk-Pokémon", "gifts.randomize", "Geschenk-Pokémon randomisieren",
                "Pokémon, die du im Spiel geschenkt bekommst (z. B. von Personen in der Story), werden durch zufällige "
                "ersetzt.", &st.randomizeGifts, body);
    bindCheck(body, "gifts.tera", "Tera-Typen zufällig",
              "Geschenk-Pokémon bekommen einen zufälligen Tera-Typ.", &st.randomizeGiftsTeraTypes);
    bindLimiter(body, "gifts.allowed", &st.giftsPokemon);

    return wrapPage("Starter und Geschenke", "Lege fest, welche Pokémon du zu Beginn und unterwegs geschenkt bekommst.", content);
}

// ----------------------------------------------------------- Wilde Pokemon

QWidget* ModernRandomizerWindow::buildWildsPage() {
    svWilds& w = code.svRandomizerWilds;
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    const QString ogerInfo = "Ogerpon und Terapagos können auch wild auftauchen. Achtung: Fange sie, aber "
                             "terakristallisiere sie nicht – das kann das Spiel zum Absturz bringen.";

    QVBoxLayout* body = nullptr;
    sectionCard(layout, "Paldea", "wilds.paldea", "Wilde Pokémon in Paldea randomisieren",
                "Alle wild auftauchenden Pokémon im Hauptspiel werden zufällig.", &w.randomizePaldeaWild, body);
    bindCheck(body, "wilds.paldea.oger", "Ogerpon und Terapagos erlauben", ogerInfo, &w.ogerponTerapagosPaldea);
    bindLimiter(body, "wilds.paldea.allowed", &w.paldeaWilds);

    sectionCard(layout, "Kitakami", "wilds.kitakami", "Wilde Pokémon in Kitakami randomisieren",
                "Alle wild auftauchenden Pokémon aus „Die türkisgrüne Maske“ (DLC 1).", &w.randomizeKitakamiWild, body);
    bindCheck(body, "wilds.kitakami.oger", "Ogerpon und Terapagos erlauben", ogerInfo, &w.ogerponTerapagosKitakami);
    bindLimiter(body, "wilds.kitakami.allowed", &w.kitakamiWilds);

    sectionCard(layout, "Blaubeer-Akademie", "wilds.blueberry", "Wilde Pokémon im Tera-Dom randomisieren",
                "Alle wild auftauchenden Pokémon aus „Die Indigoblaue Scheibe“ (DLC 2).", &w.randomizeBlueberryWild, body);
    bindCheck(body, "wilds.blueberry.oger", "Ogerpon und Terapagos erlauben", ogerInfo, &w.ogerponTearapagosBlueberry);
    bindLimiter(body, "wilds.blueberry.allowed", &w.blueberrWilds);

    return wrapPage("Wilde Pokémon", "Lege fest, welche Pokémon in der Wildnis auftauchen.", content);
}

// ---------------------------------------------------- Statische Begegnungen

QWidget* ModernRandomizerWindow::buildFixedPage() {
    svFixed& f = code.svRandomizerFixed;
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QVBoxLayout* body = nullptr;
    sectionCard(layout, "Statische Begegnungen", "fixed.randomize", "Statische Begegnungen randomisieren",
                "Pokémon, die an festen Orten in der Spielwelt stehen (z. B. besondere Pokémon, die du ansprechen "
                "kannst), werden durch zufällige ersetzt.", &f.randomizeFixedEncounters, body);
    bindCheck(body, "fixed.sameTera", "Tera-Typen beibehalten",
              "Die neuen Pokémon behalten den Tera-Typ des ursprünglichen Pokémon an dieser Stelle.", &f.keepSameTera);
    bindLimiter(body, "fixed.allowed", &f.fixedEncountersPokemon);

    return wrapPage("Statische Begegnungen", "Feste Pokémon in der Spielwelt.", content);
}

// --------------------------------------------------------- Pokemon-Daten

QWidget* ModernRandomizerWindow::buildPersonalPage() {
    svPersonal& p = code.svRandomizerPersonal;
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* note = new Card("Hinweis");
    note->body()->addWidget(mutedLabel("Diese Optionen ändern die Pokémon selbst – überall im Spiel, auch bei deinen eigenen. "
                                       "Die Trainer-Einstellungen (starke Movesets, Level-Regeln) berücksichtigen die "
                                       "geänderten Daten automatisch.", note));
    layout->addWidget(note);

    QVBoxLayout* body = nullptr;
    sectionCard(layout, "Fähigkeiten", "personal.abilities", "Fähigkeiten randomisieren",
                "Jedes Pokémon bekommt zufällige Fähigkeiten.", &p.randomizeAbilities, body);
    bindCheck(body, "personal.banWonderGuard", "Wunderwache verbieten",
              "Wunderwache macht ein Pokémon gegen fast alles immun und kann Kämpfe unschaffbar machen.", &p.banWonderGuard);
    bindCheck(body, "personal.banExit", "Reißaus und Notausgang verbieten",
              "Diese Fähigkeiten lassen Pokémon bei wenig KP aus dem Kampf fliehen, was vor allem bei Wildpokémon nervt.",
              &p.banExitAbilities);

    sectionCard(layout, "Typen", "personal.types", "Typen randomisieren",
                "Jedes Pokémon bekommt zufällige Typen.", &p.randomizeTypes, body,
                [this](bool on) { code.svRandomizerWilds.typesChanged = on; });
    bindCheck(body, "personal.extraTypes", "Zusätzliche Typen vergeben",
              "Pokémon mit nur einem Typ bekommen mit 60 % Wahrscheinlichkeit einen zweiten Typ.", &p.grantExtraTypes);

    sectionCard(layout, "Attacken", "personal.moves", "Lernbare Attacken randomisieren",
                "Pokémon lernen zufällige Attacken beim Levelaufstieg.", &p.randomizeMoveset, body);
    body->addWidget(mutedLabel("Tipp: Kombiniert mit „Starke Movesets“ bei den Trainern wählen Trainer-Pokémon "
                               "die besten ihrer neuen Attacken.", content));

    sectionCard(layout, "Basiswerte", "personal.bst", "Basiswerte randomisieren",
                "Die Statuswerte (KP, Angriff, Verteidigung …) werden neu verteilt.", &p.randomizeBST, body);
    bindCheck(body, "personal.keepBst", "Gesamtwert beibehalten",
              "Die Summe aller Basiswerte bleibt gleich, nur die Verteilung ändert sich. So bleiben starke Pokémon stark.",
              &p.keepSameBST);

    sectionCard(layout, "Entwicklungen", "personal.evos", "Entwicklungen randomisieren",
                "Pokémon entwickeln sich in zufällige andere Pokémon.", &p.randomizeEvolutions, body);
    bindCheck(body, "personal.evoEveryLevel", "Bei jedem Level entwickeln",
              "Chaos-Modus: Pokémon entwickeln sich bei jedem Levelaufstieg in ein zufälliges Pokémon.", &p.evolveEveryLevel);

    auto* fixCard = new Card("Entwicklungen erleichtern");
    bindCheck(fixCard->body(), "personal.fixEvos", "Tausch- und Sonderentwicklungen anpassen",
              "Pokémon, die sich nur durch Tausch oder unter besonderen Bedingungen entwickeln (z. B. Kadabra, Maschock), "
              "entwickeln sich stattdessen per Level. Regionale Formen hängen dann von der Tageszeit ab.", &p.fixEvolutions);
    layout->addWidget(fixCard);

    sectionCard(layout, "TMs", "personal.tms", "TMs randomisieren",
                "Die Attacken auf den Technischen Maschinen werden zufällig.", &p.randomizeTMs, body);
    bindCheck(body, "personal.tmNoAnim", "Auch Attacken ohne Animation erlauben",
              "Erlaubt auch Attacken, die das Spiel nicht vorgesehen hat. Sie funktionieren, zeigen aber keine Animation.",
              &p.noAnimationTMs);

    return wrapPage("Pokémon-Daten", "Fähigkeiten, Typen, Attacken, Werte, Entwicklungen und TMs.", content);
}

// ------------------------------------------------------------------- Items

QWidget* ModernRandomizerWindow::buildItemsPage() {
    svItems& it = code.svRandomizerItems;
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QVBoxLayout* body = nullptr;
    sectionCard(layout, "Items", "items.randomize", "Items randomisieren",
                "Schaltet die Item-Randomisierung ein. Darunter wählst du, welche Items betroffen sind.",
                &it.randomizeItems, body);
    bindCheck(body, "items.hidden", "Versteckte Items",
              "Unsichtbare Items, die du an bestimmten Stellen findest.", &it.randomizeHiddenItems);
    bindCheck(body, "items.pickup", "Herumliegende Items",
              "Items, die sichtbar in der Spielwelt liegen und die du aufheben kannst.", &it.randomizePickUpItems);
    bindCheck(body, "items.drops", "Pokémon-Materialien",
              "Die Materialien, die Pokémon nach einem Kampf fallen lassen.", &it.randomizePokemonDrops);
    bindCheck(body, "items.letsgo", "Items aus Auto-Kämpfen",
              "Items, die dein Pokémon im „Los geht’s!“-Modus findet.", &it.randomizeLetsGoItems);

    return wrapPage("Items", "Lege fest, welche Items in der Spielwelt zufällig werden.", content);
}

// --------------------------------------------------------- Raids & Bosse

QWidget* ModernRandomizerWindow::buildRaidsBossesPage() {
    svBoss& b = code.svRandomizerBosses;
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QVBoxLayout* body = nullptr;
    sectionCard(layout, "Bosskämpfe", "bosses.randomize", "Bosskämpfe randomisieren",
                "Story-Kämpfe gegen besondere Pokémon: Herrscher-Pokémon, die Schätze des Unheils, die Kämpfe in "
                "Area Zero, Gierspenst, die Pokémon im Intro und weitere.", &b.randomize_bosses, body);
    bindLimiter(body, "bosses.allowed", &b.BossLimiter);

    auto* raids = new Card("Tera-Raids", "noch nicht verfügbar");
    raids->body()->addWidget(mutedLabel("Die Randomisierung von Tera-Raids ist im ursprünglichen Randomizer unfertig und wurde "
                                        "dort nie ausgeführt. Sie kann in einem späteren Schritt neu gebaut werden.", raids));
    layout->addWidget(raids);

    return wrapPage("Raids und Bosse", "Besondere Kämpfe außerhalb der normalen Trainer.", content);
}

// --------------------------------------------------------------- Statusleiste

QStringList ModernRandomizerWindow::activeAreas() const {
    QStringList areas;
    const svStarters& st = code.svRandomizerStarters;
    const svPersonal& p = code.svRandomizerPersonal;
    const svWilds& w = code.svRandomizerWilds;
    if (st.randomizeStarters) areas << "Starter";
    if (st.randomizeGifts) areas << "Geschenke";
    if (w.randomizePaldeaWild || w.randomizeKitakamiWild || w.randomizeBlueberryWild) areas << "Wilde Pokémon";
    if (code.svRandomizerFixed.randomizeFixedEncounters) areas << "Statische";
    if (p.randomizeAbilities || p.randomizeTypes || p.randomizeMoveset || p.randomizeBST ||
        p.randomizeEvolutions || p.randomizeTMs || p.fixEvolutions) areas << "Pokémon-Daten";
    if (code.svRandomizerItems.randomizeItems) areas << "Items";
    if (trainersEnabled && (regionPaldea || regionKitakami || regionBlueberry)) areas << "Trainer";
    if (code.svRandomizerBosses.randomize_bosses) areas << "Bosse";
    return areas;
}

void ModernRandomizerWindow::updateStatus() {
    updateSeedInfo();
    if (activeInfo == nullptr) {
        return;
    }
    QStringList areas = activeAreas();
    activeInfo->setText(areas.isEmpty() ? "Noch nichts ausgewählt" : "Aktiv: " + areas.join(", "));
}
