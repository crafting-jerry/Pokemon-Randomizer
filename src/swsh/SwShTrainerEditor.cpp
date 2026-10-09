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
    "Statt der letzten vier gelernten Attacken bekommt jedes Pokémon die stärksten passenden Attacken – "
    "inklusive Attacken der Vorentwicklungen, mit Typ-Bonus, Typ-Abdeckung und einer nützlichen Statusattacke.";
const QString kInfoSmartTMs =
    "Starke Movesets dürfen auch Attacken enthalten, die das Pokémon nur per TM oder TP lernt. Starke "
    "Attacken gibt es erst auf höherem Level, Statusattacken ab Level 20.";
const QString kInfoGigantamax =
    "Pokémon, die im Original gigadynamaximieren (z. B. die Asse der Arenaleiter), werden durch ein Pokémon mit "
    "Gigadynamax-Form ersetzt – passend zum Typ der Arena, wenn möglich.";
const QString kInfoLegendaries =
    "Legendäre und Mysteriöse Pokémon sowie Ultrabestien dürfen in Trainer-Teams vorkommen.";
const QString kInfoSmartAI =
    "Trainer nutzen die stärkste Kampf-KI des Spiels und wechseln Pokémon taktisch aus.";
const QString kInfoPerfectIVs =
    "Alle Trainer-Pokémon bekommen in jedem Wert die bestmöglichen DVs (31).";
const QString kInfoShinies =
    "Trainer-Pokémon können schillernd sein (etwa jedes zweite).";
} // namespace

SwShTrainerEditor::SwShTrainerEditor(swsh::TrainerOptions* options, QWidget* parent) : QWidget(parent), o(options) {
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
        smartTMs = new QCheckBox("TM- und TP-Attacken erlauben", this);
        row->addWidget(smartMoves);
        row->addWidget(new InfoButton(kInfoSmartMoves, this));
        row->addSpacing(18);
        row->addWidget(smartTMs);
        row->addWidget(new InfoButton(kInfoSmartTMs, this));
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

    gigantamax = addCheck(layout, "Gigadynamax-Asse behalten", kInfoGigantamax, &o->keepGigantamax);

    auto* more = new Collapsible("Mehr Optionen: Legendäre, Kluge KI, perfekte DVs, Schillernde", false, this);
    layout->addWidget(more);
    legendaries = addCheck(more->body(), "Legendäre Pokémon erlauben", kInfoLegendaries, &o->legendaries);
    smartAI = addCheck(more->body(), "Kluge KI", kInfoSmartAI, &o->smartAI);
    perfectIVs = addCheck(more->body(), "Perfekte DVs", kInfoPerfectIVs, &o->perfectIVs);
    shinies = addCheck(more->body(), "Schillernde Pokémon erlauben", kInfoShinies, &o->shinies);

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
    gigantamax->setChecked(o->keepGigantamax);
    legendaries->setChecked(o->legendaries);
    smartAI->setChecked(o->smartAI);
    perfectIVs->setChecked(o->perfectIVs);
    shinies->setChecked(o->shinies);
    blockers.clear();
    updateEnabledStates();
}
