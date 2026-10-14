#include "headers/bdsp/BdspRandomizerWindow.h"
#include "headers/modern_ui/modern_widgets.h"
#include "headers/swsh/SwShTrainerEditor.h"

#include <QApplication>
#include "headers/common/mod_output.h"
#include <QCompleter>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QTextStream>
#include <QUrl>
#include <functional>
#include <memory>
#include <vector>

using namespace modernui;

namespace {
const QStringList kPages = {"Start", "Trainer", "Starter & Geschenke", "Wilde Pokémon", "Items", "Pokémon-Daten",
                            "Kampfturm"};

const char* kSettingsOrg = "Pokemon Randomizer";
const char* kSettingsApp = "DiamantPerle";
const char* kOutputFolder = "Randomizers-Output/Diamant-Perle";
} // namespace

BdspRandomizerWindow::BdspRandomizerWindow(QWidget* parent) : QWidget(parent) {
    setObjectName("modernRoot");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(modernui::styleSheet());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* middle = new QHBoxLayout();
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(0);
    middle->addWidget(buildSidebar());

    pages = new QStackedWidget(this);
    pages->addWidget(buildStartPage());
    pages->addWidget(buildTrainerPage());
    pages->addWidget(buildStartersPage());
    pages->addWidget(buildWildPage());
    pages->addWidget(buildItemsPage());
    pages->addWidget(buildDataPage());
    pages->addWidget(buildTowerPage());
    middle->addWidget(pages, 1);

    root->addLayout(middle, 1);
    root->addWidget(buildBottomBar());

    connect(nav, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    nav->setCurrentRow(0);

    loadSettings();
}

BdspRandomizerWindow::~BdspRandomizerWindow() {
    saveSettings();
}

// --------------------------------------------------------------- Aufbau

QWidget* BdspRandomizerWindow::buildSidebar() {
    auto* sidebar = new QWidget(this);
    sidebar->setObjectName("sidebar");
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->setFixedWidth(220);

    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(2);

    auto* back = new QPushButton("← Spielauswahl", sidebar);
    back->setObjectName("backButton");
    back->setCursor(Qt::PointingHandCursor);
    connect(back, &QPushButton::clicked, this, &BdspRandomizerWindow::backRequested);
    layout->addWidget(back, 0, Qt::AlignLeft);
    layout->addSpacing(6);

    auto* title = new QLabel("Sinnoh Randomizer", sidebar);
    title->setObjectName("appTitle");
    auto* subtitle = new QLabel("Strahlender Diamant und Leuchtende Perle", sidebar);
    subtitle->setObjectName("appSubtitle");
    subtitle->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(subtitle);

    nav = new QListWidget(sidebar);
    nav->addItems(kPages);
    nav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    nav->setCursor(Qt::PointingHandCursor);
    layout->addWidget(nav, 1);

    layout->addWidget(mutedLabel("Bewege die Maus über ein (i), um eine Erklärung zu sehen.", sidebar));
    return sidebar;
}

QWidget* BdspRandomizerWindow::buildBottomBar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("bottomBar");
    bar->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(20, 10, 20, 10);

    auto* infoColumn = new QVBoxLayout();
    infoColumn->setSpacing(2);
    activeInfo = new QLabel("", bar);
    detailInfo = mutedLabel("", bar);
    detailInfo->setWordWrap(false);
    infoColumn->addWidget(activeInfo);
    infoColumn->addWidget(detailInfo);
    layout->addLayout(infoColumn);
    layout->addStretch();

    startButton = new QPushButton("Randomisieren", bar);
    startButton->setObjectName("primary");
    startButton->setEnabled(false);
    startButton->setCursor(Qt::PointingHandCursor);
    connect(startButton, &QPushButton::clicked, this, &BdspRandomizerWindow::startRandomizer);
    layout->addWidget(startButton);
    return bar;
}

QWidget* BdspRandomizerWindow::wrapPage(const QString& title, const QString& subtitle, QWidget* content) {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* inner = new QWidget(scroll);
    auto* outer = new QHBoxLayout(inner);
    outer->setContentsMargins(28, 22, 28, 22);

    auto* columnWidget = new QWidget(inner);
    auto* column = new QVBoxLayout(columnWidget);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(12);
    auto* titleLabel = new QLabel(title, columnWidget);
    titleLabel->setObjectName("pageTitle");
    column->addWidget(titleLabel);
    if (!subtitle.isEmpty()) column->addWidget(mutedLabel(subtitle, columnWidget));
    column->addSpacing(4);
    content->setParent(columnWidget);
    column->addWidget(content);
    column->addStretch();

    columnWidget->setMaximumWidth(1000);
    outer->addWidget(columnWidget, 1);
    scroll->setWidget(inner);
    return scroll;
}

QHBoxLayout* BdspRandomizerWindow::pathRow(QWidget* parent, const QString& caption, const QString& info,
                                           QLineEdit*& edit, bool romfs) {
    auto* row = new QHBoxLayout();
    row->setSpacing(8);
    auto* label = new QLabel(caption, parent);
    label->setFixedWidth(60);
    row->addWidget(label);
    row->addWidget(new InfoButton(info, parent));
    edit = new QLineEdit(parent);
    edit->setPlaceholderText(romfs ? "z. B. …\\Dump\\Pokemon Strahlender Diamant\\romfs"
                                   : "z. B. …\\Dump\\Pokemon Strahlender Diamant\\exefs");
    row->addWidget(edit, 1);
    auto* button = new QPushButton("Durchsuchen …", parent);
    button->setObjectName("secondary");
    button->setCursor(Qt::PointingHandCursor);
    row->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, romfs]() { browse(romfs); });
    connect(edit, &QLineEdit::editingFinished, this, &BdspRandomizerWindow::checkPaths);
    return row;
}

QWidget* BdspRandomizerWindow::buildStartPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // Spieldateien
    auto* game = new Card("Spieldateien", QString(), content);
    game->header()->addWidget(new InfoButton(
        "Der Randomizer liest die Spieldaten aus deinem eigenen Dump (Strahlender Diamant oder Leuchtende Perle). "
        "Getestet mit Update 1.3.0 – installiere das Update am besten vor dem Dumpen.", game));
    game->body()->addWidget(mutedLabel(
        "Wähle die Ordner „romfs“ und „exefs“ deines Dumps aus. Du kannst auch den übergeordneten Spielordner "
        "auswählen – beide Ordner werden dann automatisch erkannt.", game));
    game->body()->addLayout(pathRow(game, "RomFS",
        "Der Ordner „romfs“ aus deinem Dump. Er enthält den Unterordner „Data“.", romfsEdit, true));
    game->body()->addLayout(pathRow(game, "ExeFS",
        "Der Ordner „exefs“ aus deinem Dump. Daraus wird erkannt, ob es Strahlender Diamant oder Leuchtende Perle "
        "ist. Er enthält die Datei „main.npdm“.", exefsEdit, false));

    auto* statusGrid = new QWidget(game);
    auto* grid = new QVBoxLayout(statusGrid);
    grid->setContentsMargins(0, 6, 0, 0);
    grid->setSpacing(4);
    auto addStatus = [&](const QString& caption, QLabel*& value, const QString& info) {
        auto* row = new QHBoxLayout();
        auto* label = new QLabel(caption, statusGrid);
        label->setObjectName("pathCaption");
        label->setFixedWidth(90);
        row->addWidget(label);
        value = new QLabel("–", statusGrid);
        row->addWidget(value);
        row->addWidget(new InfoButton(info, statusGrid));
        row->addStretch();
        grid->addLayout(row);
    };
    addStatus("Spiel", gameLabel, "Wird aus der ExeFS erkannt. Wichtig für den Mod-Ordner (Title-ID) und später "
                                  "für versionsexklusive Pokémon.");
    addStatus("Daten", versionLabel, "Die Spieldaten liegen in Unity-Dateien. Der Randomizer liest sie direkt aus "
                                     "deinem Dump; getestet mit Update 1.3.0.");
    addStatus("Inhalt", contentLabel, "Anzahl der gelesenen Pokémon, Attacken und Trainer.");
    game->body()->addWidget(statusGrid);

    messageArea = new QWidget(game);
    messageBox = new QVBoxLayout(messageArea);
    messageBox->setContentsMargins(0, 4, 0, 0);
    messageBox->setSpacing(4);
    game->body()->addWidget(messageArea);
    layout->addWidget(game);

    // Allgemein
    auto* general = new Card("Allgemein", QString(), content);
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Seed", general));
        row->addWidget(new InfoButton("Mit demselben Seed und denselben Einstellungen entsteht immer genau dasselbe "
                                      "Ergebnis. Leer lassen für ein zufälliges Ergebnis.", general));
        row->addStretch();
        seedEdit = new QLineEdit(general);
        seedEdit->setPlaceholderText("leer = zufällig");
        seedEdit->setFixedWidth(260);
        row->addWidget(seedEdit);
        general->body()->addLayout(row);
        connect(seedEdit, &QLineEdit::textChanged, this, [this]() { if (activeInfo) updateStatus(); });
    }
    spoilerLog = new QCheckBox("Spoiler-Log erstellen", general);
    spoilerLog->setChecked(true);
    general->body()->addLayout(rowWithInfo(spoilerLog,
        "Erstellt zusätzlich die Datei Spoiler-Log.html mit allen Trainer-Teams, Startern, wilden Pokémon, Items, "
        "Läden, Pokémon-Daten und dem Kampfturm. Öffnet sich im Browser und ist durchsuchbar."));
    layout->addWidget(general);

    // Ausgabe
    auto* output = new Card("Ausgabe", QString(), content);
    output->body()->addWidget(mutedLabel(
        "Die Ergebnisse landen im Ordner Randomizers-Output → Diamant-Perle → Randomizer-1. Dort liegt ein Ordner "
        "„romfs“ mit nur den geänderten Dateien (bei neuen Startern oder Evoli zusätzlich „exefs“). Für Emulatoren "
        "kopierst du beides in einen eigenen Mod-Ordner des Spiels (oder lässt es unten direkt dorthin schreiben), "
        "für die Switch nach atmosphere/contents/<Title-ID>/.", output));
    modDirect = new QCheckBox("Direkt in den Mod-Ordner des Emulators schreiben", output);
    output->body()->addLayout(rowWithInfo(modDirect,
        "Nach dem Randomisieren werden die Ordner „romfs“ und „exefs“ zusätzlich direkt in den Mod-Ordner kopiert. "
        "Ältere Ordner dort werden dabei ersetzt – du musst also nichts mehr von Hand kopieren.\n\n"
        "„Emulator suchen“ findet Ryujinx und yuzu (sowie Ableger wie suyu, sudachi, citron und Eden) automatisch "
        "und legt den passenden Ordner …/<Title-ID>/Randomizer an. Für eine echte Switch bleibt es beim Kopieren "
        "nach atmosphere/contents/<Title-ID>/."));
    modRow = new QWidget(output);
    {
        auto* row = new QHBoxLayout(modRow);
        row->setContentsMargins(0, 0, 0, 0);
        auto* caption = new QLabel("Mod-Ordner", modRow);
        caption->setObjectName("pathCaption");
        caption->setFixedWidth(90);
        row->addWidget(caption);
        modFolderEdit = new QLineEdit(modRow);
        modFolderEdit->setPlaceholderText("z. B. …/Ryujinx/mods/contents/<Title-ID>/Randomizer");
        row->addWidget(modFolderEdit, 1);
        auto* browseButton = new QPushButton("Durchsuchen", modRow);
        browseButton->setObjectName("secondary");
        browseButton->setCursor(Qt::PointingHandCursor);
        row->addWidget(browseButton);
        auto* searchButton = new QPushButton("Emulator suchen", modRow);
        searchButton->setObjectName("secondary");
        searchButton->setCursor(Qt::PointingHandCursor);
        row->addWidget(searchButton);
        row->addWidget(new InfoButton(
            "Der Ordner deines Mods, z. B. …/mods/contents/<Title-ID>/Randomizer (Ryujinx) oder "
            "…/load/<Title-ID>/Randomizer (yuzu). Darin wird der Ordner „romfs“ angelegt bzw. ersetzt.", modRow));
        connect(browseButton, &QPushButton::clicked, this, [this]() {
            const QString start = modFolderEdit->text().trimmed().isEmpty() ? QDir::homePath() : modFolderEdit->text().trimmed();
            const QString dir = QFileDialog::getExistingDirectory(this, "Mod-Ordner wählen", start);
            if (!dir.isEmpty()) modFolderEdit->setText(QDir::toNativeSeparators(dir));
        });
        connect(searchButton, &QPushButton::clicked, this, [this]() { findEmulator(); });
    }
    output->body()->addWidget(modRow);
    modRow->setEnabled(false);
    connect(modDirect, &QCheckBox::toggled, modRow, &QWidget::setEnabled);
    {
        auto* row = new QHBoxLayout();
        auto* open = new QPushButton("Ausgabeordner öffnen", output);
        open->setObjectName("secondary");
        open->setCursor(Qt::PointingHandCursor);
        row->addWidget(open);
        row->addStretch();
        output->body()->addLayout(row);
        connect(open, &QPushButton::clicked, this, []() {
            QString path = QDir::current().filePath(kOutputFolder);
            QDir().mkpath(path);
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
    }
    layout->addWidget(output);

    // Vorlagen
    auto* templates = new Card("Vorlagen", QString(), content);
    templates->header()->addWidget(new InfoButton(
        "Speichert alle Einstellungen (alle Seiten, Seed und Spoiler-Log) in einer Datei. So kannst du "
        "Lieblings-Einstellungen wiederverwenden oder mit anderen teilen. Die Pfade zu deinem Dump und zum "
        "Mod-Ordner werden nicht mitgespeichert.", templates));
    templates->body()->addWidget(mutedLabel(
        "Einstellungen als Vorlage speichern oder eine gespeicherte Vorlage laden.", templates));
    {
        auto* row = new QHBoxLayout();
        auto* saveButton = new QPushButton("Einstellungen speichern …", templates);
        auto* loadButton = new QPushButton("Einstellungen laden …", templates);
        for (QPushButton* b : {saveButton, loadButton}) {
            b->setObjectName("secondary");
            b->setCursor(Qt::PointingHandCursor);
            row->addWidget(b);
        }
        row->addStretch();
        templates->body()->addLayout(row);
        connect(saveButton, &QPushButton::clicked, this, [this]() { saveTemplate(); });
        connect(loadButton, &QPushButton::clicked, this, [this]() { loadTemplate(); });
    }
    layout->addWidget(templates);
    layout->addStretch();

    return wrapPage("Start", "Spieldateien, allgemeine Einstellungen und Ausgabe.", content);
}

// ---------------------------------------------------------- Spieldateien

void BdspRandomizerWindow::browse(bool romfs) {
    QLineEdit* edit = romfs ? romfsEdit : exefsEdit;
    QString start = edit->text();
    if (start.isEmpty()) start = (romfs ? exefsEdit : romfsEdit)->text();
    QString dir = QFileDialog::getExistingDirectory(this, romfs ? "RomFS-Ordner wählen" : "ExeFS-Ordner wählen", start);
    if (dir.isEmpty()) return;
    setDumpPaths(romfs ? dir : romfsEdit->text(), romfs ? exefsEdit->text() : dir);
}

void BdspRandomizerWindow::setDumpPaths(const QString& romfsIn, const QString& exefsIn) {
    QString romfs = QDir::fromNativeSeparators(romfsIn.trimmed());
    QString exefs = QDir::fromNativeSeparators(exefsIn.trimmed());

    // Spielordner ausgewaehlt (enthaelt romfs und/oder exefs)?
    auto findChild = [](const QString& folder, const QString& name) -> QString {
        if (folder.isEmpty()) return QString();
        QDir dir(folder);
        for (const QString& entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.compare(name, Qt::CaseInsensitive) == 0) return dir.filePath(entry);
        }
        return QString();
    };
    for (QString* p : {&romfs, &exefs}) {
        if (!p->isEmpty() && !QDir(*p + "/Data").exists() && !QFileInfo::exists(*p + "/main.npdm")) {
            const QString r = findChild(*p, "romfs");
            const QString e = findChild(*p, "exefs");
            if (!r.isEmpty()) romfs = r;
            if (!e.isEmpty()) exefs = e;
        }
    }
    if (exefs.isEmpty() && !romfs.isEmpty()) exefs = findChild(QFileInfo(romfs).absolutePath(), "exefs");

    romfsEdit->setText(QDir::toNativeSeparators(romfs));
    exefsEdit->setText(QDir::toNativeSeparators(exefs));
    checkPaths();
}

void BdspRandomizerWindow::checkPaths() {
    const QString romfs = QDir::fromNativeSeparators(romfsEdit->text().trimmed());
    const QString exefs = QDir::fromNativeSeparators(exefsEdit->text().trimmed());

    check = bdsp::checkDump(romfs, exefs);
    files.reset();
    if (check.ok) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        auto loaded = std::make_unique<bdsp::GameFiles>();
        QStringList errors;
        if (loaded->load(romfs, &errors)) {
            files = std::move(loaded);
        } else {
            check.ok = false;
            if (errors.isEmpty()) errors << "Die Spieldaten konnten nicht gelesen werden.";
            check.errors << errors;
        }
        QApplication::restoreOverrideCursor();
    }

    if (check.version != bdsp::Version::Unknown) {
        gameLabel->setText(bdsp::versionName(check.version) + "  ·  Title-ID " + bdsp::titleId(check.version));
        gameLabel->setObjectName("statusOk");
    } else {
        gameLabel->setText(exefs.isEmpty() ? "nicht erkannt (ExeFS fehlt)" : "nicht erkannt");
        gameLabel->setObjectName(exefs.isEmpty() ? "statusWarn" : "statusError");
    }
    if (files) {
        versionLabel->setText("gelesen ✓");
        versionLabel->setObjectName("statusOk");
        int species = 0;
        for (int i = 1; i <= bdsp::kMaxSpecies; i++) {
            if (files->personal.isPresent(files->personal.indexOf(i, 0))) species++;
        }
        int moves = 0;
        for (const bdsp::Move& m : files->moves.all()) moves += m.canUse ? 1 : 0;
        int trainers = 0;
        for (const bdsp::Trainer& t : files->trainers.all()) trainers += t.team.isEmpty() ? 0 : 1;
        contentLabel->setText(QString("%1 Pokémon  ·  %2 Attacken  ·  %3 Trainer").arg(species).arg(moves).arg(trainers));
        contentLabel->setObjectName("statusOk");
    } else {
        versionLabel->setText(romfs.isEmpty() ? "–" : "nicht gelesen");
        versionLabel->setObjectName(romfs.isEmpty() ? "" : "statusError");
        contentLabel->setText("–");
        contentLabel->setObjectName("");
    }
    for (QLabel* l : {gameLabel, versionLabel, contentLabel}) {
        l->style()->unpolish(l);
        l->style()->polish(l);
    }
    showMessages();
    fillStarterCombos();
    refreshStartersPage();
    updateStatus();
    saveSettings();
}

void BdspRandomizerWindow::showMessages() {
    while (QLayoutItem* item = messageBox->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    auto add = [this](const QString& text, const char* style) {
        auto* label = new QLabel(text, messageArea);
        label->setObjectName(style);
        label->setWordWrap(true);
        messageBox->addWidget(label);
    };
    if (romfsEdit->text().trimmed().isEmpty() && exefsEdit->text().trimmed().isEmpty()) {
        messageArea->hide();
        return;
    }
    for (const QString& e : check.errors) add("✗  " + e, "statusError");
    for (const QString& w : check.warnings) add("!  " + w, "statusWarn");
    messageArea->setVisible(messageBox->count() > 0);
}

QStringList BdspRandomizerWindow::activeAreas() const {
    QStringList areas;
    if (trainerSettings.enabled) areas << "Trainer";
    if (encounterSettings.starterMode != 0) areas << "Starter";
    if (encounterSettings.gifts) areas << "Geschenke";
    if (encounterSettings.statics) areas << "Begegnungen";
    if (encounterSettings.trades) areas << "Tausch";
    if (wildSettings.enabled) areas << (wildSettings.underground ? "Wilde Pokémon + Untergrund" : "Wilde Pokémon");
    if (itemSettings.fieldItems || itemSettings.hiddenItems) areas << "Items";
    if (itemSettings.shops) areas << "Läden";
    if (itemSettings.trainerItems) areas << "Trainer-Items";
    if (dataSettings.tradeEvolutions) areas << "Tausch-Entwicklungen";
    if (dataSettings.abilities || dataSettings.types || dataSettings.stats || dataSettings.levelMoves ||
        dataSettings.tmMode != 0) areas << "Pokémon-Daten";
    if (towerSettings.enabled) areas << "Kampfturm";
    return areas;
}

void BdspRandomizerWindow::updateStatus() {
    const QStringList areas = activeAreas();
    if (files) {
        const QString game = check.version == bdsp::Version::Unknown ? QString("Strahlender Diamant/Leuchtende Perle")
                                                                    : bdsp::versionName(check.version);
        // Lange Listen kuerzen, damit die Statusleiste das Fenster nicht verbreitert (alles steht im Tooltip)
        QStringList shown = areas.mid(0, 6);
        if (areas.size() > shown.size()) shown << QString("+ %1 weitere").arg(areas.size() - shown.size());
        activeInfo->setText(areas.isEmpty() ? QString("Noch nichts ausgewählt") : "Aktiv: " + shown.join(", "));
        activeInfo->setToolTip(areas.size() > 6 ? areas.join("\n") : QString());
        const QString seed = seedEdit->text().trimmed();
        detailInfo->setText(game + "  ·  Seed: " + (seed.isEmpty() ? QString("zufällig") : seed));
        startButton->setEnabled(!areas.isEmpty());
        startButton->setToolTip(areas.isEmpty() ? QString("Wähle zuerst aus, was randomisiert werden soll.") : QString());
    } else {
        activeInfo->setText("Spieldateien fehlen");
        detailInfo->setText("Gib auf der Start-Seite den RomFS- und ExeFS-Ordner deines Dumps an.");
        startButton->setEnabled(false);
        startButton->setToolTip("Zuerst die Spieldateien auf der Start-Seite angeben.");
    }
}

void BdspRandomizerWindow::refreshAllPages() {
    refreshTrainerPage();
    refreshStartersPage();
    refreshWildPage();
    refreshItemsPage();
    refreshDataPage();
    refreshTowerPage();
    updateStatus();
}

void BdspRandomizerWindow::showPage(int index) {
    nav->setCurrentRow(index);
}

// ---------------------------------------------------------- Starter & Geschenke

QWidget* BdspRandomizerWindow::buildStartersPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // --- Starter ---
    auto* starterCard = new Card("Starter", QString(), content);
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Starter-Pokémon", starterCard));
        row->addWidget(new InfoButton("Original: Chelast, Panflam und Plinfa.\n"
                                      "Zufällig: drei zufällige Pokémon.\n"
                                      "Wunsch: du legst die drei Starter selbst fest.\n\n"
                                      "Im Koffer von Professor Eibe liegen danach die neuen Starter. Dein Rivale Perl "
                                      "und Lucius/Lucia bekommen passend dazu die neuen Starter in ihre Teams.\n"
                                      "Die Starter stehen im Programmcode des Spiels. Dafür wird zusätzlich ein kleiner "
                                      "Patch im Ordner „exefs“ erstellt (wird beim Mod-Ordner automatisch mitkopiert).",
                                      starterCard));
        row->addStretch();
        starterMode = new SegmentedControl({"Original", "Zufällig", "Wunsch"}, starterCard);
        row->addWidget(starterMode);
        starterCard->body()->addLayout(row);
        connect(starterMode, &SegmentedControl::changed, this, [this](int mode) {
            encounterSettings.starterMode = mode;
            starterRandomBox->setVisible(mode == 1);
            starterWishBox->setVisible(mode == 2);
            updateStatus();
        });
    }

    starterRandomBox = new QWidget(starterCard);
    starterRandomBox->setObjectName("subEditor");
    starterRandomBox->setAttribute(Qt::WA_StyledBackground, true);
    {
        auto* box = new QVBoxLayout(starterRandomBox);
        box->setContentsMargins(14, 12, 14, 12);
        box->setSpacing(8);
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Typen", starterRandomBox));
        row->addWidget(new InfoButton("Beliebig: keine Regel.\n"
                                      "Verschieden: die drei Starter teilen sich keinen Typ.\n"
                                      "Pflanze/Feuer/Wasser: wie im Original je ein Pflanzen-, Feuer- und Wasser-Pokémon.",
                                      starterRandomBox));
        row->addStretch();
        starterTypes = new SegmentedControl({"Beliebig", "Verschieden", "Pflanze/Feuer/Wasser"}, starterRandomBox);
        row->addWidget(starterTypes);
        box->addLayout(row);
        connect(starterTypes, &SegmentedControl::changed, this, [this](int v) { encounterSettings.starterTypes = v; });
        starterStages = new QCheckBox("Nur Pokémon mit zwei Entwicklungen", starterRandomBox);
        box->addLayout(rowWithInfo(starterStages,
            "Wie echte Starter: Basis-Pokémon, die sich zweimal entwickeln (z. B. Glumanda → Glutexo → Glurak)."));
        connect(starterStages, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.starterThreeStages = on; });
        starterStrength = new QCheckBox("Ähnlich stark wie die Original-Starter", starterRandomBox);
        box->addLayout(rowWithInfo(starterStrength,
            "Optional: Die neuen Starter haben ungefähr dieselbe Basiswerte-Summe wie Chelast, Panflam und Plinfa. "
            "Ohne diese Option kann jedes passende Pokémon Starter werden."));
        connect(starterStrength, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.starterSimilarStrength = on; });
    }
    starterCard->body()->addWidget(starterRandomBox);

    starterWishBox = new QWidget(starterCard);
    starterWishBox->setObjectName("subEditor");
    starterWishBox->setAttribute(Qt::WA_StyledBackground, true);
    {
        auto* box = new QVBoxLayout(starterWishBox);
        box->setContentsMargins(14, 12, 14, 12);
        box->setSpacing(8);
        const QStringList labels = {"Statt Chelast", "Statt Panflam", "Statt Plinfa"};
        for (int i = 0; i < 3; i++) {
            auto* row = new QHBoxLayout();
            auto* label = new QLabel(labels[i], starterWishBox);
            label->setFixedWidth(120);
            row->addWidget(label);
            wishCombos[i] = new QComboBox(starterWishBox);
            wishCombos[i]->setEditable(true);
            wishCombos[i]->setInsertPolicy(QComboBox::NoInsert);
            wishCombos[i]->setMinimumWidth(280);
            wishCombos[i]->lineEdit()->setPlaceholderText("Pokémon suchen …");
            row->addWidget(wishCombos[i]);
            row->addStretch();
            box->addLayout(row);
            connect(wishCombos[i], QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, i](int index) {
                const int code = index >= 0 ? wishCombos[i]->itemData(index).toInt() : 0;
                encounterSettings.wished[i] = {code, 0};
            });
        }
        box->addWidget(mutedLabel("Leer gelassene Felder behalten den Original-Starter. Tippe einen Namen ein, um zu suchen.",
                                  starterWishBox));
    }
    starterCard->body()->addWidget(starterWishBox);
    layout->addWidget(starterCard);

    // --- Geschenke ---
    auto* giftCard = new Card("Geschenkte Pokémon", QString(), content);
    giftsBox = new QCheckBox("Geschenkte Pokémon randomisieren", giftCard);
    giftCard->body()->addLayout(rowWithInfo(giftsBox,
        "Evoli von Bebe in Herzhofen sowie die Eier vom Wanderer in Herzhofen (Wonneira) und von Urs auf der "
        "Eiseninsel (Riolu). Aus den Eiern schlüpfen nur Basis-Pokémon. Evoli steht im Programmcode und kommt "
        "deshalb mit in den Patch im Ordner „exefs“."));
    connect(giftsBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.gifts = on; updateStatus(); });
    layout->addWidget(giftCard);

    // --- Begegnungen ---
    auto* staticCard = new Card("Statische Begegnungen", QString(), content);
    staticsBox = new QCheckBox("Legendäre und besondere Begegnungen", staticCard);
    staticCard->body()->addLayout(rowWithInfo(staticsBox,
        "Dialga bzw. Palkia an der Speersäule, Giratina, Heatran, Regigigas, Selfe und Tobutz, Arceus, Darkrai, Shaymin "
        "und alle Legendären im Ramanas-Park sowie Driftlon am Windkraftwerk, Rotom in der Alten Villa und Kryppuk. "
        "Legendäre werden wieder zu Legendären. Die Regis, die Regigigas zum Aufwachen braucht, werden dabei "
        "mitgetauscht – du brauchst dann die neuen Pokémon im Team."));
    connect(staticsBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.statics = on; updateStatus(); });
    layout->addWidget(staticCard);

    // --- Tausch ---
    auto* tradeCard = new Card("Tausch", QString(), content);
    tradesBox = new QCheckBox("Tauschpartner geben zufällige Pokémon", tradeCard);
    tradeCard->body()->addLayout(rowWithInfo(tradesBox,
        "Die vier Tauschpartner (sonst Abra, Plaudagei, Alpollo und Karpador) geben zufällige Pokémon mit passenden "
        "Attacken. Was du abgeben musst, bleibt gleich."));
    connect(tradesBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.trades = on; updateStatus(); });
    layout->addWidget(tradeCard);

    // --- Gemeinsame Regeln ---
    auto* rulesCard = new Card("Regeln für Geschenke, Begegnungen und Tausch", QString(), content);
    strengthBox = new QCheckBox("Ähnlich starke Pokémon", rulesCard);
    rulesCard->body()->addLayout(rowWithInfo(strengthBox,
        "Optional, standardmäßig aus: Das neue Pokémon hat ungefähr dieselbe Basiswerte-Summe wie das alte. Gilt nicht "
        "für die Starter, die haben eine eigene Option."));
    connect(strengthBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.similarStrength = on; });
    legendBox = new QCheckBox("Legendäre Pokémon erlauben", rulesCard);
    rulesCard->body()->addLayout(rowWithInfo(legendBox,
        "Legendäre und Mysteriöse Pokémon dürfen auch dort auftauchen, wo vorher keine waren – auch als zufällige "
        "Starter."));
    connect(legendBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.legendaries = on; });
    layout->addWidget(rulesCard);
    layout->addStretch();

    refreshStartersPage();
    return wrapPage("Starter & Geschenke", "Starter, geschenkte Pokémon, statische Begegnungen und Tausch.", content);
}

void BdspRandomizerWindow::fillStarterCombos() {
    if (!files || wishCombos[0] == nullptr) return;
    bdsp::GameTexts texts;
    texts.load(QDir::fromNativeSeparators(romfsEdit->text().trimmed()));
    const QList<bdsp::StarterChoice> all = bdsp::availablePokemon(*files);
    for (int i = 0; i < 3; i++) {
        QSignalBlocker block(wishCombos[i]);
        wishCombos[i]->clear();
        wishCombos[i]->addItem("– Original –", 0);
        for (const bdsp::StarterChoice& c : all) {
            wishCombos[i]->addItem(QString("%1  ·  #%2").arg(texts.pokemonName(c.species, 0)).arg(c.species, 3, 10, QChar('0')),
                                   c.species);
        }
        auto* completer = new QCompleter(wishCombos[i]->model(), wishCombos[i]);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        wishCombos[i]->setCompleter(completer);
    }
    refreshStartersPage();
}

void BdspRandomizerWindow::refreshStartersPage() {
    if (starterMode == nullptr) return;
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (QWidget* w : std::initializer_list<QWidget*>{starterMode, starterTypes, starterStages, starterStrength, giftsBox,
                                                     staticsBox, tradesBox, strengthBox, legendBox,
                                                     wishCombos[0], wishCombos[1], wishCombos[2]}) {
        blockers.push_back(std::make_unique<QSignalBlocker>(w));
    }
    starterMode->setCurrent(encounterSettings.starterMode);
    starterTypes->setCurrent(encounterSettings.starterTypes);
    starterStages->setChecked(encounterSettings.starterThreeStages);
    starterStrength->setChecked(encounterSettings.starterSimilarStrength);
    giftsBox->setChecked(encounterSettings.gifts);
    staticsBox->setChecked(encounterSettings.statics);
    tradesBox->setChecked(encounterSettings.trades);
    strengthBox->setChecked(encounterSettings.similarStrength);
    legendBox->setChecked(encounterSettings.legendaries);
    for (int i = 0; i < 3; i++) {
        const int index = wishCombos[i]->findData(encounterSettings.wished[i].species);
        wishCombos[i]->setCurrentIndex(index >= 0 ? index : 0);
        wishCombos[i]->setEnabled(files != nullptr);
    }
    starterRandomBox->setVisible(encounterSettings.starterMode == 1);
    starterWishBox->setVisible(encounterSettings.starterMode == 2);
}

// ---------------------------------------------------------- Wilde Pokemon

namespace {
QCheckBox* addOption(QVBoxLayout* layout, QWidget* parent, const QString& text, const QString& info, bool* target,
                     QObject* context, std::function<void()> after = nullptr) {
    auto* box = new QCheckBox(text, parent);
    layout->addLayout(rowWithInfo(box, info));
    QObject::connect(box, &QCheckBox::toggled, context, [target, after](bool on) {
        *target = on;
        if (after) after();
    });
    return box;
}
} // namespace

QWidget* BdspRandomizerWindow::buildWildPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    {
        auto* row = new QHBoxLayout();
        row->setSpacing(8);
        wildSwitch = new QCheckBox("Wilde Pokémon randomisieren", content);
        wildSwitch->setObjectName("masterSwitch");
        row->addWidget(wildSwitch);
        row->addWidget(new InfoButton("Pokémon im hohen Gras, in Höhlen, beim Surfen und Angeln – auf allen Routen und in "
                                      "allen Höhlen. Dazu Schwärme, Tag/Nacht-Pokémon, der Poké-Radar, die Pokémon aus den "
                                      "GBA-Spielen (Einschub-Slots), die Honigbäume, das Große Moor und der Trophäengarten.",
                                      content));
        row->addStretch();
        layout->addLayout(row);
        connect(wildSwitch, &QCheckBox::toggled, this, [this](bool on) {
            wildSettings.enabled = on;
            wildContent->setEnabled(on);
            updateStatus();
        });
    }

    wildContent = new QWidget(content);
    auto* inner = new QVBoxLayout(wildContent);
    inner->setContentsMargins(0, 0, 0, 0);
    inner->setSpacing(12);
    layout->addWidget(wildContent);

    auto* card = new Card("Einstellungen", QString(), wildContent);
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Austausch", card));
        row->addWidget(new InfoButton(
            "Pro Gebiet: In jedem Gebiet wird jedes Pokémon durch ein festes anderes ersetzt. Ein Gebiet hat also "
            "weiterhin eine eigene, überschaubare Auswahl.\n"
            "Global 1:1: Jedes Pokémon wird überall durch dasselbe andere ersetzt (z. B. jedes Bidiza wird zu Evoli).\n"
            "Jeder Platz: Jeder Eintrag wird einzeln ausgewürfelt – die größte Vielfalt.", card));
        row->addStretch();
        wildMode = new SegmentedControl({"Pro Gebiet", "Global 1:1", "Jeder Platz"}, card);
        row->addWidget(wildMode);
        card->body()->addLayout(row);
        connect(wildMode, &SegmentedControl::changed, this, [this](int v) { wildSettings.mode = v; });
    }
    wildLevel = addOption(card->body(), card, "Level-passende Entwicklungen",
        "Pokémon tauchen erst auf, wenn sie auf diesem Level schon entwickelt sein könnten (z. B. kein Garados vor "
        "Level 20). Pokémon ohne Entwicklung, wie Fossilien, sind immer erlaubt.", &wildSettings.levelAppropriate, this);
    wildType = addOption(card->body(), card, "Gleicher Typ wie das Original",
        "Optional: Das neue Pokémon hat mindestens einen Typ mit dem alten gemeinsam – beim Angeln gibt es dann "
        "weiterhin Wasser-Pokémon.", &wildSettings.sameType, this);
    wildStrength = addOption(card->body(), card, "Ähnlich starke Pokémon",
        "Optional: Das neue Pokémon hat ungefähr dieselbe Basiswerte-Summe wie das alte.", &wildSettings.similarStrength, this);
    wildLegends = addOption(card->body(), card, "Legendäre Pokémon erlauben",
        "Legendäre und Mysteriöse Pokémon dürfen auch als wilde Pokémon auftauchen.", &wildSettings.legendaries, this);
    inner->addWidget(card);

    auto* ugCard = new Card("Untergrund", QString(), wildContent);
    wildUnderground = addOption(ugCard->body(), ugCard, "Pokémon im Untergrund randomisieren",
        "Die Pokémon in den Höhlen des Untergrunds (Pokémon-Hort), auch die seltenen Pokémon der einzelnen "
        "Höhlen-Arten. Wie im Original steigen die Level dort mit deinen Orden – die Regel für level-passende "
        "Entwicklungen gilt im Untergrund deshalb nicht.", &wildSettings.underground, this, [this]() { updateStatus(); });
    inner->addWidget(ugCard);

    inner->addWidget(mutedLabel("Der Mod enthält die Tabellen für Strahlender Diamant und für Leuchtende Perle. "
                                "Editionsexklusive Pokémon werden für jede Edition getrennt ausgewürfelt. Gebiete mit "
                                "Icognito behalten ihre Buchstaben nur, wenn dort weiterhin Icognito vorkommt.", wildContent));
    layout->addStretch();

    refreshWildPage();
    return wrapPage("Wilde Pokémon", "Gras, Höhlen, Wasser, Angeln, Honigbäume, Großes Moor und Untergrund.", content);
}

void BdspRandomizerWindow::refreshWildPage() {
    if (wildSwitch == nullptr) return;
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (QWidget* w : std::initializer_list<QWidget*>{wildSwitch, wildMode, wildLevel, wildType, wildStrength, wildLegends,
                                                     wildUnderground}) {
        blockers.push_back(std::make_unique<QSignalBlocker>(w));
    }
    wildSwitch->setChecked(wildSettings.enabled);
    wildMode->setCurrent(wildSettings.mode);
    wildLevel->setChecked(wildSettings.levelAppropriate);
    wildType->setChecked(wildSettings.sameType);
    wildStrength->setChecked(wildSettings.similarStrength);
    wildLegends->setChecked(wildSettings.legendaries);
    wildUnderground->setChecked(wildSettings.underground);
    wildContent->setEnabled(wildSettings.enabled);
}

// ------------------------------------------------------------------ Items

QWidget* BdspRandomizerWindow::buildItemsPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* world = new Card("Items in der Spielwelt", QString(), content);
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Verteilung", world));
        row->addWidget(new InfoButton(
            "Mischen: Alle Items bleiben im Spiel, liegen aber an anderen Orten. Es gibt also genauso viele "
            "Sonderbonbons wie im Original.\n"
            "Komplett zufällig: Jeder Ort bekommt ein zufälliges Item, das im Spiel vorkommt (jeweils einmal).", world));
        row->addStretch();
        itemMode = new SegmentedControl({"Mischen", "Komplett zufällig"}, world);
        row->addWidget(itemMode);
        world->body()->addLayout(row);
        connect(itemMode, &SegmentedControl::changed, this, [this](int v) { itemSettings.mode = v; });
    }
    auto status = [this]() { updateStatus(); };
    itemField = addOption(world->body(), world, "Items auf dem Boden",
        "Die Pokéball-Symbole auf Routen, in Städten und in Höhlen. TMs und Basis-Items (z. B. Fahrrad, Azurflöte, "
        "Schlüssel) bleiben, wo sie sind.", &itemSettings.fieldItems, this, status);
    itemHidden = addOption(world->body(), world, "Versteckte Items",
        "Die unsichtbaren Items, die du mit dem Itemradar oder durch Absuchen findest – z. B. Sternenstücke, "
        "Herzschuppen oder Beeren-Pflanzen.", &itemSettings.hiddenItems, this, status);
    layout->addWidget(world);

    auto* other = new Card("Läden und Trainer", QString(), content);
    itemShops = addOption(other->body(), other, "Laden-Angebote zufällig",
        "Die Angebote in den Pokémon-Centern, im Kaufhaus Schleiede und in den kleinen Läden werden zufällig. "
        "Pokébälle, Tränke, Beleber, Heiler und Schutz bleiben immer erhältlich, TMs und Honig ebenfalls.",
        &itemSettings.shops, this, status);
    itemTrainers = addOption(other->body(), other, "Items der Trainer-Pokémon zufällig",
        "Trainer-Pokémon, die im Original ein Item tragen, bekommen ein zufälliges Kampf-Item "
        "(z. B. Überreste, Leben-Orb, Wahlschal, Fokusgurt oder typverstärkende Items).",
        &itemSettings.trainerItems, this, status);
    layout->addWidget(other);
    layout->addStretch();

    refreshItemsPage();
    return wrapPage("Items", "Gegenstände in der Spielwelt, in Läden und bei Trainern.", content);
}

void BdspRandomizerWindow::refreshItemsPage() {
    if (itemMode == nullptr) return;
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (QWidget* w : std::initializer_list<QWidget*>{itemMode, itemField, itemHidden, itemShops, itemTrainers}) {
        blockers.push_back(std::make_unique<QSignalBlocker>(w));
    }
    itemMode->setCurrent(itemSettings.mode);
    itemField->setChecked(itemSettings.fieldItems);
    itemHidden->setChecked(itemSettings.hiddenItems);
    itemShops->setChecked(itemSettings.shops);
    itemTrainers->setChecked(itemSettings.trainerItems);
}

// ----------------------------------------------------------- Pokemon-Daten

QWidget* BdspRandomizerWindow::buildDataPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto status = [this]() { updateStatus(); };
    auto* evo = new Card("Entwicklungen", QString(), content);
    dataTradeEvos = addOption(evo->body(), evo, "Tausch-Entwicklungen ohne Tausch",
        "Pokémon, die sich nur durch Tausch entwickeln, entwickeln sich jetzt allein:\n"
        "• Kadabra, Maschock, Georok und Alpollo ab Level 37\n"
        "• Tausch mit Item (z. B. Onix mit Metallmantel, Seedraking mit Drachenschuppe, Porygon mit Up-Grade): "
        "Level-Aufstieg, während es das Item trägt",
        &dataSettings.tradeEvolutions, this, status);
    layout->addWidget(evo);

    auto* values = new Card("Werte und Fähigkeiten", QString(), content);
    dataAbilities = addOption(values->body(), values, "Fähigkeiten zufällig",
        "Jedes Pokémon bekommt zufällige Fähigkeiten (normal und versteckt). Es kommen nur Fähigkeiten vor, die es "
        "im Spiel gibt. Spezial-Fähigkeiten wie Wunderwache, Prognose oder Variabilität werden weder vergeben noch "
        "weggenommen.", &dataSettings.abilities, this, status);
    dataTypes = addOption(values->body(), values, "Typen zufällig",
        "Die Typen werden neu verteilt. Einfache Pokémon bleiben einfach, doppelte bleiben doppelt. Ein Feuer-Pokémon "
        "wird also z. B. komplett zum Psycho-Pokémon.", &dataSettings.types, this, status);
    dataStats = addOption(values->body(), values, "Basiswerte mischen",
        "Die sechs Basiswerte werden untereinander vertauscht – die Summe bleibt gleich. Ein schneller Angreifer kann so "
        "zu einem langsamen Verteidiger werden. Ninjatom behält seinen 1 KP.", &dataSettings.stats, this, status);
    dataFamilies = addOption(values->body(), values, "Entwicklungsreihen einheitlich",
        "Empfohlen: Alle Pokémon einer Entwicklungsreihe bekommen dieselben Fähigkeiten, dieselbe Typ-Zuordnung und "
        "dieselbe Werte-Verteilung (Chelast, Chelcarain und Chelterrar passen also zusammen).",
        &dataSettings.keepFamilies, this);
    layout->addWidget(values);

    auto* moves = new Card("Attacken", QString(), content);
    dataMoves = addOption(moves->body(), moves, "Level-Attacken zufällig",
        "Jedes Pokémon lernt per Level zufällige Attacken – etwa zwei Drittel Schadensattacken, davon die Hälfte vom "
        "eigenen Typ. Schwache Attacken kommen früh, starke spät. Die erste Attacke ist immer eine Schadensattacke.",
        &dataSettings.levelMoves, this, status);
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("TM-Kompatibilität", moves));
        row->addWidget(new InfoButton("Original: wie im Spiel.\n"
                                      "Zufällig: jedes Pokémon kann genauso viele TMs lernen wie vorher, aber andere.\n"
                                      "Alle: jedes Pokémon kann jede TM lernen.\n\n"
                                      "Die VMs gibt es in Strahlender Diamant und Leuchtende Perle nicht mehr – "
                                      "Zerschneider, Surfer & Co. setzt du über die Pokétch-App ein, dafür braucht kein "
                                      "Pokémon die Attacke.", moves));
        row->addStretch();
        dataTMs = new SegmentedControl({"Original", "Zufällig", "Alle"}, moves);
        row->addWidget(dataTMs);
        moves->body()->addLayout(row);
        connect(dataTMs, &SegmentedControl::changed, this, [this](int v) {
            dataSettings.tmMode = v;
            updateStatus();
        });
    }
    layout->addWidget(moves);
    layout->addWidget(mutedLabel("Starter, Trainer, wilde Pokémon und der Kampfturm berücksichtigen die neuen Daten "
                                 "automatisch (z. B. Typ-Arenen und starke Movesets).", content));
    layout->addStretch();

    refreshDataPage();
    return wrapPage("Pokémon-Daten", "Entwicklungen, Typen, Werte, Fähigkeiten und Attacken der Pokémon.", content);
}

void BdspRandomizerWindow::refreshDataPage() {
    if (dataTMs == nullptr) return;
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (QWidget* w : std::initializer_list<QWidget*>{dataTradeEvos, dataAbilities, dataTypes, dataStats, dataFamilies,
                                                     dataMoves, dataTMs}) {
        blockers.push_back(std::make_unique<QSignalBlocker>(w));
    }
    dataTradeEvos->setChecked(dataSettings.tradeEvolutions);
    dataAbilities->setChecked(dataSettings.abilities);
    dataTypes->setChecked(dataSettings.types);
    dataStats->setChecked(dataSettings.stats);
    dataFamilies->setChecked(dataSettings.keepFamilies);
    dataMoves->setChecked(dataSettings.levelMoves);
    dataTMs->setCurrent(dataSettings.tmMode);
}

// --------------------------------------------------------------- Kampfturm

QWidget* BdspRandomizerWindow::buildTowerPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    {
        auto* row = new QHBoxLayout();
        row->setSpacing(8);
        towerBox = new QCheckBox("Kampfturm-Teams randomisieren", content);
        towerBox->setObjectName("masterSwitch");
        row->addWidget(towerBox);
        row->addWidget(new InfoButton(
            "Alle Pokémon, aus denen die Gegner im Kampfturm ihre Teams zusammenstellen – in Einzel- und Doppelkämpfen, "
            "auch die Kampfturm-Meister. Sie bekommen starke Movesets für Level 50, passende Fähigkeiten und Kampf-Items. "
            "Kein Team enthält eine Art doppelt.", content));
        row->addStretch();
        layout->addLayout(row);
        connect(towerBox, &QCheckBox::toggled, this, [this](bool on) {
            towerSettings.enabled = on;
            towerContent->setEnabled(on);
            updateStatus();
        });
    }
    towerContent = new QWidget(content);
    auto* inner = new QVBoxLayout(towerContent);
    inner->setContentsMargins(0, 0, 0, 0);
    auto* card = new Card("Einstellungen", QString(), towerContent);
    towerEvolvedBox = addOption(card->body(), card, "Nur voll entwickelte Pokémon",
        "Im Kampfturm treten nur Endstufen oder Pokémon ohne Entwicklung an – wie im Original.",
        &towerSettings.fullyEvolved, this);
    towerLegendsBox = addOption(card->body(), card, "Legendäre Pokémon erlauben",
        "Gegner im Kampfturm dürfen auch Legendäre und Mysteriöse Pokémon einsetzen.", &towerSettings.legendaries, this);
    inner->addWidget(card);
    layout->addWidget(towerContent);
    layout->addStretch();

    refreshTowerPage();
    return wrapPage("Kampfturm", "Die Gegner im Kampfturm auf der Kampfzone.", content);
}

void BdspRandomizerWindow::refreshTowerPage() {
    if (towerBox == nullptr) return;
    QSignalBlocker b1(towerBox), b2(towerEvolvedBox), b3(towerLegendsBox);
    towerBox->setChecked(towerSettings.enabled);
    towerEvolvedBox->setChecked(towerSettings.fullyEvolved);
    towerLegendsBox->setChecked(towerSettings.legendaries);
    towerContent->setEnabled(towerSettings.enabled);
}

// ---------------------------------------------------------------- Trainer

QWidget* BdspRandomizerWindow::buildTrainerPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    {
        auto* row = new QHBoxLayout();
        row->setSpacing(8);
        trainerSwitch = new QCheckBox("Trainer randomisieren", content);
        trainerSwitch->setObjectName("masterSwitch");
        row->addWidget(trainerSwitch);
        row->addWidget(new InfoButton("Schaltet die Randomisierung aller Trainer ein oder aus. Gilt für Strahlender "
                                      "Diamant und Leuchtende Perle.", content));
        row->addStretch();
        layout->addLayout(row);
        connect(trainerSwitch, &QCheckBox::toggled, this, [this](bool on) {
            trainerSettings.enabled = on;
            trainerContent->setEnabled(on);
            updateStatus();
        });
    }

    trainerContent = new QWidget(content);
    auto* inner = new QVBoxLayout(trainerContent);
    inner->setContentsMargins(0, 0, 0, 0);
    inner->setSpacing(12);
    layout->addWidget(trainerContent);

    auto* baseCard = new Card("Grundeinstellungen", "gilt für alle Trainer", trainerContent);
    baseEditor = new SwShTrainerEditor(&trainerSettings.base, baseCard, false);
    baseCard->body()->addWidget(baseEditor);
    inner->addWidget(baseCard);

    auto* typeCard = new Card("Typ-Trainer", QString(), trainerContent);
    typeThemeBox = new QCheckBox("Typ-Trainer behalten ihren Typ", typeCard);
    typeCard->body()->addLayout(rowWithInfo(typeThemeBox,
        "Trainer mit festem Typ bekommen nur zufällige Pokémon dieses Typs. Gilt automatisch für alle Gruppen."));
    typeCard->body()->addWidget(mutedLabel(
        "Betrifft: alle acht Arenen mit Arenatrainern und Revanchen (Veit, Silvana, Hilda, Marinus, Lamina, Adam, "
        "Frida, Volkner) sowie die Top Vier (Herbaro, Teresa, Ignaz, Lucian).", typeCard));
    connect(typeThemeBox, &QCheckBox::toggled, this, [this](bool on) { trainerSettings.keepTypeTheme = on; });
    inner->addWidget(typeCard);

    auto* groupsCard = new Card("Ausnahmen für einzelne Gruppen", "optional", trainerContent);
    groupsCard->body()->addWidget(mutedLabel("Normalerweise gelten die Grundeinstellungen für alle. Hier kannst du "
                                             "einzelnen Gruppen eigene Einstellungen geben oder sie unverändert lassen.",
                                             groupsCard));
    auto* collapsible = new Collapsible("Gruppen anzeigen", false, groupsCard);
    groupsCard->body()->addWidget(collapsible);

    for (int g = 0; g < bdsp::GroupCount; g++) {
        const bdsp::GroupInfo info = bdsp::groupInfo(g);
        auto* row = new QWidget(groupsCard);
        row->setObjectName("groupRow");
        row->setAttribute(Qt::WA_StyledBackground, true);
        auto* rowLayout = new QVBoxLayout(row);
        rowLayout->setContentsMargins(0, 6, 0, 6);
        rowLayout->setSpacing(6);

        auto* head = new QHBoxLayout();
        head->addWidget(new QLabel(info.title, row));
        head->addWidget(new InfoButton(info.info, row));
        head->addStretch();
        groupSelectors[g] = new SegmentedControl({"Wie oben", "Eigene", "Nicht ändern"}, row);
        head->addWidget(groupSelectors[g]);
        rowLayout->addLayout(head);

        groupEditorBoxes[g] = new QWidget(row);
        groupEditorBoxes[g]->setObjectName("subEditor");
        groupEditorBoxes[g]->setAttribute(Qt::WA_StyledBackground, true);
        auto* boxLayout = new QVBoxLayout(groupEditorBoxes[g]);
        boxLayout->setContentsMargins(14, 12, 14, 12);
        groupEditors[g] = new SwShTrainerEditor(&trainerSettings.own[g], groupEditorBoxes[g], false);
        boxLayout->addWidget(groupEditors[g]);
        groupEditorBoxes[g]->setVisible(false);
        rowLayout->addWidget(groupEditorBoxes[g]);
        collapsible->body()->addWidget(row);

        connect(groupSelectors[g], &SegmentedControl::changed, this, [this, g](int mode) {
            trainerSettings.modes[g] = mode;
            if (mode == bdsp::OwnSettings && !ownInitialized[g]) {
                trainerSettings.own[g] = trainerSettings.base; // Startpunkt: die Grundeinstellungen
                ownInitialized[g] = true;
                groupEditors[g]->refresh();
            }
            groupEditorBoxes[g]->setVisible(mode == bdsp::OwnSettings);
        });
    }
    inner->addWidget(groupsCard);
    layout->addStretch();

    refreshTrainerPage();
    return wrapPage("Trainer", "Lege fest, wie die Teams aller Trainer zufällig zusammengestellt werden.", content);
}

void BdspRandomizerWindow::refreshTrainerPage() {
    {
        QSignalBlocker b1(trainerSwitch), b2(typeThemeBox);
        trainerSwitch->setChecked(trainerSettings.enabled);
        typeThemeBox->setChecked(trainerSettings.keepTypeTheme);
    }
    trainerContent->setEnabled(trainerSettings.enabled);
    baseEditor->refresh();
    for (int g = 0; g < bdsp::GroupCount; g++) {
        QSignalBlocker block(groupSelectors[g]);
        groupSelectors[g]->setCurrent(trainerSettings.modes[g]);
        groupEditors[g]->refresh();
        groupEditorBoxes[g]->setVisible(trainerSettings.modes[g] == bdsp::OwnSettings);
    }
}

// ---------------------------------------------------------- Randomisieren

namespace {
quint64 seedFromText(const QString& text) {
    bool ok = false;
    const quint64 value = text.toULongLong(&ok);
    if (ok) return value;
    const QByteArray hash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
    quint64 result = 0;
    for (int i = 0; i < 8; i++) result = (result << 8) | static_cast<quint8>(hash[i]);
    return result;
}
} // namespace

bool BdspRandomizerWindow::randomizeTo(const QString& outputFolder, QString* error) {
    if (!files) {
        if (error) *error = "Die Spieldateien sind nicht geladen.";
        return false;
    }
    QString seedText = seedEdit->text().trimmed();
    if (seedText.isEmpty()) seedText = QString::number(QRandomGenerator::global()->bounded(1000000000));
    const quint64 seed = seedFromText(seedText);

    const QString romfs = QDir::fromNativeSeparators(romfsEdit->text().trimmed());
    bdsp::GameTexts texts;
    texts.load(romfs);

    bdsp::GameFiles work = *files; // Original bleibt fuer weitere Durchlaeufe unveraendert

    // Pokemon-Daten zuerst: Starter, Trainer, wilde Pokemon und Kampfturm richten sich nach den neuen
    // Typen, Attacken und Entwicklungen
    const bdsp::PokemonDataResult dataResult = bdsp::randomizePokemonData(work, texts, dataSettings, seed);

    // Starter, Geschenke, Begegnungen und Tausch zuerst: die Starter von Rivale und Lucius/Lucia
    // werden dabei festgelegt und vom Trainer-Randomizer nicht mehr veraendert
    const QString exefs = QDir::fromNativeSeparators(exefsEdit->text().trimmed());
    bdsp::ScriptData scripts; // Skripte werden von mehreren Bereichen geaendert und einmal gespeichert
    bdsp::EncounterResult encounters;
    QString stepError;
    if (!bdsp::randomizeEncounters(romfs, exefs, work, scripts, encounterSettings, seed, encounters, &stepError)) {
        if (error) *error = stepError;
        return false;
    }
    const bdsp::TrainerRandomizerResult result = bdsp::randomizeTrainers(work, trainerSettings, seed);

    // Items nach den Trainern: Trainer-Items ersetzen die Items der neuen Teams
    bdsp::ItemResult items;
    if (!bdsp::randomizeItems(romfs, work, scripts, itemSettings, seed, items, &stepError)) {
        if (error) *error = stepError;
        return false;
    }
    bdsp::WildResult wild;
    if (!bdsp::randomizeWild(romfs, work, wildSettings, check.version, seed, wild, &stepError)) {
        if (error) *error = stepError;
        return false;
    }
    bdsp::TowerResult tower;
    if (!bdsp::randomizeTower(work, towerSettings, seed, tower, &stepError)) {
        if (error) *error = stepError;
        return false;
    }

    QDir(outputFolder).removeRecursively();
    const QString romfsOut = outputFolder + "/romfs";
    QString saveError;
    if (!work.save(romfsOut, &saveError) || !bdsp::writeEncounters(outputFolder, encounters, &saveError) ||
        !bdsp::writeWild(romfsOut, wild, &saveError) || !scripts.save(romfsOut, &saveError)) {
        if (error) *error = "Die Dateien konnten nicht geschrieben werden:\n" + saveError;
        return false;
    }
    lastHadExefsPatch = !encounters.ips.isEmpty();

    if (spoilerLog->isChecked()) {
        QList<bdsp::SpoilerSection> sections = bdsp::encounterSpoiler(encounters, work, texts);
        sections += bdsp::wildSpoiler(wild, work, texts);
        sections += bdsp::itemSpoiler(items, texts);
        sections += bdsp::pokemonDataSpoiler(dataResult, tower, work, texts);
        bdsp::writeSpoiler(outputFolder + "/Spoiler-Log.html", work, texts, trainerSettings, sections, seedText,
                           check.version);
    }

    QFile info(outputFolder + "/Info.txt");
    if (info.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&info);
        out << "Pokémon Strahlender Diamant/Leuchtende Perle Randomizer\n";
        out << "Erstellt: " << QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm") << "\n";
        out << "Seed: " << seedText << "\n";
        out << "Spiel: " << bdsp::versionName(check.version) << "\n";
        out << "Geänderte Trainer: " << result.randomized << "\n";
        out << "Geänderte Starter/Geschenke/Begegnungen/Tausch: " << encounters.changes.size() << "\n";
        int wildAreas = static_cast<int>(wild.areas.size() + wild.undergroundLog.size());
        out << "Geänderte Gebiete mit wilden Pokémon: " << wildAreas << "\n";
        out << "Geänderte Items: " << items.fieldItems + items.hiddenItems << " (Boden/versteckt), " << items.shopItems
            << " (Läden), " << items.trainerItems << " (Trainer)\n";
        out << "Pokémon-Daten: " << dataResult.types << " Typen, " << dataResult.stats << " Basiswerte, "
            << dataResult.abilities << " Fähigkeiten, " << dataResult.learnsets << " Lernsets, " << dataResult.tms
            << " TM-Listen, " << dataResult.evolutions.size() << " Tausch-Entwicklungen\n";
        out << "Kampfturm-Pokémon: " << tower.pokemon.size() << "\n";
        if (!encounters.ips.isEmpty()) out << "ExeFS-Patch: exefs/" << encounters.ipsName << "\n";
        out << "\nTrainer-Einstellungen:\n" << QJsonDocument(bdsp::settingsToJson(trainerSettings)).toJson() << "\n";
        out << "Starter & Geschenke:\n" << QJsonDocument(bdsp::encounterSettingsToJson(encounterSettings)).toJson() << "\n";
        out << "Wilde Pokémon:\n" << QJsonDocument(bdsp::wildSettingsToJson(wildSettings)).toJson() << "\n";
        out << "Items:\n" << QJsonDocument(bdsp::itemSettingsToJson(itemSettings)).toJson() << "\n";
        out << "Pokémon-Daten:\n" << QJsonDocument(bdsp::pokemonDataSettingsToJson(dataSettings)).toJson() << "\n";
        out << "Kampfturm:\n" << QJsonDocument(bdsp::towerSettingsToJson(towerSettings)).toJson();
    }
    lastSeed = seedText;
    return true;
}

void BdspRandomizerWindow::startRandomizer() {
    saveSettings();
    const QString output = QDir::current().filePath(QString(kOutputFolder) + "/Randomizer-1");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = randomizeTo(output, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, "Randomisieren", error);
        return;
    }

    bool copied = false;
    QString copyError;
    if (modDirect->isChecked()) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        copied = copyToModFolder(output, &copyError);
        QApplication::restoreOverrideCursor();
    }

    QMessageBox box(this);
    box.setWindowTitle("Fertig");
    box.setIcon(copyError.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    const QString tid = bdsp::titleId(check.version);
    box.setText("Der Randomizer ist fertig (Seed " + lastSeed + ").");
    if (copied) {
        box.setInformativeText(QString(lastHadExefsPatch ? "Die Ordner „romfs“ und „exefs“ wurden" : "Der Ordner „romfs“ wurde") +
                               " direkt in deinen Mod-Ordner geschrieben:\n" +
                               QDir::toNativeSeparators(modFolderEdit->text().trimmed()) +
                               "\n\nStarte das Spiel im Emulator (neu) – der Mod ist sofort aktiv. Eine Kopie liegt "
                               "außerdem in Randomizer-1.");
    } else if (!copyError.isEmpty()) {
        box.setInformativeText(copyError + "\n\nDie Dateien liegen trotzdem in Randomizer-1 und können von Hand "
                               "kopiert werden.");
    } else {
        QString text =
            "Kopiere den Ordner „romfs“ aus Randomizer-1 in einen Mod-Ordner deines Spiels:\n\n"
            "• Ryujinx: Rechtsklick auf das Spiel → „Mod-Verzeichnis öffnen“, dort einen Ordner (z. B. Randomizer) "
            "anlegen und „romfs“ hineinkopieren.\n"
            "• Switch (Atmosphère): atmosphere/contents/" + (tid.isEmpty() ? QString("<Title-ID>") : tid) + "/romfs";
        if (lastHadExefsPatch) {
            text += "\n\nWichtig: Es gibt zusätzlich einen Ordner „exefs“ (Patch für Starter/Evoli). Bei Emulatoren "
                    "kommt er neben „romfs“ in denselben Mod-Ordner. Auf der Switch gehört die .ips-Datei nach "
                    "atmosphere/exefs_patches/Randomizer/.";
        }
        box.setInformativeText(text);
    }
    QPushButton* openMod = copied ? box.addButton("Mod-Ordner öffnen", QMessageBox::ActionRole) : nullptr;
    QPushButton* openFolder = box.addButton("Ordner öffnen", QMessageBox::ActionRole);
    QPushButton* openSpoiler = spoilerLog->isChecked() ? box.addButton("Spoiler-Log öffnen", QMessageBox::ActionRole) : nullptr;
    box.addButton(QMessageBox::Close);
    box.exec();
    if (openMod != nullptr && box.clickedButton() == openMod) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::fromNativeSeparators(modFolderEdit->text().trimmed())));
    } else if (box.clickedButton() == openFolder) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(output));
    } else if (openSpoiler != nullptr && box.clickedButton() == openSpoiler) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(output + "/Spoiler-Log.html"));
    }
}

// ------------------------------------------------------------ Vorlagen

void BdspRandomizerWindow::saveTemplate() {
    const QString dir = modoutput::templateFolder(modoutput::kBdsp);
    QString path = QFileDialog::getSaveFileName(this, "Einstellungen speichern", dir + "/Diamant-Perle.json",
                                                "Randomizer-Vorlage (*.json)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".json", Qt::CaseInsensitive)) path += ".json";
    QJsonObject json = settingsJson();
    json["format"] = "Pokemon-Randomizer-Vorlage";
    json["game"] = "DiamantPerle";
    json["formatVersion"] = 1;
    if (!bdsp::writeFile(path, QJsonDocument(json).toJson(QJsonDocument::Indented))) {
        QMessageBox::warning(this, "Einstellungen speichern", "Die Datei konnte nicht gespeichert werden:\n" + path);
        return;
    }
    QMessageBox::information(this, "Einstellungen speichern", "Die Vorlage wurde gespeichert:\n" + QDir::toNativeSeparators(path));
}

void BdspRandomizerWindow::loadTemplate() {
    const QString dir = modoutput::templateLoadFolder(modoutput::kBdsp, {QDir::current().filePath("Randomizers-Output/Vorlagen")});
    const QString path = QFileDialog::getOpenFileName(this, "Einstellungen laden", QDir(dir).exists() ? dir : QDir::currentPath(),
                                                      "Randomizer-Vorlage (*.json)");
    if (path.isEmpty()) return;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bdsp::readFile(path), &parseError);
    if (!doc.isObject()) {
        QMessageBox::warning(this, "Einstellungen laden", "Die Datei ist keine gültige Vorlage:\n" + parseError.errorString());
        return;
    }
    const QJsonObject json = doc.object();
    if (json.value("game").toString() != "DiamantPerle") {
        QMessageBox::warning(this, "Einstellungen laden",
                             "Diese Vorlage gehört nicht zu Strahlender Diamant/Leuchtende Perle und kann hier nicht "
                             "geladen werden.");
        return;
    }
    applySettingsJson(json);
    saveSettings();
    QMessageBox::information(this, "Einstellungen laden", "Die Vorlage wurde geladen:\n" + QFileInfo(path).fileName());
}

// ------------------------------------------------------- Mod-Ordner

void BdspRandomizerWindow::findEmulator() {
    const QString tid = bdsp::titleId(check.version);
    if (tid.isEmpty()) {
        QMessageBox::information(this, "Emulator suchen",
                                 "Wähle zuerst oben deine Spieldateien aus – die Title-ID (Strahlender Diamant oder "
                                 "Leuchtende Perle) wird aus der ExeFS erkannt.");
        return;
    }
    const QString path = modoutput::askEmulatorModFolder(this, {{bdsp::versionName(check.version), tid}});
    if (path.isEmpty()) return;
    modFolderEdit->setText(QDir::toNativeSeparators(path));
    modDirect->setChecked(true);
    saveSettings();
}

bool BdspRandomizerWindow::copyToModFolder(const QString& output, QString* error) const {
    return modoutput::copyToModFolder(output, modFolderEdit->text(), {QDir::fromNativeSeparators(romfsEdit->text().trimmed()),
                                       QDir::fromNativeSeparators(exefsEdit->text().trimmed())}, true, error);
}

// ------------------------------------------------------------ Speichern

QJsonObject BdspRandomizerWindow::settingsJson() const {
    QJsonObject json;
    json["seed"] = seedEdit->text();
    json["spoilerLog"] = spoilerLog->isChecked();
    json["trainer"] = bdsp::settingsToJson(trainerSettings);
    json["encounters"] = bdsp::encounterSettingsToJson(encounterSettings);
    json["wild"] = bdsp::wildSettingsToJson(wildSettings);
    json["items"] = bdsp::itemSettingsToJson(itemSettings);
    json["data"] = bdsp::pokemonDataSettingsToJson(dataSettings);
    json["tower"] = bdsp::towerSettingsToJson(towerSettings);
    return json;
}

void BdspRandomizerWindow::applySettingsJson(const QJsonObject& json) {
    if (json.contains("seed")) seedEdit->setText(json.value("seed").toString());
    if (json.contains("spoilerLog")) spoilerLog->setChecked(json.value("spoilerLog").toBool(true));
    if (json.value("trainer").isObject()) {
        trainerSettings = bdsp::TrainerSettings();
        bdsp::settingsFromJson(json.value("trainer").toObject(), trainerSettings);
        for (int g = 0; g < bdsp::GroupCount; g++) ownInitialized[g] = trainerSettings.modes[g] == bdsp::OwnSettings;
    }
    if (json.value("encounters").isObject()) {
        encounterSettings = bdsp::EncounterSettings();
        bdsp::encounterSettingsFromJson(json.value("encounters").toObject(), encounterSettings);
    }
    if (json.value("wild").isObject()) {
        wildSettings = bdsp::WildSettings();
        bdsp::wildSettingsFromJson(json.value("wild").toObject(), wildSettings);
    }
    if (json.value("items").isObject()) {
        itemSettings = bdsp::ItemSettings();
        bdsp::itemSettingsFromJson(json.value("items").toObject(), itemSettings);
    }
    if (json.value("data").isObject()) {
        dataSettings = bdsp::PokemonDataSettings();
        bdsp::pokemonDataSettingsFromJson(json.value("data").toObject(), dataSettings);
    }
    if (json.value("tower").isObject()) {
        towerSettings = bdsp::TowerSettings();
        bdsp::towerSettingsFromJson(json.value("tower").toObject(), towerSettings);
    }
    refreshTrainerPage();
    refreshStartersPage();
    refreshWildPage();
    refreshItemsPage();
    refreshDataPage();
    refreshTowerPage();
    if (activeInfo) updateStatus();
}

void BdspRandomizerWindow::saveSettings() const {
    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.setValue("RomFS", romfsEdit->text().trimmed());
    settings.setValue("ExeFS", exefsEdit->text().trimmed());
    settings.setValue("ModFolder", modFolderEdit->text().trimmed());
    settings.setValue("ModDirect", modDirect->isChecked());
    settings.setValue("Settings", QString::fromUtf8(QJsonDocument(settingsJson()).toJson(QJsonDocument::Compact)));
}

void BdspRandomizerWindow::loadSettings() {
    QSettings settings(kSettingsOrg, kSettingsApp);
    applySettingsJson(QJsonDocument::fromJson(settings.value("Settings").toString().toUtf8()).object());
    modFolderEdit->setText(settings.value("ModFolder").toString());
    modDirect->setChecked(settings.value("ModDirect", false).toBool());
    const QString romfs = settings.value("RomFS").toString();
    const QString exefs = settings.value("ExeFS").toString();
    if (romfs.isEmpty() && exefs.isEmpty()) {
        updateStatus();
        messageArea->hide();
        return;
    }
    romfsEdit->setText(romfs);
    exefsEdit->setText(exefs);
    checkPaths();
}
