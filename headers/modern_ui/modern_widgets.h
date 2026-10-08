#ifndef MODERN_WIDGETS_H
#define MODERN_WIDGETS_H

// ---------------------------------------------------------------------------
// Wiederverwendbare Bausteine fuer die neue Oberflaeche:
//  - InfoButton:       kleines "i" mit Erklaerung (Hover oder Klick)
//  - SegmentedControl: Auswahl aus mehreren Optionen (statt mehrerer Checkboxen)
//  - Card:             Karte mit Titel und optionalem Hinweis-Label
//  - Collapsible:      einklappbarer Bereich
// ---------------------------------------------------------------------------

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>

namespace modernui {

class InfoButton : public QToolButton {
    Q_OBJECT
public:
    explicit InfoButton(const QString& text, QWidget* parent = nullptr);
private:
    QString infoText;
};

class SegmentedControl : public QWidget {
    Q_OBJECT
public:
    explicit SegmentedControl(const QStringList& options, QWidget* parent = nullptr);
    int current() const;
    void setCurrent(int index);
signals:
    void changed(int index);
private:
    QButtonGroup* group;
};

class Card : public QFrame {
    Q_OBJECT
public:
    explicit Card(const QString& title, const QString& badge = QString(), QWidget* parent = nullptr);
    QVBoxLayout* body() const { return bodyLayout; }
    QHBoxLayout* header() const { return headerLayout; }
private:
    QVBoxLayout* bodyLayout;
    QHBoxLayout* headerLayout;
};

class Collapsible : public QWidget {
    Q_OBJECT
public:
    explicit Collapsible(const QString& title, bool expanded = false, QWidget* parent = nullptr);
    QVBoxLayout* body() const { return bodyLayout; }
    void setExpanded(bool expanded);
private:
    QToolButton* toggle;
    QWidget* content;
    QVBoxLayout* bodyLayout;
};

// Zeile: [widget] [i]
QHBoxLayout* rowWithInfo(QWidget* widget, const QString& info, bool stretch = true);

// Kleines graues Label
QLabel* mutedLabel(const QString& text, QWidget* parent = nullptr);

// Komplettes Stylesheet der neuen Oberflaeche
QString styleSheet();

} // namespace modernui

#endif // MODERN_WIDGETS_H
