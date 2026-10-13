#include "headers/bdsp/BdspRandomizerWindow.h"
#include "headers/modern_ui/modern_widgets.h"
#include "headers/swsh/SwShTrainerEditor.h"

#include <QApplication>
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

using namespace modernui;

namespace {
const QStringList kPages = {"Start", "Trainer", "Starter & Geschenke", "Wilde Pokémon", "Items", "Pokémon-Daten"};

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
    pages->addWidget(buildComingSoonPage("Starter & Geschenke", "Starter, geschenkte Pokémon, statische Begegnungen und Tausch.",
        "Dieser Bereich folgt im nächsten Schritt: zufällige Starter (auch nach Wunsch), Geschenke wie Evoli oder "
        "Togepi-Ei, Legendäre und andere statische Begegnungen sowie Tauschpartner."));
    pages->addWidget(buildComingSoonPage("Wilde Pokémon", "Gras, Höhlen, Wasser, Angeln, Honigbäume und der Große Sumpf.",
        "Dieser Bereich folgt in einem der nächsten Schritte."));
    pages->addWidget(buildComingSoonPage("Items", "Items auf dem Boden, versteckte Items und Läden.",
        "Dieser Bereich folgt in einem der nächsten Schritte."));
    pages->addWidget(buildComingSoonPage("Pokémon-Daten", "Fähigkeiten, Typen, Basiswerte, Attacken und Entwicklungen.",
        "Dieser Bereich folgt in einem der nächsten Schritte."));
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
        "Erstellt zusätzlich die Datei Spoiler-Log.html mit allen Trainer-Teams. Öffnet sich im Browser und ist "
        "durchsuchbar."));
    layout->addWidget(general);

    // Ausgabe
    auto* output = new Card("Ausgabe", QString(), content);
    output->body()->addWidget(mutedLabel(
        "Die Ergebnisse landen im Ordner Randomizers-Output → Diamant-Perle → Randomizer-1. Dort liegt ein Ordner "
        "„romfs“ mit nur den geänderten Dateien. Für Emulatoren kopierst du ihn in einen eigenen Mod-Ordner des "
        "Spiels (oder lässt ihn unten direkt dorthin schreiben), für die Switch nach atmosphere/contents/<Title-ID>/.", output));
    modDirect = new QCheckBox("Direkt in den Mod-Ordner des Emulators schreiben", output);
    output->body()->addLayout(rowWithInfo(modDirect,
        "Nach dem Randomisieren wird der Ordner „romfs“ zusätzlich direkt in den Mod-Ordner kopiert. Ein älterer "
        "„romfs“-Ordner dort wird dabei ersetzt – du musst also nichts mehr von Hand kopieren.\n\n"
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

QWidget* BdspRandomizerWindow::buildComingSoonPage(const QString& title, const QString& subtitle, const QString& text) {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* card = new Card("In Arbeit", "Bald verfügbar", content);
    card->body()->addWidget(mutedLabel(text, card));
    layout->addWidget(card);
    return wrapPage(title, subtitle, content);
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
    return areas;
}

void BdspRandomizerWindow::updateStatus() {
    const QStringList areas = activeAreas();
    if (files) {
        const QString game = check.version == bdsp::Version::Unknown ? QString("Strahlender Diamant/Leuchtende Perle")
                                                                    : bdsp::versionName(check.version);
        activeInfo->setText(areas.isEmpty() ? QString("Noch nichts ausgewählt") : "Aktiv: " + areas.join(", "));
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
    updateStatus();
}

void BdspRandomizerWindow::showPage(int index) {
    nav->setCurrentRow(index);
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
    const bdsp::TrainerRandomizerResult result = bdsp::randomizeTrainers(work, trainerSettings, seed);

    QDir(outputFolder).removeRecursively();
    const QString romfsOut = outputFolder + "/romfs";
    QString saveError;
    if (!work.save(romfsOut, &saveError)) {
        if (error) *error = "Die Dateien konnten nicht geschrieben werden:\n" + saveError;
        return false;
    }

    if (spoilerLog->isChecked()) {
        bdsp::writeSpoiler(outputFolder + "/Spoiler-Log.html", work, texts, trainerSettings, {}, seedText, check.version);
    }

    QFile info(outputFolder + "/Info.txt");
    if (info.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&info);
        out << "Pokémon Strahlender Diamant/Leuchtende Perle Randomizer\n";
        out << "Erstellt: " << QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm") << "\n";
        out << "Seed: " << seedText << "\n";
        out << "Spiel: " << bdsp::versionName(check.version) << "\n";
        out << "Geänderte Trainer: " << result.randomized << "\n\n";
        out << "Trainer-Einstellungen:\n" << QJsonDocument(bdsp::settingsToJson(trainerSettings)).toJson();
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
        box.setInformativeText("Der Ordner „romfs“ wurde direkt in deinen Mod-Ordner geschrieben:\n" +
                               QDir::toNativeSeparators(modFolderEdit->text().trimmed()) +
                               "\n\nStarte das Spiel im Emulator (neu) – der Mod ist sofort aktiv. Eine Kopie liegt "
                               "außerdem in Randomizer-1.");
    } else if (!copyError.isEmpty()) {
        box.setInformativeText(copyError + "\n\nDie Dateien liegen trotzdem in Randomizer-1 und können von Hand "
                               "kopiert werden.");
    } else {
        box.setInformativeText(
            "Kopiere den Ordner „romfs“ aus Randomizer-1 in einen Mod-Ordner deines Spiels:\n\n"
            "• Ryujinx: Rechtsklick auf das Spiel → „Mod-Verzeichnis öffnen“, dort einen Ordner (z. B. Randomizer) "
            "anlegen und „romfs“ hineinkopieren.\n"
            "• Switch (Atmosphère): atmosphere/contents/" + (tid.isEmpty() ? QString("<Title-ID>") : tid) + "/romfs");
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
    const QString dir = QDir::current().filePath("Randomizers-Output/Vorlagen");
    QDir().mkpath(dir);
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
    const QString dir = QDir::current().filePath("Randomizers-Output/Vorlagen");
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
    QStringList roots;
    const QString appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty()) roots << appData;
    roots << QDir::homePath() + "/.config" << QDir::homePath() + "/.local/share"
          << QDir::homePath() + "/Library/Application Support";
    struct Emulator { QString name, folder, sub; bool lower; };
    const QList<Emulator> emulators = {
        {"Ryujinx", "Ryujinx", "mods/contents", true},
        {"yuzu", "yuzu", "load", false},
        {"suyu", "suyu", "load", false},
        {"sudachi", "sudachi", "load", false},
        {"citron", "citron", "load", false},
        {"Eden", "eden", "load", false},
    };
    QStringList labels, paths;
    for (const QString& root : roots) {
        for (const Emulator& e : emulators) {
            const QString base = root + "/" + e.folder;
            if (!QDir(base).exists()) continue;
            const QString path = QDir::cleanPath(base + "/" + e.sub + "/" + (e.lower ? tid.toLower() : tid) + "/Randomizer");
            if (paths.contains(path)) continue;
            labels << e.name + "  –  " + QDir::toNativeSeparators(path);
            paths << path;
        }
    }
    if (paths.isEmpty()) {
        QMessageBox::information(this, "Emulator suchen",
                                 "Es wurde kein Emulator gefunden (Ryujinx, yuzu, suyu, sudachi, citron, Eden).\n\n"
                                 "Wähle den Mod-Ordner bitte über „Durchsuchen“ aus. In Ryujinx findest du ihn per "
                                 "Rechtsklick auf das Spiel → „Mod-Verzeichnis öffnen“.");
        return;
    }
    int choice = 0;
    if (paths.size() > 1) {
        bool ok = false;
        const QString picked = QInputDialog::getItem(this, "Emulator suchen", "Mehrere Emulatoren gefunden:", labels, 0,
                                                     false, &ok);
        if (!ok) return;
        choice = static_cast<int>(labels.indexOf(picked));
    }
    modFolderEdit->setText(QDir::toNativeSeparators(paths[choice]));
    modDirect->setChecked(true);
    saveSettings();
}

namespace {
bool copyFolder(const QString& from, const QString& to) {
    QDir source(from);
    if (!QDir().mkpath(to)) return false;
    for (const QFileInfo& entry : source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString target = to + "/" + entry.fileName();
        if (entry.isDir()) {
            if (!copyFolder(entry.absoluteFilePath(), target)) return false;
        } else if (!QFile::copy(entry.absoluteFilePath(), target)) {
            return false;
        }
    }
    return true;
}
} // namespace

bool BdspRandomizerWindow::copyToModFolder(const QString& output, QString* error) const {
    const QString folder = QDir::cleanPath(QDir::fromNativeSeparators(modFolderEdit->text().trimmed()));
    if (folder.isEmpty() || QDir(folder).isRoot()) {
        if (error) *error = "Bitte gib einen Mod-Ordner an.";
        return false;
    }
    const QString target = folder + "/romfs";
    // Schutz: niemals den eigenen Dump ersetzen
    const QString dump = QFileInfo(QDir::fromNativeSeparators(romfsEdit->text().trimmed())).canonicalFilePath();
    const QString targetCanonical = QFileInfo(target).canonicalFilePath();
    if (!dump.isEmpty() && !targetCanonical.isEmpty() &&
        (dump == targetCanonical || dump.startsWith(targetCanonical + "/"))) {
        if (error) *error = "Der Mod-Ordner zeigt auf deinen Dump. Bitte wähle einen eigenen Mod-Ordner.";
        return false;
    }
    if (QDir(target).exists() && !QDir(target).removeRecursively()) {
        if (error) *error = "Der alte Ordner „romfs“ im Mod-Ordner konnte nicht gelöscht werden:\n" +
                            QDir::toNativeSeparators(target);
        return false;
    }
    if (!copyFolder(output + "/romfs", target)) {
        if (error) *error = "Die Dateien konnten nicht in den Mod-Ordner kopiert werden:\n" + QDir::toNativeSeparators(target);
        return false;
    }
    return true;
}

// ------------------------------------------------------------ Speichern

QJsonObject BdspRandomizerWindow::settingsJson() const {
    QJsonObject json;
    json["seed"] = seedEdit->text();
    json["spoilerLog"] = spoilerLog->isChecked();
    json["trainer"] = bdsp::settingsToJson(trainerSettings);
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
    refreshTrainerPage();
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
