#include "headers/modern_ui/GameSelectPage.h"
#include "headers/modern_ui/modern_widgets.h"

#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>

using namespace modernui;

namespace {

const QString kVersion = "0.2.0";

// Vorschaubild: auf feste Groesse zuschneiden (mittig) und Ecken abrunden
QPixmap coverThumbnail(const QString& path, const QSize& size, bool dimmed) {
    QPixmap result(size * 2);
    result.setDevicePixelRatio(2.0);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, size.width(), size.height()), 8, 8);
    painter.setClipPath(clip);

    QPixmap source(path);
    if (source.isNull()) {
        painter.fillRect(QRect(QPoint(0, 0), size), QColor("#262b38"));
    } else {
        QPixmap scaled = source.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int x = (scaled.width() - size.width()) / 2;
        int y = (scaled.height() - size.height()) / 2;
        painter.drawPixmap(0, 0, scaled, x, y, size.width(), size.height());
    }
    if (dimmed) {
        painter.fillRect(QRect(QPoint(0, 0), size), QColor(19, 21, 28, 150));
    }
    return result;
}

} // namespace

QList<GameSelectPage::GameInfo> GameSelectPage::games() {
    return {
        {0, "Pokémon Karmesin und Purpur",
         "Generation 9 · inklusive „Die türkisgrüne Maske“ und „Die Indigoblaue Scheibe“",
         "Trainer, wilde Pokémon, Starter, Geschenke, Items, Pokémon-Daten und Bosskämpfe",
         "assets/Supported Games/image1.jpeg", true},
        {1, "Pokémon Schwert und Schild",
         "Generation 8 · inklusive Erweiterungspass",
         "Benötigt einen eigenen Dump mit Update 1.3.2 · Trainer mit Typ-Arenen, Starter, Geschenke, Begegnungen "
         "und wilde Pokémon",
         "assets/Supported Games/image4.jpeg", true},
        {2, "Pokémon Strahlender Diamant und Leuchtende Perle",
         "Generation 8 · Remake von Diamant und Perl",
         "Trainer, wilde Pokémon, Starter und mehr",
         "assets/Supported Games/image3.jpeg", false},
    };
}

GameSelectPage::GameSelectPage(QWidget* parent) : QWidget(parent) {
    setObjectName("modernRoot");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(modernui::styleSheet());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- Inhalt (scrollbar, mittig, begrenzte Breite) ---
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget(scroll);
    auto* center = new QHBoxLayout(inner);
    center->setContentsMargins(28, 36, 28, 28);
    center->addStretch(1);

    auto* column = new QWidget(inner);
    column->setMaximumWidth(820);
    column->setMinimumWidth(520);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel("Pokémon Randomizer", column);
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    layout->addWidget(mutedLabel("Wähle das Spiel, das du randomisieren möchtest.", column));
    layout->addSpacing(8);

    for (const GameInfo& game : games()) {
        layout->addWidget(buildGameCard(game));
    }
    layout->addStretch();

    center->addWidget(column, 4);
    center->addStretch(1);
    scroll->setWidget(inner);
    root->addWidget(scroll, 1);

    // --- Fusszeile ---
    auto* bar = new QWidget(this);
    bar->setObjectName("bottomBar");
    bar->setAttribute(Qt::WA_StyledBackground, true);
    auto* barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(20, 10, 20, 10);
    barLayout->setSpacing(10);

    barLayout->addWidget(mutedLabel("Version " + kVersion, bar));
    barLayout->addStretch();

    auto* onTop = new QCheckBox("Immer im Vordergrund", bar);
    QSettings settings("Pokemon Randomizer", "Main");
    onTop->setChecked(settings.value("AlwaysOnTop", false).toBool());
    connect(onTop, &QCheckBox::toggled, this, &GameSelectPage::alwaysOnTopChanged);
    barLayout->addLayout(rowWithInfo(onTop, "Das Fenster bleibt über allen anderen Fenstern, z. B. über dem Emulator.", false));

    auto* update = new QPushButton("Nach Updates suchen", bar);
    update->setObjectName("secondary");
    update->setCursor(Qt::PointingHandCursor);
    connect(update, &QPushButton::clicked, this, &GameSelectPage::updateCheckRequested);
    barLayout->addWidget(update);
    root->addWidget(bar);
}

QWidget* GameSelectPage::buildGameCard(const GameInfo& game) {
    auto* card = new QFrame();
    card->setObjectName("card");
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(14, 14, 18, 14);
    row->setSpacing(16);

    auto* cover = new QLabel(card);
    cover->setPixmap(coverThumbnail(game.cover, QSize(72, 96), !game.available));
    cover->setFixedSize(72, 96);
    row->addWidget(cover, 0, Qt::AlignTop);

    auto* text = new QVBoxLayout();
    text->setSpacing(4);
    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    auto* title = new QLabel(game.title, card);
    title->setObjectName(game.available ? "gameTitle" : "gameTitleSoon");
    titleRow->addWidget(title);
    auto* badge = new QLabel(game.preview ? "Im Aufbau" : (game.available ? "Verfügbar" : "Bald verfügbar"), card);
    badge->setObjectName(game.available && !game.preview ? "badgeReady" : "badge");
    badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    titleRow->addWidget(badge, 0, Qt::AlignVCenter);
    titleRow->addStretch();
    text->addLayout(titleRow);
    text->addWidget(mutedLabel(game.subtitle, card));
    auto* features = mutedLabel(game.features, card);
    features->setObjectName("gameFeatures");
    text->addWidget(features);
    row->addLayout(text, 1);

    auto* open = new QPushButton(game.available ? "Öffnen" : "In Arbeit", card);
    open->setObjectName(game.available ? "primary" : "secondary");
    open->setEnabled(game.available);
    open->setCursor(game.available ? Qt::PointingHandCursor : Qt::ArrowCursor);
    row->addWidget(open, 0, Qt::AlignVCenter);

    if (game.available) {
        int id = game.id;
        connect(open, &QPushButton::clicked, this, [this, id]() { emit gameChosen(id); });
    }
    return card;
}
