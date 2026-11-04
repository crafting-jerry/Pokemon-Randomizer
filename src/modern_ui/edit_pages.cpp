#include "headers/modern_ui/edit_pages.h"
#include "headers/modern_ui/modern_widgets.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringListModel>
#include <QStyle>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace modernui {

namespace {

const char* kEditStyle = R"(
#editList { background: #171a23; border: 1px solid #2c3140; border-radius: 10px; outline: 0; padding: 4px; }
#editList::item { padding: 4px 6px; border-radius: 6px; }
#editList::item:selected { background: #2a2350; color: #d6ccff; }
#editList::item:hover:!selected { background: #20242f; }
#editCard { background: #1b1f29; border: 1px solid #2c3140; border-radius: 10px; }
#fieldLabel { color: #9097a8; font-size: 11px; }
#tableHead { color: #9097a8; font-size: 11px; font-weight: 600; }
#cardTitle { font-size: 13px; font-weight: 600; }
#removeButton { background: transparent; border: 1px solid #3a4052; border-radius: 7px; padding: 3px 8px; color: #9097a8; }
#removeButton:hover { border-color: #ff8a8a; color: #ff8a8a; }
QComboBox[edited="true"], QSpinBox[edited="true"] { border-color: #f2c46d; }
QCheckBox[edited="true"] { color: #f2c46d; }
#textValue { color: #e6e8ee; padding: 4px 0; }
#editWarning { background: #3a2f17; border: 1px solid #f2c46d; border-radius: 8px; color: #f6d896; padding: 8px 10px; }
)";

const QColor kChangedColor(0xf2, 0xc4, 0x6d);

// Aendert Werte nur, wenn das Feld den Fokus hat: sonst scrollt das Mausrad die Seite
class WheelGuard : public QObject {
public:
    explicit WheelGuard(QScrollArea* area, QObject* parent) : QObject(parent), scroll(area) {}
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Wheel) {
            auto* widget = qobject_cast<QWidget*>(watched);
            if (widget && !widget->hasFocus() && scroll) {
                QApplication::sendEvent(scroll->verticalScrollBar(), event);
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }
private:
    QPointer<QScrollArea> scroll;
};

QLabel* smallLabel(const QString& text, const char* name, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(name);
    return label;
}

} // namespace

// ---------------------------------------------------------------- EditPickers

EditPickers::EditPickers(const edit::Catalog& catalog, QObject* parent) : QObject(parent), data(catalog) {}

EditPickers::List& EditPickers::ensure(edit::Kind kind) {
    const int key = static_cast<int>(kind);
    List& list = lists[key];
    if (!list.model) {
        QStringList names;
        for (const auto& entry : data.list(kind)) {
            list.rows.insert(entry.first, static_cast<int>(list.values.size()));
            list.values.append(entry.first);
            names.append(entry.second);
        }
        list.model = new QStringListModel(names, this);
    }
    return list;
}

QStringListModel* EditPickers::model(edit::Kind kind) { return ensure(kind).model; }

int EditPickers::rowOf(edit::Kind kind, int value) { return ensure(kind).rows.value(value, -1); }

int EditPickers::valueAt(edit::Kind kind, int row) {
    const List& list = ensure(kind);
    return row >= 0 && row < list.values.size() ? list.values[row] : 0;
}

// ----------------------------------------------------------- EditSectionPage

EditSectionPage::EditSectionPage(const edit::Section& sectionIn, EditPickers* pickersIn, QWidget* parent)
    : QWidget(parent), section(sectionIn), pickers(pickersIn) {
    setStyleSheet(kEditStyle);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 12);
    root->setSpacing(16);

    // Links: Suche und Liste
    auto* left = new QWidget(this);
    left->setFixedWidth(290);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);
    auto* title = new QLabel(section.title, left);
    title->setObjectName("pageTitle");
    leftLayout->addWidget(title);
    if (!section.subtitle.isEmpty()) leftLayout->addWidget(mutedLabel(section.subtitle, left));
    searchEdit = new QLineEdit(left);
    searchEdit->setPlaceholderText("Suchen (Name, Pokémon, Ort …)");
    searchEdit->setClearButtonEnabled(true);
    leftLayout->addWidget(searchEdit);
    onlyChanged = new QCheckBox("Nur geänderte anzeigen", left);
    leftLayout->addWidget(onlyChanged);
    tree = new QTreeWidget(left);
    tree->setObjectName("editList");
    tree->setHeaderHidden(true);
    tree->setIndentation(10);
    tree->setUniformRowHeights(true);
    leftLayout->addWidget(tree, 1);
    countLabel = mutedLabel(QString(), left);
    leftLayout->addWidget(countLabel);
    root->addWidget(left);

    // Rechts: Details
    detailScroll = new QScrollArea(this);
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    root->addWidget(detailScroll, 1);

    buildList();
    connect(searchEdit, &QLineEdit::textChanged, this, &EditSectionPage::applyFilter);
    connect(onlyChanged, &QCheckBox::toggled, this, &EditSectionPage::applyFilter);
    connect(tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (!item) return;
        const QVariant data = item->data(0, Qt::UserRole);
        if (data.isValid()) showEntry(data.toInt());
    });

    if (!entryItems.isEmpty()) {
        tree->setCurrentItem(entryItems.first());
    } else {
        auto* empty = new QWidget();
        auto* layout = new QVBoxLayout(empty);
        layout->addWidget(mutedLabel("In diesem Bereich gibt es nichts zu bearbeiten.", empty));
        layout->addStretch();
        detailScroll->setWidget(empty);
    }
}

void EditSectionPage::buildList() {
    tree->clear();
    entryItems.clear();
    QHash<QString, QTreeWidgetItem*> groups;
    for (int i = 0; i < section.entries.size(); i++) {
        const edit::Entry& entry = section.entries[i];
        QTreeWidgetItem* parentItem = nullptr;
        if (!entry.group.isEmpty()) {
            parentItem = groups.value(entry.group);
            if (!parentItem) {
                parentItem = new QTreeWidgetItem(tree, {entry.group});
                parentItem->setFlags(Qt::ItemIsEnabled);
                QFont font = parentItem->font(0);
                font.setBold(true);
                parentItem->setFont(0, font);
                parentItem->setForeground(0, QColor(0x90, 0x97, 0xa8));
                groups.insert(entry.group, parentItem);
                parentItem->setExpanded(true);
            }
        }
        auto* item = parentItem ? new QTreeWidgetItem(parentItem, {entry.title}) : new QTreeWidgetItem(tree, {entry.title});
        item->setData(0, Qt::UserRole, i);
        entryItems.append(item);
        markItem(i);
    }
    tree->expandAll();
    applyFilter();
}

void EditSectionPage::markItem(int index) {
    if (index < 0 || index >= entryItems.size()) return;
    const edit::Entry& entry = section.entries[index];
    const bool changed = entry.changed && entry.changed();
    QTreeWidgetItem* item = entryItems[index];
    item->setForeground(0, changed ? QBrush(kChangedColor) : QBrush());
    item->setToolTip(0, changed ? QString("Geändert") : QString());
}

void EditSectionPage::refreshMarks() {
    for (int i = 0; i < entryItems.size(); i++) markItem(i);
}

void EditSectionPage::applyFilter() {
    const QStringList words = searchEdit->text().toLower().split(' ', Qt::SkipEmptyParts);
    const bool changedOnly = onlyChanged->isChecked();
    int visible = 0;
    for (int i = 0; i < entryItems.size(); i++) {
        const edit::Entry& entry = section.entries[i];
        bool show = true;
        if (changedOnly) show = entry.changed && entry.changed();
        if (show && !words.isEmpty()) {
            QString haystack = (entry.group + " " + entry.title).toLower();
            if (entry.search) haystack += " " + entry.search().toLower();
            for (const QString& w : words) {
                if (!haystack.contains(w)) {
                    show = false;
                    break;
                }
            }
        }
        entryItems[i]->setHidden(!show);
        if (show) visible++;
    }
    for (int g = 0; g < tree->topLevelItemCount(); g++) {
        QTreeWidgetItem* top = tree->topLevelItem(g);
        if (top->childCount() == 0) continue;
        bool any = false;
        for (int c = 0; c < top->childCount() && !any; c++) any = !top->child(c)->isHidden();
        top->setHidden(!any);
    }
    countLabel->setText(QString("%1 von %2 Einträgen").arg(visible).arg(entryItems.size()));
}

void EditSectionPage::updateEditedState(QWidget* widget, const edit::Field& field) {
    const bool changed = field.changed();
    widget->setProperty("edited", changed);
    QString tip = field.info;
    if (field.original) {
        const QString before = edit::valueText(pickers->catalog(), field, field.original());
        tip = (tip.isEmpty() ? QString() : tip + "\n\n") + "Vorher: " + before;
    }
    widget->setToolTip(tip);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

QWidget* EditSectionPage::fieldWidget(const edit::Field& field, QWidget* parent) {
    const bool readOnly = !field.set;
    auto* guard = new WheelGuard(detailScroll, parent);
    auto afterChange = [this, field](QWidget* widget) {
        updateEditedState(widget, field);
        markItem(currentEntry);
        emit edited();
        if (field.rebuild) scheduleRebuild();
        else refreshWarnings();
    };

    switch (field.kind) {
    case edit::Kind::Text: {
        auto* label = smallLabel(field.text ? field.text() : QString(), "textValue", parent);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (!field.info.isEmpty()) label->setToolTip(field.info);
        return label;
    }
    case edit::Kind::Check: {
        auto* box = new QCheckBox(parent);
        box->setChecked(field.get && field.get() != 0);
        box->setEnabled(!readOnly);
        updateEditedState(box, field);
        connect(box, &QCheckBox::toggled, this, [field, box, afterChange](bool on) {
            field.set(on ? 1 : 0);
            afterChange(box);
        });
        return box;
    }
    case edit::Kind::Number: {
        auto* spin = new QSpinBox(parent);
        spin->setRange(field.min, field.max);
        spin->setValue(field.get ? field.get() : 0);
        spin->setEnabled(!readOnly);
        spin->setFocusPolicy(Qt::StrongFocus);
        spin->installEventFilter(guard);
        spin->setMinimumWidth(70);
        updateEditedState(spin, field);
        connect(spin, &QSpinBox::valueChanged, this, [field, spin, afterChange](int value) {
            field.set(value);
            afterChange(spin);
        });
        return spin;
    }
    case edit::Kind::Choice: {
        auto* combo = new QComboBox(parent);
        combo->addItems(field.choices);
        // Lange Eintraege (z. B. Faehigkeiten) sollen das Raster nicht verbreitern; die Liste zeigt sie ganz
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(8);
        combo->view()->setMinimumWidth(combo->view()->sizeHintForColumn(0) + 24);
        const int value = field.get ? field.get() : 0;
        if (value >= 0 && value < field.choices.size()) {
            combo->setCurrentIndex(value);
        } else {
            combo->addItem(QString("Wert %1").arg(value));
            combo->setCurrentIndex(combo->count() - 1);
        }
        combo->setEnabled(!readOnly);
        combo->setFocusPolicy(Qt::StrongFocus);
        combo->installEventFilter(guard);
        updateEditedState(combo, field);
        connect(combo, &QComboBox::activated, this, [field, combo, afterChange](int index) {
            if (index < 0 || index >= field.choices.size()) return;
            field.set(index);
            afterChange(combo);
        });
        return combo;
    }
    default: {
        // Pokemon, Attacke, Item, Faehigkeit, Typ, Wesen: durchsuchbare Auswahl
        auto* combo = new QComboBox(parent);
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setModel(pickers->model(field.kind));
        combo->setMaxVisibleItems(18);
        combo->setMinimumWidth(field.kind == edit::Kind::Pokemon ? 190 : 150);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setFocusPolicy(Qt::StrongFocus);
        combo->installEventFilter(guard);
        combo->completer()->setFilterMode(Qt::MatchContains);
        combo->completer()->setCompletionMode(QCompleter::PopupCompletion);
        combo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
        combo->setEnabled(!readOnly);
        const edit::Kind kind = field.kind;
        EditPickers* p = pickers;
        auto showValue = [p, kind, combo](int value) {
            const int row = p->rowOf(kind, value);
            QSignalBlocker block(combo);
            combo->setCurrentIndex(row);
            if (row < 0) combo->setEditText(p->catalog().name(kind, value));
        };
        showValue(field.get ? field.get() : 0);
        updateEditedState(combo, field);
        connect(combo, &QComboBox::currentIndexChanged, this, [field, combo, p, kind, afterChange](int row) {
            if (row < 0) return;
            const int value = p->valueAt(kind, row);
            if (field.get && field.get() == value) return;
            field.set(value);
            afterChange(combo);
        });
        // Unvollstaendige Eingabe beim Verlassen wieder auf den aktuellen Wert setzen
        connect(combo->lineEdit(), &QLineEdit::editingFinished, this, [field, showValue]() {
            if (field.get) showValue(field.get());
        });
        return combo;
    }
    }
}

void EditSectionPage::addFieldGrid(QGridLayout* grid, const QList<edit::Field>& fields, int perLine, QWidget* parent,
                                   int startRow) {
    int column = 0;
    int row = startRow;
    for (const edit::Field& field : fields) {
        const int span = qBound(1, field.span, perLine);
        if (column + span > perLine) {
            column = 0;
            row += 2;
        }
        auto* label = smallLabel(field.label, "fieldLabel", parent);
        if (!field.info.isEmpty()) label->setToolTip(field.info);
        grid->addWidget(label, row, column, 1, span);
        grid->addWidget(fieldWidget(field, parent), row + 1, column, 1, span);
        column += span;
    }
    for (int c = 0; c < perLine; c++) grid->setColumnStretch(c, 1);
}

void EditSectionPage::scheduleRebuild() {
    const int index = currentEntry;
    QTimer::singleShot(0, this, [this, index]() {
        if (index != currentEntry) return;
        const int scrollPos = detailScroll->verticalScrollBar()->value();
        showEntry(index);
        QTimer::singleShot(0, this, [this, scrollPos]() { detailScroll->verticalScrollBar()->setValue(scrollPos); });
    });
}

void EditSectionPage::showEntry(int index) {
    if (index < 0 || index >= section.entries.size()) return;
    currentEntry = index;
    const edit::Entry& entry = section.entries[index];
    const edit::Content content = entry.build ? entry.build() : edit::Content();

    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 0, 12, 12);
    layout->setSpacing(12);

    auto* title = new QLabel(entry.title, page);
    title->setObjectName("pageTitle");
    title->setWordWrap(true);
    layout->addWidget(title);
    if (!entry.group.isEmpty()) layout->addWidget(mutedLabel(entry.group, page));
    if (!content.hint.isEmpty()) layout->addWidget(mutedLabel(content.hint, page));
    warningLabel = new QLabel(page);
    warningLabel->setObjectName("editWarning");
    warningLabel->setWordWrap(true);
    layout->addWidget(warningLabel);
    currentWarnings = content.warnings;
    refreshWarnings();

    if (!content.fields.isEmpty()) {
        auto* card = new QFrame(page);
        card->setObjectName("editCard");
        auto* grid = new QGridLayout(card);
        grid->setContentsMargins(14, 12, 14, 14);
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(4);
        addFieldGrid(grid, content.fields, qMax(1, content.fieldsPerLine), card, 0);
        layout->addWidget(card);
    }

    for (const edit::Table& table : content.tables) {
        auto* card = new QFrame(page);
        card->setObjectName("editCard");
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 12, 14, 14);
        cardLayout->setSpacing(8);

        auto* head = new QHBoxLayout();
        if (!table.title.isEmpty()) head->addWidget(smallLabel(table.title, "cardTitle", card));
        head->addStretch();
        if (table.add) {
            auto* addButton = new QPushButton("+ Hinzufügen", card);
            addButton->setObjectName("secondary");
            addButton->setCursor(Qt::PointingHandCursor);
            addButton->setEnabled(!table.canAdd || table.canAdd());
            const auto addFn = table.add;
            connect(addButton, &QPushButton::clicked, this, [this, addFn]() {
                addFn();
                markItem(currentEntry);
                emit edited();
                scheduleRebuild();
            });
            head->addWidget(addButton);
        }
        cardLayout->addLayout(head);

        const int rows = table.rowCount ? table.rowCount() : 0;
        const bool removable = static_cast<bool>(table.remove) && (!table.canRemove || table.canRemove());
        if (rows == 0) cardLayout->addWidget(mutedLabel("(leer)", card));

        auto makeRemove = [this, &table, card](int r) {
            auto* button = new QPushButton("Entfernen", card);
            button->setObjectName("removeButton");
            button->setCursor(Qt::PointingHandCursor);
            const auto removeFn = table.remove;
            connect(button, &QPushButton::clicked, this, [this, removeFn, r]() {
                removeFn(r);
                markItem(currentEntry);
                emit edited();
                scheduleRebuild();
            });
            return button;
        };

        if (table.cards) {
            for (int r = 0; r < rows; r++) {
                auto* sub = new QFrame(card);
                sub->setObjectName("subEditor");
                sub->setAttribute(Qt::WA_StyledBackground, true);
                auto* subLayout = new QVBoxLayout(sub);
                subLayout->setContentsMargins(12, 10, 12, 12);
                subLayout->setSpacing(6);
                auto* subHead = new QHBoxLayout();
                subHead->addWidget(smallLabel(table.rowTitle ? table.rowTitle(r) : QString("Eintrag %1").arg(r + 1),
                                              "cardTitle", sub));
                subHead->addStretch();
                if (removable) subHead->addWidget(makeRemove(r));
                subLayout->addLayout(subHead);
                auto* grid = new QGridLayout();
                grid->setHorizontalSpacing(12);
                grid->setVerticalSpacing(3);
                addFieldGrid(grid, table.row(r), qMax(1, table.columnsPerLine), sub, 0);
                subLayout->addLayout(grid);
                cardLayout->addWidget(sub);
            }
        } else if (rows > 0) {
            auto* grid = new QGridLayout();
            grid->setHorizontalSpacing(10);
            grid->setVerticalSpacing(5);
            for (int c = 0; c < table.columns.size(); c++) {
                grid->addWidget(smallLabel(table.columns[c], "tableHead", card), 0, c);
            }
            for (int r = 0; r < rows; r++) {
                const QList<edit::Field> fields = table.row(r);
                for (int c = 0; c < fields.size(); c++) {
                    grid->addWidget(fieldWidget(fields[c], card), r + 1, c);
                }
                if (removable) grid->addWidget(makeRemove(r), r + 1, static_cast<int>(fields.size()));
            }
            for (int c = 0; c < table.columns.size(); c++) {
                // Auswahlfelder duerfen breiter werden als Zahlen
                const QList<edit::Field> first = table.row(0);
                const bool wide = c < first.size() && first[c].kind != edit::Kind::Number && first[c].kind != edit::Kind::Check;
                grid->setColumnStretch(c, wide ? 3 : 1);
            }
            cardLayout->addLayout(grid);
        }
        layout->addWidget(card);
    }
    layout->addStretch();

    QWidget* old = detailScroll->takeWidget();
    detailScroll->setWidget(page);
    if (old) old->deleteLater();
    detail = page;
}

void EditSectionPage::refreshWarnings() {
    if (!warningLabel) return;
    const QStringList list = currentWarnings ? currentWarnings() : QStringList();
    warningLabel->setText(list.isEmpty() ? QString() : "⚠ " + list.join("\n⚠ "));
    warningLabel->setVisible(!list.isEmpty());
}

// -------------------------------------------------------------- EditWorkspace

EditWorkspace::EditWorkspace(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    stack = new QStackedWidget(this);
    layout->addWidget(stack);

    nav = new QListWidget();
    nav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    nav->setCursor(Qt::PointingHandCursor);
    connect(nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= pages.size()) return;
        pages[row]->refreshMarks();
        stack->setCurrentIndex(row);
    });
}

void EditWorkspace::clear() {
    {
        QSignalBlocker block(nav);
        nav->clear();
    }
    for (EditSectionPage* page : pages) {
        stack->removeWidget(page);
        page->deleteLater();
    }
    pages.clear();
    if (pickers) {
        pickers->deleteLater();
        pickers = nullptr;
    }
}

void EditWorkspace::setSections(const QList<edit::Section>& sections, const edit::Catalog& catalog) {
    clear();
    pickers = new EditPickers(catalog, this);
    QSignalBlocker block(nav);
    for (const edit::Section& section : sections) {
        auto* page = new EditSectionPage(section, pickers, stack);
        connect(page, &EditSectionPage::edited, this, &EditWorkspace::edited);
        stack->addWidget(page);
        pages.append(page);
        nav->addItem(section.title);
    }
    if (!pages.isEmpty()) {
        nav->setCurrentRow(0);
        stack->setCurrentIndex(0);
    }
}

} // namespace modernui
