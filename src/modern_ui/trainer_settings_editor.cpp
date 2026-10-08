#include "headers/modern_ui/trainer_settings_editor.h"

#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QJsonArray>
#include <memory>
#include <vector>

using namespace modernui;

namespace {

const QString kInfoTeamSize =
    "Original: Trainer behalten ihre Teamgröße.\n"
    "+ Zufällig: Trainer bekommen zufällig zusätzliche Pokémon (bis maximal 6).\n"
    "Immer 6: Jeder Trainer hat ein volles Team.";
const QString kInfoBattle =
    "Original: Einzel- und Doppelkämpfe bleiben wie im Spiel.\n"
    "Zufällig: Jeder Kampf wird zufällig ein Einzel- oder Doppelkampf.\n"
    "Alle Doppel: Wo möglich, werden alle Kämpfe zu Doppelkämpfen.";
const QString kInfoFullyEvolved =
    "Trainer-Pokémon ab diesem Level sind immer in ihrer Endstufe oder haben gar keine Entwicklung.";
const QString kInfoLevelEvos =
    "Pokémon tauchen erst auf, wenn sie auf diesem Level schon entwickelt sein könnten (z. B. kein Garados vor Level 20). "
    "Bei Entwicklungen per Item, Freundschaft oder Tausch wird ein sinnvolles Level geschätzt.";
const QString kInfoSmartMoves =
    "Statt der letzten vier gelernten Attacken bekommt jedes Pokémon die stärksten passenden Attacken – "
    "inklusive Attacken der Vorentwicklungen, mit Typ-Bonus, Typ-Abdeckung und einer nützlichen Statusattacke.";
const QString kInfoSmartTMs =
    "Starke Movesets dürfen auch Attacken enthalten, die das Pokémon nur per TM lernen kann.";
const QString kInfoAllowTera =
    "Alle Trainer dürfen im Kampf terakristallisieren, nicht nur Arenaleiter und andere wichtige Trainer.";
const QString kInfoRandomTera =
    "Die Tera-Typen der Trainer-Pokémon werden zufällig gewählt. Ohne diese Option behalten Typ-Trainer ihren Arenatyp als Tera-Typ.";
const QString kInfoSmartAI =
    "Trainer nutzen die stärkste Kampf-KI des Spiels, setzen Items ein und wechseln Pokémon taktisch aus.";
const QString kInfoPerfectIVs =
    "Alle Trainer-Pokémon bekommen in jedem Wert die bestmöglichen DVs (31).";
const QString kInfoShinies =
    "Trainer-Pokémon können schillernd sein (etwa jedes zweite).";

} // namespace

TrainerSettingsEditor::TrainerSettingsEditor(trainerSettings* settings, QWidget* parent)
    : QWidget(parent), s(settings) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // --- Teamgroesse ---
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Teamgröße", this));
        row->addWidget(new InfoButton(kInfoTeamSize, this));
        row->addStretch();
        teamSize = new SegmentedControl({"Original", "+ Zufällig", "Immer 6"}, this);
        row->addWidget(teamSize);
        layout->addLayout(row);
        connect(teamSize, &SegmentedControl::changed, this, [this](int index) {
            s->extraPokemon = (index == 1);
            s->force6Pokemon = (index == 2);
            emit changed();
        });
    }

    // --- Kampfart ---
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Kampfart", this));
        row->addWidget(new InfoButton(kInfoBattle, this));
        row->addStretch();
        battleType = new SegmentedControl({"Original", "Zufällig", "Alle Doppel"}, this);
        row->addWidget(battleType);
        layout->addLayout(row);
        connect(battleType, &SegmentedControl::changed, this, [this](int index) {
            s->singlesOrDoubles = (index == 1);
            s->allDoubles = (index == 2);
            emit changed();
        });
    }

    // --- Vollentwickelt ab Level ---
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        fullyEvolved = new QCheckBox("Nur vollentwickelte Pokémon ab Level", this);
        fullyEvolvedLevel = new QSpinBox(this);
        fullyEvolvedLevel->setRange(1, 100);
        fullyEvolvedLevel->setValue(40);
        fullyEvolvedLevel->setFixedWidth(60);
        fullyEvolvedLevel->setButtonSymbols(QAbstractSpinBox::NoButtons);
        fullyEvolvedLevel->setAlignment(Qt::AlignCenter);
        row->addWidget(fullyEvolved);
        row->addWidget(fullyEvolvedLevel);
        row->addWidget(new InfoButton(kInfoFullyEvolved, this));
        row->addStretch();
        layout->addLayout(row);
        connect(fullyEvolved, &QCheckBox::toggled, this, [this](bool checked) {
            s->fullyEvolvedLevel = checked ? fullyEvolvedLevel->value() : 0;
            updateEnabledStates();
            emit changed();
        });
        connect(fullyEvolvedLevel, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
            if (fullyEvolved->isChecked()) {
                s->fullyEvolvedLevel = value;
                emit changed();
            }
        });
    }

    levelEvos = addCheck(layout, "Level-passende Entwicklungen", kInfoLevelEvos, &s->levelAppropriateEvos);

    // --- Movesets ---
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        smartMoves = new QCheckBox("Starke Movesets", this);
        smartTMs = new QCheckBox("TM-Attacken erlauben", this);
        row->addWidget(smartMoves);
        row->addWidget(new InfoButton(kInfoSmartMoves, this));
        row->addSpacing(18);
        row->addWidget(smartTMs);
        row->addWidget(new InfoButton(kInfoSmartTMs, this));
        row->addStretch();
        layout->addLayout(row);
        connect(smartMoves, &QCheckBox::toggled, this, [this](bool checked) {
            s->smartMovesets = checked;
            updateEnabledStates();
            emit changed();
        });
        connect(smartTMs, &QCheckBox::toggled, this, [this](bool checked) {
            s->smartMovesTMs = checked;
            emit changed();
        });
    }

    // --- Tera ---
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        allowTera = new QCheckBox("Alle dürfen terakristallisieren", this);
        randomTera = new QCheckBox("Tera-Typen zufällig", this);
        row->addWidget(allowTera);
        row->addWidget(new InfoButton(kInfoAllowTera, this));
        row->addSpacing(18);
        row->addWidget(randomTera);
        row->addWidget(new InfoButton(kInfoRandomTera, this));
        row->addStretch();
        layout->addLayout(row);
        connect(allowTera, &QCheckBox::toggled, this, [this](bool checked) { s->allowTera = checked; emit changed(); });
        connect(randomTera, &QCheckBox::toggled, this, [this](bool checked) { s->randomizeTeras = checked; emit changed(); });
    }

    // --- Mehr Optionen (eingeklappt) ---
    auto* more = new Collapsible("Mehr Optionen: Kluge KI, perfekte DVs, Schillernde, erlaubte Pokémon", false, this);
    layout->addWidget(more);
    QVBoxLayout* m = more->body();

    smartAI = addCheck(m, "Kluge KI", kInfoSmartAI, &s->makeAISmart);
    perfectIVs = addCheck(m, "Perfekte DVs", kInfoPerfectIVs, &s->forcePerfectIV);
    shinies = addCheck(m, "Schillernde Pokémon erlauben", kInfoShinies, &s->allowShinies);

    // Erlaubte Pokemon
    limiterEditor = new LimiterEditor(&s->allowedPokemons, this);
    m->addWidget(limiterEditor);
    connect(limiterEditor, &LimiterEditor::changed, this, &TrainerSettingsEditor::changed);

    refresh();
}

QCheckBox* TrainerSettingsEditor::addCheck(QVBoxLayout* layout, const QString& text, const QString& info, bool* target) {
    auto* box = new QCheckBox(text, this);
    layout->addLayout(rowWithInfo(box, info));
    connect(box, &QCheckBox::toggled, this, [this, target](bool checked) {
        *target = checked;
        emit changed();
    });
    return box;
}

void TrainerSettingsEditor::updateEnabledStates() {
    fullyEvolvedLevel->setEnabled(fullyEvolved->isChecked());
    smartTMs->setEnabled(smartMoves->isChecked());
}

void TrainerSettingsEditor::refresh() {
    // Waehrend des Setzens keine Signale, sonst wuerden Werte zurueckgeschrieben
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (QWidget* w : findChildren<QWidget*>()) {
        blockers.push_back(std::make_unique<QSignalBlocker>(w));
    }

    teamSize->setCurrent(s->force6Pokemon ? 2 : (s->extraPokemon ? 1 : 0));
    battleType->setCurrent(s->allDoubles ? 2 : (s->singlesOrDoubles ? 1 : 0));
    fullyEvolved->setChecked(s->fullyEvolvedLevel > 0);
    if (s->fullyEvolvedLevel > 0) {
        fullyEvolvedLevel->setValue(s->fullyEvolvedLevel);
    }
    levelEvos->setChecked(s->levelAppropriateEvos);
    smartMoves->setChecked(s->smartMovesets);
    smartTMs->setChecked(s->smartMovesTMs);
    allowTera->setChecked(s->allowTera);
    randomTera->setChecked(s->randomizeTeras);
    smartAI->setChecked(s->makeAISmart);
    perfectIVs->setChecked(s->forcePerfectIV);
    shinies->setChecked(s->allowShinies);
    limiterEditor->refresh();

    blockers.clear();
    updateEnabledStates();
}

// ------------------------------ JSON (Vorlagen) -----------------------------

namespace {
QJsonArray boolList(const QList<bool>& list) {
    QJsonArray array;
    for (bool b : list) {
        array.append(b);
    }
    return array;
}
void readBoolList(const QJsonValue& value, QList<bool>& list) {
    if (!value.isArray()) {
        return;
    }
    QJsonArray array = value.toArray();
    for (int i = 0; i < array.size() && i < list.size(); i++) {
        list[i] = array[i].toBool(list[i]);
    }
}
} // namespace

QJsonObject trainerSettingsToJson(const trainerSettings& s) {
    QJsonObject o;
    o["allowTera"] = s.allowTera;
    o["randomizeTeras"] = s.randomizeTeras;
    o["singlesOrDoubles"] = s.singlesOrDoubles;
    o["allDoubles"] = s.allDoubles;
    o["allowShinies"] = s.allowShinies;
    o["force6Pokemon"] = s.force6Pokemon;
    o["extraPokemon"] = s.extraPokemon;
    o["forcePerfectIV"] = s.forcePerfectIV;
    o["makeAISmart"] = s.makeAISmart;
    o["fullyEvolvedLevel"] = s.fullyEvolvedLevel;
    o["levelAppropriateEvos"] = s.levelAppropriateEvos;
    o["smartMovesets"] = s.smartMovesets;
    o["smartMovesTMs"] = s.smartMovesTMs;

    o["allowedPokemons"] = limiterToJson(s.allowedPokemons);
    return o;
}

void trainerSettingsFromJson(const QJsonObject& o, trainerSettings& s) {
    s.allowTera = o["allowTera"].toBool(s.allowTera);
    s.randomizeTeras = o["randomizeTeras"].toBool(s.randomizeTeras);
    s.singlesOrDoubles = o["singlesOrDoubles"].toBool(s.singlesOrDoubles);
    s.allDoubles = o["allDoubles"].toBool(s.allDoubles);
    s.allowShinies = o["allowShinies"].toBool(s.allowShinies);
    s.force6Pokemon = o["force6Pokemon"].toBool(s.force6Pokemon);
    s.extraPokemon = o["extraPokemon"].toBool(s.extraPokemon);
    s.forcePerfectIV = o["forcePerfectIV"].toBool(s.forcePerfectIV);
    s.makeAISmart = o["makeAISmart"].toBool(s.makeAISmart);
    s.fullyEvolvedLevel = o["fullyEvolvedLevel"].toInt(s.fullyEvolvedLevel);
    s.levelAppropriateEvos = o["levelAppropriateEvos"].toBool(s.levelAppropriateEvos);
    s.smartMovesets = o["smartMovesets"].toBool(s.smartMovesets);
    s.smartMovesTMs = o["smartMovesTMs"].toBool(s.smartMovesTMs);

    limiterFromJson(o["allowedPokemons"].toObject(), s.allowedPokemons);
}

// ------------------------------- LimiterEditor ------------------------------

LimiterEditor::LimiterEditor(allowedPokemonLimiter* limiter, QWidget* parent) : QWidget(parent), l(limiter) {
    auto* m = new QVBoxLayout(this);
    m->setContentsMargins(0, 0, 0, 0);
    m->setSpacing(6);

    auto* title = new QLabel("Erlaubte Pokémon", this);
    title->setObjectName("sectionLabel");
    auto* titleRow = new QHBoxLayout();
    titleRow->addWidget(title);
    titleRow->addWidget(new InfoButton("Legt fest, aus welchen Pokémon zufällig gewählt wird. Pokémon, die nicht im Spiel "
                                       "sind, werden automatisch ausgeschlossen. Legendäre werden über ihre Generation "
                                       "zugelassen oder ausgeschlossen.", this));
    titleRow->addStretch();
    m->addLayout(titleRow);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    grid->addWidget(new QLabel("Generationen", this), 0, 0);
    grid->addWidget(new QLabel("Legendäre aus Gen.", this), 1, 0);
    for (int g = 0; g < 9; g++) {
        auto* gen = new QCheckBox(QString::number(g + 1), this);
        auto* legend = new QCheckBox(QString::number(g + 1), this);
        gens.append(gen);
        legends.append(legend);
        grid->addWidget(gen, 0, g + 1);
        grid->addWidget(legend, 1, g + 1);
        connect(gen, &QCheckBox::toggled, this, [this, g](bool checked) {
            l->gens[g] = checked;
            emit changed();
        });
        connect(legend, &QCheckBox::toggled, this, [this, g](bool checked) {
            l->genLegends[g] = checked;
            emit changed();
        });
    }
    grid->setColumnStretch(10, 1);
    m->addLayout(grid);

    auto* stages = new QHBoxLayout();
    stages->setSpacing(14);
    auto addStage = [&](const QString& text, bool* target) {
        auto* box = new QCheckBox(text, this);
        stages->addWidget(box);
        connect(box, &QCheckBox::toggled, this, [this, target](bool checked) {
            *target = checked;
            emit changed();
        });
        return box;
    };
    stage1 = addStage("Basis-Pokémon", &l->stage1);
    stage2 = addStage("Mittelstufe", &l->stage2);
    stage3 = addStage("Endstufe", &l->stage3);
    singleStage = addStage("Ohne Entwicklung", &l->singleStage);
    paradox = addStage("Paradox-Pokémon", &l->paradox);
    stages->addStretch();
    m->addLayout(stages);

    refresh();
}

void LimiterEditor::refresh() {
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (QWidget* w : findChildren<QWidget*>()) {
        blockers.push_back(std::make_unique<QSignalBlocker>(w));
    }
    for (int g = 0; g < 9; g++) {
        gens[g]->setChecked(g < l->gens.size() && l->gens[g]);
        legends[g]->setChecked(g < l->genLegends.size() && l->genLegends[g]);
    }
    stage1->setChecked(l->stage1);
    stage2->setChecked(l->stage2);
    stage3->setChecked(l->stage3);
    singleStage->setChecked(l->singleStage);
    paradox->setChecked(l->paradox);
}

QJsonObject limiterToJson(const allowedPokemonLimiter& l) {
    QJsonObject allowed;
    allowed["gens"] = boolList(l.gens);
    allowed["genLegends"] = boolList(l.genLegends);
    allowed["stage1"] = l.stage1;
    allowed["stage2"] = l.stage2;
    allowed["stage3"] = l.stage3;
    allowed["singleStage"] = l.singleStage;
    allowed["paradox"] = l.paradox;
    return allowed;
}

void limiterFromJson(const QJsonObject& allowed, allowedPokemonLimiter& l) {
    readBoolList(allowed["gens"], l.gens);
    readBoolList(allowed["genLegends"], l.genLegends);
    l.stage1 = allowed["stage1"].toBool(l.stage1);
    l.stage2 = allowed["stage2"].toBool(l.stage2);
    l.stage3 = allowed["stage3"].toBool(l.stage3);
    l.singleStage = allowed["singleStage"].toBool(l.singleStage);
    l.paradox = allowed["paradox"].toBool(l.paradox);
}
