#ifndef TRAINER_SETTINGS_EDITOR_H
#define TRAINER_SETTINGS_EDITOR_H

// ---------------------------------------------------------------------------
// TrainerSettingsEditor
// Bearbeitet genau ein trainerSettings-Objekt. Wird fuer die Grundeinstellungen
// und fuer jede Gruppe mit "Eigene" Einstellungen verwendet.
// ---------------------------------------------------------------------------

#include <QWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QList>
#include <QJsonObject>
#include "modern_widgets.h"
#include "../qtwindows_headers/sharedRandomizerClass.h"

using trainerSettings = sharedRandomizerClass::trainerSettings;
using allowedPokemonLimiter = sharedRandomizerClass::allowedPokemonLimiter;

// ---------------------------------------------------------------------------
// LimiterEditor: Auswahl der erlaubten Pokemon (Generationen, Legendaere,
// Entwicklungsstufen, Paradox). Wird auf allen Seiten wiederverwendet.
// ---------------------------------------------------------------------------
class LimiterEditor : public QWidget {
    Q_OBJECT
public:
    explicit LimiterEditor(allowedPokemonLimiter* limiter, QWidget* parent = nullptr);
    void refresh();
signals:
    void changed();
private:
    allowedPokemonLimiter* l;
    QList<QCheckBox*> gens;
    QList<QCheckBox*> legends;
    QCheckBox* stage1 = nullptr;
    QCheckBox* stage2 = nullptr;
    QCheckBox* stage3 = nullptr;
    QCheckBox* singleStage = nullptr;
    QCheckBox* paradox = nullptr;
};

QJsonObject limiterToJson(const allowedPokemonLimiter& l);
void limiterFromJson(const QJsonObject& o, allowedPokemonLimiter& l);

class TrainerSettingsEditor : public QWidget {
    Q_OBJECT
public:
    // settings muss so lange leben wie der Editor
    explicit TrainerSettingsEditor(trainerSettings* settings, QWidget* parent = nullptr);

    // Widgets aus den aktuellen Werten neu setzen (z. B. nach dem Laden einer Vorlage)
    void refresh();

signals:
    void changed();

private:
    trainerSettings* s;

    modernui::SegmentedControl* teamSize = nullptr;
    modernui::SegmentedControl* battleType = nullptr;
    QCheckBox* fullyEvolved = nullptr;
    QSpinBox* fullyEvolvedLevel = nullptr;
    QCheckBox* levelEvos = nullptr;
    QCheckBox* smartMoves = nullptr;
    QCheckBox* smartTMs = nullptr;
    QCheckBox* allowTera = nullptr;
    QCheckBox* randomTera = nullptr;
    QCheckBox* smartAI = nullptr;
    QCheckBox* perfectIVs = nullptr;
    QCheckBox* shinies = nullptr;
    LimiterEditor* limiterEditor = nullptr;

    QCheckBox* addCheck(QVBoxLayout* layout, const QString& text, const QString& info, bool* target);
    void updateEnabledStates();
};

// Speichern und Laden (Vorlagen)
QJsonObject trainerSettingsToJson(const trainerSettings& s);
void trainerSettingsFromJson(const QJsonObject& o, trainerSettings& s);

#endif // TRAINER_SETTINGS_EDITOR_H
