#include "headers/modern_ui/modern_widgets.h"

#include <QToolTip>
#include <QCursor>
#include <QStyle>

namespace modernui {

// ------------------------------- InfoButton --------------------------------

InfoButton::InfoButton(const QString& text, QWidget* parent)
    : QToolButton(parent), infoText(text) {
    setObjectName("infoButton");
    setText("i");
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    // Rich Text sorgt fuer automatischen Zeilenumbruch im Tooltip
    setToolTip("<p>" + text.toHtmlEscaped().replace("\n", "<br>") + "</p>");
    connect(this, &QToolButton::clicked, this, [this]() {
        QToolTip::showText(mapToGlobal(QPoint(0, height() + 2)), toolTip(), this);
    });
}

// ---------------------------- SegmentedControl -----------------------------

SegmentedControl::SegmentedControl(const QStringList& options, QWidget* parent)
    : QWidget(parent), group(new QButtonGroup(this)) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    group->setExclusive(true);

    for (int i = 0; i < options.size(); i++) {
        auto* button = new QPushButton(options[i], this);
        button->setObjectName("segment");
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("first", i == 0);
        button->setProperty("last", i == options.size() - 1);
        group->addButton(button, i);
        layout->addWidget(button);
    }
    if (!options.isEmpty()) {
        group->button(0)->setChecked(true);
    }
    connect(group, &QButtonGroup::idClicked, this, &SegmentedControl::changed);
}

int SegmentedControl::current() const {
    return group->checkedId();
}

void SegmentedControl::setCurrent(int index) {
    if (group->button(index) != nullptr) {
        group->button(index)->setChecked(true);
    }
}

// ---------------------------------- Card -----------------------------------

Card::Card(const QString& title, const QString& badge, QWidget* parent) : QFrame(parent) {
    setObjectName("card");
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 14, 16, 14);
    outer->setSpacing(8);

    headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);
    auto* titleLabel = new QLabel(title, this);
    titleLabel->setObjectName("cardTitle");
    headerLayout->addWidget(titleLabel);
    if (!badge.isEmpty()) {
        auto* badgeLabel = new QLabel(badge, this);
        badgeLabel->setObjectName("badge");
        badgeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        headerLayout->addWidget(badgeLabel, 0, Qt::AlignVCenter);
    }
    headerLayout->addStretch();
    outer->addLayout(headerLayout);

    bodyLayout = new QVBoxLayout();
    bodyLayout->setSpacing(6);
    outer->addLayout(bodyLayout);
}

// ------------------------------- Collapsible -------------------------------

Collapsible::Collapsible(const QString& title, bool expanded, QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);

    toggle = new QToolButton(this);
    toggle->setObjectName("collapseHeader");
    toggle->setText(title);
    toggle->setCheckable(true);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setCursor(Qt::PointingHandCursor);
    outer->addWidget(toggle, 0, Qt::AlignLeft);

    content = new QWidget(this);
    bodyLayout = new QVBoxLayout(content);
    bodyLayout->setContentsMargins(14, 2, 0, 2);
    bodyLayout->setSpacing(6);
    outer->addWidget(content);

    connect(toggle, &QToolButton::toggled, this, &Collapsible::setExpanded);
    setExpanded(expanded);
}

void Collapsible::setExpanded(bool expanded) {
    toggle->blockSignals(true);
    toggle->setChecked(expanded);
    toggle->blockSignals(false);
    toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    content->setVisible(expanded);
}

// --------------------------------- Helfer ----------------------------------

QHBoxLayout* rowWithInfo(QWidget* widget, const QString& info, bool stretch) {
    auto* row = new QHBoxLayout();
    row->setSpacing(6);
    row->addWidget(widget);
    if (!info.isEmpty()) {
        row->addWidget(new InfoButton(info, widget->parentWidget()));
    }
    if (stretch) {
        row->addStretch();
    }
    return row;
}

QLabel* mutedLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("muted");
    label->setWordWrap(true);
    return label;
}

QString styleSheet() {
    return QStringLiteral(R"(
#modernRoot { background: #13151c; }
#modernRoot QWidget { color: #e6e8ee; font-family: "Segoe UI", "Noto Sans", sans-serif; font-size: 13px; }
#modernRoot QScrollArea { background: transparent; border: none; }
#modernRoot QScrollArea > QWidget > QWidget { background: transparent; }

#modernRoot #sidebar { background: #181b24; border-right: 1px solid #2c3140; }
#modernRoot #appTitle { font-size: 16px; font-weight: 600; padding: 4px 6px; }
#modernRoot #appSubtitle { color: #9097a8; font-size: 11px; padding: 0 6px 8px 6px; }
#modernRoot #sidebar QListWidget { background: transparent; border: none; outline: 0; }
#modernRoot #sidebar QListWidget::item { padding: 9px 12px; border-radius: 8px; color: #9097a8; margin: 1px 0; }
#modernRoot #sidebar QListWidget::item:selected { background: #2a2350; color: #d6ccff; }
#modernRoot #sidebar QListWidget::item:hover:!selected { background: #20242f; color: #e6e8ee; }

#modernRoot #pageTitle { font-size: 22px; font-weight: 600; }
#modernRoot #muted { color: #9097a8; }
#modernRoot #sectionLabel { color: #9097a8; font-size: 12px; font-weight: 600; padding-top: 6px; }

#modernRoot #card { background: #1b1f29; border: 1px solid #2c3140; border-radius: 12px; }
#modernRoot #cardTitle { font-size: 15px; font-weight: 600; }
#modernRoot #badgeReady { background: #1e3a2c; color: #7ee2a8; border-radius: 9px; padding: 2px 9px; font-size: 11px; }
#modernRoot #gameTitle { font-size: 16px; font-weight: 600; }
#modernRoot #gameTitleSoon { font-size: 16px; font-weight: 600; color: #9097a8; }
#modernRoot #gameFeatures { color: #6f7689; font-size: 12px; }
#modernRoot #backButton { background: transparent; border: none; color: #9097a8; text-align: left; padding: 4px 6px; }
#modernRoot #backButton:hover { color: #d6ccff; }
#modernRoot #primary:disabled { background: #2c3140; color: #5c6274; }
#modernRoot #statusOk { color: #7ee2a8; }
#modernRoot #statusWarn { color: #f2c46d; }
#modernRoot #statusError { color: #ff8a8a; }
#modernRoot #pathCaption { color: #9097a8; font-size: 12px; font-weight: 600; }
#modernRoot #badge { background: #262b38; color: #9097a8; border-radius: 9px; padding: 2px 9px; font-size: 11px; }
#modernRoot #groupRow { border-top: 1px solid #262b38; }
#modernRoot #subEditor { background: #171a23; border: 1px solid #2c3140; border-radius: 10px; }

#modernRoot QCheckBox { spacing: 8px; }
#modernRoot QCheckBox:disabled { color: #5c6274; }
#modernRoot QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1px solid #4a5165; background: #13151c; }
#modernRoot QCheckBox::indicator:hover { border-color: #7c5cff; }
#modernRoot QCheckBox::indicator:checked { background: #7c5cff; border-color: #7c5cff; image: url(assets/icons/check.png); }
#modernRoot QCheckBox::indicator:disabled { border-color: #333849; background: #1a1d26; }
#modernRoot QCheckBox::indicator:checked:disabled { background: #3a3360; border-color: #3a3360; }
#modernRoot #masterSwitch { font-size: 15px; font-weight: 600; }

#modernRoot #infoButton { border: 1px solid #4a5165; border-radius: 8px; min-width: 16px; max-width: 16px; min-height: 16px; max-height: 16px;
              font-size: 10px; font-weight: 700; color: #9097a8; background: transparent; padding: 0; }
#modernRoot #infoButton:hover { border-color: #7c5cff; color: #d6ccff; }

#modernRoot #segment { background: #13151c; border: 1px solid #3a4052; padding: 5px 12px; color: #9097a8; border-radius: 0; margin-left: -1px; }
#modernRoot #segment[first="true"] { border-top-left-radius: 7px; border-bottom-left-radius: 7px; margin-left: 0; }
#modernRoot #segment[last="true"] { border-top-right-radius: 7px; border-bottom-right-radius: 7px; }
#modernRoot #segment:hover:!checked { color: #e6e8ee; }
#modernRoot #segment:checked { background: #2a2350; color: #d6ccff; border-color: #5b47c7; }
#modernRoot #segment:disabled { color: #5c6274; }

#modernRoot QSpinBox, #modernRoot QLineEdit { background: #13151c; border: 1px solid #3a4052; border-radius: 7px; padding: 4px 8px;
                                              selection-background-color: #7c5cff; min-height: 20px; }
#modernRoot QSpinBox:focus, #modernRoot QLineEdit:focus { border-color: #7c5cff; }
#modernRoot QSpinBox:disabled { color: #5c6274; border-color: #2c3140; }
#modernRoot QComboBox { background: #13151c; border: 1px solid #3a4052; border-radius: 7px; padding: 4px 8px; min-height: 20px; }
#modernRoot QComboBox:hover { border-color: #7c5cff; }
#modernRoot QComboBox:disabled { color: #5c6274; border-color: #2c3140; }
#modernRoot QComboBox::drop-down { border: none; width: 20px; }
#modernRoot QComboBox::down-arrow { image: url(assets/icons/chevron.png); width: 12px; height: 12px; }
#modernRoot QComboBox QAbstractItemView { background: #1b1f29; color: #e6e8ee; border: 1px solid #3a4052;
                                          selection-background-color: #2a2350; selection-color: #d6ccff; outline: 0; }

#modernRoot #primary { background: #7c5cff; color: #ffffff; border: none; border-radius: 8px; padding: 9px 26px; font-weight: 600; font-size: 14px; }
#modernRoot #primary:hover { background: #8d70ff; }
#modernRoot #primary:pressed { background: #6a4be6; }
#modernRoot #secondary { background: transparent; border: 1px solid #3a4052; border-radius: 8px; padding: 7px 14px; }
#modernRoot #secondary:hover { border-color: #7c5cff; color: #d6ccff; }

#modernRoot #bottomBar { background: #181b24; border-top: 1px solid #2c3140; }
#modernRoot #collapseHeader { background: transparent; border: none; color: #9097a8; padding: 4px 0; font-weight: 600; }
#modernRoot #collapseHeader:hover { color: #e6e8ee; }

QToolTip { background: #262b38; color: #e6e8ee; border: 1px solid #3a4052; padding: 6px; border-radius: 6px; }
#modernRoot QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
#modernRoot QScrollBar::handle:vertical { background: #2f3546; border-radius: 4px; min-height: 30px; }
#modernRoot QScrollBar::handle:vertical:hover { background: #3c4358; }
#modernRoot QScrollBar::add-line:vertical, #modernRoot QScrollBar::sub-line:vertical { height: 0; }
#modernRoot QScrollBar::add-page:vertical, #modernRoot QScrollBar::sub-page:vertical { background: transparent; }
)");
}

} // namespace modernui
