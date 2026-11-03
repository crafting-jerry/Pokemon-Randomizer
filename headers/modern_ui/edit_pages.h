#ifndef MODERN_EDIT_PAGES_H
#define MODERN_EDIT_PAGES_H

// ---------------------------------------------------------------------------
// Bearbeiten-Modus der neuen Oberflaeche (fuer alle Spiele gleich):
//  - EditWorkspace: Seitenleiste mit den Reitern (Trainer, Wilde Pokemon ...)
//    und rechts die Seite des gewaehlten Reiters
//  - EditSectionPage: links eine durchsuchbare Liste der Eintraege, rechts
//    die Felder des gewaehlten Eintrags
// Die Daten kommen als edit::Section aus dem jeweiligen Spiel (common/edit_model.h).
// Geaenderte Werte werden markiert, der Vorher-Wert steht im Tooltip.
// ---------------------------------------------------------------------------

#include <QHash>
#include <QList>
#include <QPointer>
#include <QWidget>

#include "../common/edit_model.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QScrollArea;
class QStackedWidget;
class QStringListModel;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QGridLayout;

namespace modernui {

// Gemeinsame Auswahllisten (ein Modell fuer alle Auswahlfelder einer Art)
class EditPickers : public QObject {
    Q_OBJECT
public:
    explicit EditPickers(const edit::Catalog& catalog, QObject* parent = nullptr);
    const edit::Catalog& catalog() const { return data; }
    QStringListModel* model(edit::Kind kind);
    int rowOf(edit::Kind kind, int value);          // -1 = nicht in der Liste
    int valueAt(edit::Kind kind, int row);

private:
    struct List {
        QStringListModel* model = nullptr;
        QList<int> values;
        QHash<int, int> rows;
    };
    edit::Catalog data;
    QHash<int, List> lists;
    List& ensure(edit::Kind kind);
};

class EditSectionPage : public QWidget {
    Q_OBJECT
public:
    EditSectionPage(const edit::Section& section, EditPickers* pickers, QWidget* parent = nullptr);
    void refreshMarks();   // Markierung geaenderter Eintraege aktualisieren

signals:
    void edited();

private:
    edit::Section section;
    EditPickers* pickers;
    QLineEdit* searchEdit = nullptr;
    QCheckBox* onlyChanged = nullptr;
    QTreeWidget* tree = nullptr;
    QLabel* countLabel = nullptr;
    QScrollArea* detailScroll = nullptr;
    QPointer<QWidget> detail;
    int currentEntry = -1;
    QList<QTreeWidgetItem*> entryItems;

    void buildList();
    void applyFilter();
    void showEntry(int index);
    void scheduleRebuild();
    void markItem(int index);
    QWidget* fieldWidget(const edit::Field& field, QWidget* parent);
    void addFieldGrid(QGridLayout* grid, const QList<edit::Field>& fields, int perLine, QWidget* parent, int startRow);
    void updateEditedState(QWidget* widget, const edit::Field& field);
};

class EditWorkspace : public QWidget {
    Q_OBJECT
public:
    explicit EditWorkspace(QWidget* parent = nullptr);
    // Baut alle Seiten neu auf (nach dem Randomisieren oder Laden des Originals)
    void setSections(const QList<edit::Section>& sections, const edit::Catalog& catalog);
    void clear();
    // Liste der Reiter fuer die Seitenleiste des Fensters
    QListWidget* navigation() const { return nav; }
    bool isActive() const { return !pages.isEmpty(); }

signals:
    void edited();

private:
    QListWidget* nav = nullptr;
    QStackedWidget* stack = nullptr;
    EditPickers* pickers = nullptr;
    QList<EditSectionPage*> pages;
};

} // namespace modernui

#endif // MODERN_EDIT_PAGES_H
