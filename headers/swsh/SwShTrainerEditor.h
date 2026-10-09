#ifndef SWSHTRAINEREDITOR_H
#define SWSHTRAINEREDITOR_H

// ---------------------------------------------------------------------------
// SwShTrainerEditor: bearbeitet ein swsh::TrainerOptions-Objekt
// (Grundeinstellungen oder "Eigene" Einstellungen einer Gruppe).
// ---------------------------------------------------------------------------

#include <QCheckBox>
#include <QSpinBox>
#include <QWidget>

#include "../modern_ui/modern_widgets.h"
#include "swsh_trainers.h"

class SwShTrainerEditor : public QWidget {
    Q_OBJECT
public:
    // options muss so lange leben wie der Editor
    explicit SwShTrainerEditor(swsh::TrainerOptions* options, QWidget* parent = nullptr);
    void refresh();

signals:
    void changed();

private:
    swsh::TrainerOptions* o;

    modernui::SegmentedControl* teamSize = nullptr;
    QCheckBox* fullyEvolved = nullptr;
    QSpinBox* fullyEvolvedLevel = nullptr;
    QCheckBox* levelEvos = nullptr;
    QCheckBox* smartMoves = nullptr;
    QCheckBox* smartTMs = nullptr;
    QCheckBox* gigantamax = nullptr;
    QCheckBox* legendaries = nullptr;
    QCheckBox* smartAI = nullptr;
    QCheckBox* perfectIVs = nullptr;
    QCheckBox* shinies = nullptr;

    QCheckBox* addCheck(QVBoxLayout* layout, const QString& text, const QString& info, bool* target);
    void updateEnabledStates();
};

#endif // SWSHTRAINEREDITOR_H
