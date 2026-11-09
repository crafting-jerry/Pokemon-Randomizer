#include "headers/swsh/SwShTrainerEditor.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <memory>
#include <vector>

using namespace modernui;

namespace {
const QString kInfoTeamSize =
    "Original: Trainer behalten ihre Teamgröße.\n"
    "+ Zufällig: Trainer bekommen zufällig zusätzliche Pokémon (bis maximal 6).\n"
    "Immer 6: Jeder Trainer hat ein volles Team.\n"
    "Bei Doppel- und Multikämpfen mit Partner bleibt es bei höchstens 3 Pokémon pro Trainer.";
const QString kInfoFullyEvolved =
    "Trainer-Pokémon ab diesem Level sind immer in ihrer Endstufe oder haben gar keine Entwicklung.";
const QString kInfoLevelEvos =
    "Pokémon tauchen erst auf, wenn sie auf diesem Level schon entwickelt sein könnten (z. B. kein Garados vor "
    "Level 20). Bei Entwicklungen per Item, Freundschaft oder Tausch wird ein sinnvolles Level geschätzt.";
const QString kInfoSmartMoves =
    "Pokémon behalten ihre normalen Attacken (die letzten vier gelernten), solange diese für ihr Level stark "
    "genug sind. Ist das Moveset zu schwach (z. B. Roserade mit Absorber in der Liga), werden nur die schwachen "
    "Attacken durch stärkere ersetzt, die zum Level passen – inklusive Attacken der Vorentwicklungen, mit "
    "Typ-Bonus und Typ-Abdeckung.";
const QString kInfoSmartTMs =
    "Starke Movesets dürfen auch Attacken enthalten, die das Pokémon nur per TM oder TP lernt. Starke "
    "Attacken gibt es erst auf höherem Level, Statusattacken ab Level 20.";
const QString kInfoSmartTMsNoTR =
    "Starke Movesets dürfen auch Attacken enthalten, die das Pokémon nur per TM lernt. Starke "
    "Attacken gibt es erst auf höherem Level, Statusattacken ab Level 20.";
const QString kInfoGigantamax =
    "Pokémon, die im Original gigadynamaximieren (z. B. die Asse der Arenaleiter), werden durch ein Pokémon mit "
    "Gigadynamax-Form ersetzt – passend zum Typ der Arena, wenn möglich.";
const QString kInfoLegendaries =
    "Chance für jedes Trainer-Pokémon, ein Legendäres oder Mysteriöses Pokémon bzw. eine Ultrabestie zu sein. "
    "0 % = nie, 100 % = nur Legendäre. Dezimalwerte wie 2,5 % sind möglich. Passt kein Legendäres (Typ, Level "
    "oder schon im Team), wird ein normales Pokémon gewählt.";
const QString kInfoSmartAI =
    "Trainer nutzen die stärkste Kampf-KI des Spiels und wechseln Pokémon taktisch aus.";
const QString kInfoPerfectIVs =
    "Alle Trainer-Pokémon bekommen in jedem Wert die bestmöglichen DVs (31).";
const QString kInfoShinies =
    "Chance für jedes Trainer-Pokémon, schillernd zu sein. 0 % = nie (bis auf Pokémon, die schon im Original "
    "schillern), 100 % = alle. Dezimalwerte wie 0,5 % sind möglich.";
const QString kInfoCompetitive =
    "Trainer, deren höchstes Pokémon mindestens dieses Level hat, bekommen ein Competitive-Team (das ganze Team):\n"
    "• Attacken ohne Level-Grenze aus allen Level- und TM-Attacken: starke Angriffe mit Typ-Bonus und guter "
    "Typ-Abdeckung, dazu je nach Pokémon Setup-, Heil- oder Statusattacken. In Doppelkämpfen auch Schutzschild, "
    "Mogelhieb und Rückenwind.\n"
    "• Ein passendes Item (z. B. Wahlband, Leben-Orb, Überreste, Fokusgurt) – jedes Item nur einmal pro Team.\n"
    "• Die beste Fähigkeit (auch versteckte Fähigkeiten), passendes Wesen, Fleißpunkte (252/252/4) und perfekte DVs.\n"
    "• Die stärkste Kampf-KI des Spiels wird automatisch eingeschaltet.\n"
    "Ersetzt für diese Trainer die Option „Starke Movesets“. Wie die KI kämpft, legt das Spiel selbst fest – "
    "getragene Items wirken im Kampf aber immer.";
} // namespace

SwShTrainerEditor::SwShTrainerEditor(swsh::TrainerOptions* options, QWidget* parent, bool galar)
    : QWidget(parent), o(options) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Teamgroesse
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Teamgröße", this));
        row->addWidget(new InfoButton(kInfoTeamSize, this));
        row->addStretch();
        teamSize = new SegmentedControl({"Original", "+ Zufällig", "Immer 6"}, this);
        row->addWidget(teamSize);
        layout->addLayout(row);
        connect(teamSize, &SegmentedControl::changed, this, [this](int index) {
            o->teamSize = index;
            emit changed();
        });
    }

    // Vollentwickelt ab Level
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
            o->fullyEvolvedLevel = checked ? fullyEvolvedLevel->value() : 0;
            updateEnabledStates();
            emit changed();
        });
        connect(fullyEvolvedLevel, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
            if (fullyEvolved->isChecked()) {
                o->fullyEvolvedLevel = value;
                emit changed();
            }
        });
    }

    levelEvos = addCheck(layout, "Level-passende Entwicklungen", kInfoLevelEvos, &o->levelAppropriateEvos);

    // Movesets
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        smartMoves = new QCheckBox("Starke Movesets", this);
        smartTMs = new QCheckBox(galar ? "TM- und TP-Attacken erlauben" : "TM-Attacken erlauben", this);
        row->addWidget(smartMoves);
        row->addWidget(new InfoButton(kInfoSmartMoves, this));
        row->addSpacing(18);
        row->addWidget(smartTMs);
        row->addWidget(new InfoButton(galar ? kInfoSmartTMs : kInfoSmartTMsNoTR, this));
        row->addStretch();
        layout->addLayout(row);
        connect(smartMoves, &QCheckBox::toggled, this, [this](bool checked) {
            o->smartMovesets = checked;
            updateEnabledStates();
            emit changed();
        });
        connect(smartTMs, &QCheckBox::toggled, this, [this](bool checked) {
            o->smartTMs = checked;
            emit changed();
        });
    }

    // Competitive-Modus ab Level
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        competitive = new QCheckBox("Competitive-Modus ab Level", this);
        competitiveLevel = new QSpinBox(this);
        competitiveLevel->setRange(1, 100);
        competitiveLevel->setValue(50);
        competitiveLevel->setFixedWidth(60);
        competitiveLevel->setButtonSymbols(QAbstractSpinBox::NoButtons);
        competitiveLevel->setAlignment(Qt::AlignCenter);
        row->addWidget(competitive);
        row->addWidget(competitiveLevel);
        row->addWidget(new InfoButton(kInfoCompetitive, this));
        row->addStretch();
        layout->addLayout(row);
        connect(competitive, &QCheckBox::toggled, this, [this](bool checked) {
            o->competitiveLevel = checked ? competitiveLevel->value() : 0;
            updateEnabledStates();
            emit changed();
        });
        connect(competitiveLevel, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
            if (competitive->isChecked()) {
                o->competitiveLevel = value;
                emit changed();
            }
        });
    }

    if (galar) {
        gigantamax = addCheck(layout, "Gigadynamax-Asse behalten", kInfoGigantamax, &o->keepGigantamax);
    } else {
        gigantamax = new QCheckBox(this); // nicht sichtbar, nur damit refresh() einheitlich bleibt
        gigantamax->hide();
    }

    auto* more = new Collapsible("Mehr Optionen: Legendäre, Schillernde, Kluge KI, perfekte DVs", false, this);
    layout->addWidget(more);
    legendaries = addPercentRow(more->body(), "Legendäre Pokémon", kInfoLegendaries, this);
    connect(legendaries, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        o->legendaryChance = value;
        emit changed();
    });
    shinies = addPercentRow(more->body(), "Schillernde Pokémon", kInfoShinies, this);
    connect(shinies, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        o->shinyChance = value;
        emit changed();
    });
    smartAI = addCheck(more->body(), "Kluge KI", kInfoSmartAI, &o->smartAI);
    perfectIVs = addCheck(more->body(), "Perfekte DVs", kInfoPerfectIVs, &o->perfectIVs);

    refresh();
}

QCheckBox* SwShTrainerEditor::addCheck(QVBoxLayout* layout, const QString& text, const QString& info, bool* target) {
    auto* box = new QCheckBox(text, this);
    layout->addLayout(rowWithInfo(box, info));
    connect(box, &QCheckBox::toggled, this, [this, target](bool checked) {
        *target = checked;
        emit changed();
    });
    return box;
}

void SwShTrainerEditor::updateEnabledStates() {
    fullyEvolvedLevel->setEnabled(fullyEvolved->isChecked());
    smartTMs->setEnabled(smartMoves->isChecked());
    competitiveLevel->setEnabled(competitive->isChecked());
}

void SwShTrainerEditor::refresh() {
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (QWidget* w : findChildren<QWidget*>()) {
        blockers.push_back(std::make_unique<QSignalBlocker>(w));
    }
    teamSize->setCurrent(o->teamSize);
    fullyEvolved->setChecked(o->fullyEvolvedLevel > 0);
    if (o->fullyEvolvedLevel > 0) {
        fullyEvolvedLevel->setValue(o->fullyEvolvedLevel);
    }
    levelEvos->setChecked(o->levelAppropriateEvos);
    smartMoves->setChecked(o->smartMovesets);
    smartTMs->setChecked(o->smartTMs);
    competitive->setChecked(o->competitiveLevel > 0);
    if (o->competitiveLevel > 0) {
        competitiveLevel->setValue(o->competitiveLevel);
    }
    gigantamax->setChecked(o->keepGigantamax);
    legendaries->setValue(o->legendaryChance);
    smartAI->setChecked(o->smartAI);
    perfectIVs->setChecked(o->perfectIVs);
    shinies->setValue(o->shinyChance);
    blockers.clear();
    updateEnabledStates();
}
