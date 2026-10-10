#include "headers/swsh/SwShRandomizerWindow.h"
#include "headers/modern_ui/modern_widgets.h"
#include "headers/swsh/SwShTrainerEditor.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QScrollArea>
#include <QSettings>
#include <QStyle>
#include <QUrl>
#include <QDateTime>
#include <QJsonDocument>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QFile>
#include <QTextStream>
#include <QCompleter>
#include <functional>
#include <memory>
#include <vector>
#include <QLineEdit>

using namespace modernui;

namespace {
const QStringList kPages = {"Start", "Trainer", "Starter & Geschenke", "Wilde Pokémon", "Dyna-Raids", "Items",
                            "Pokémon-Daten", "Dyna-Höhle & Kampfturm"};

const char* kSettingsOrg = "Pokemon Randomizer";
const char* kSettingsApp = "SchwertSchild";
} // namespace

SwShRandomizerWindow::SwShRandomizerWindow(QWidget* parent) : QWidget(parent) {
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
    pages->addWidget(buildRaidPage());
    pages->addWidget(buildItemPage());
    pages->addWidget(buildDataPage());
    pages->addWidget(buildFacilityPage());
    middle->addWidget(pages, 1);

    root->addLayout(middle, 1);
    root->addWidget(buildBottomBar());

    connect(nav, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    nav->setCurrentRow(0);

    loadSettings();
}

SwShRandomizerWindow::~SwShRandomizerWindow() {
    saveSettings();
}

// --------------------------------------------------------------- Aufbau

QWidget* SwShRandomizerWindow::buildSidebar() {
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
    connect(back, &QPushButton::clicked, this, &SwShRandomizerWindow::backRequested);
    layout->addWidget(back, 0, Qt::AlignLeft);
    layout->addSpacing(6);

    auto* title = new QLabel("Galar Randomizer", sidebar);
    title->setObjectName("appTitle");
    auto* subtitle = new QLabel("Schwert und Schild", sidebar);
    subtitle->setObjectName("appSubtitle");
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

QWidget* SwShRandomizerWindow::buildBottomBar() {
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
    connect(startButton, &QPushButton::clicked, this, &SwShRandomizerWindow::startRandomizer);
    layout->addWidget(startButton);
    return bar;
}

QWidget* SwShRandomizerWindow::wrapPage(const QString& title, const QString& subtitle, QWidget* content) {
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
    if (!subtitle.isEmpty()) {
        column->addWidget(mutedLabel(subtitle, columnWidget));
    }
    column->addSpacing(4);
    content->setParent(columnWidget);
    column->addWidget(content);
    column->addStretch();

    columnWidget->setMaximumWidth(1000);
    outer->addWidget(columnWidget, 1);

    scroll->setWidget(inner);
    return scroll;
}

QHBoxLayout* SwShRandomizerWindow::pathRow(QWidget* parent, const QString& caption, const QString& info,
                                           QLineEdit*& edit, bool romfs) {
    auto* row = new QHBoxLayout();
    row->setSpacing(8);
    auto* label = new QLabel(caption, parent);
    label->setFixedWidth(60);
    row->addWidget(label);
    row->addWidget(new InfoButton(info, parent));
    edit = new QLineEdit(parent);
    edit->setPlaceholderText(romfs ? "z. B. …\\Dump\\Pokemon Schwert\\romfs" : "z. B. …\\Dump\\Pokemon Schwert\\exefs");
    row->addWidget(edit, 1);
    auto* button = new QPushButton("Durchsuchen …", parent);
    button->setObjectName("secondary");
    button->setCursor(Qt::PointingHandCursor);
    row->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, romfs]() { browse(romfs); });
    connect(edit, &QLineEdit::editingFinished, this, &SwShRandomizerWindow::checkPaths);
    return row;
}

QWidget* SwShRandomizerWindow::buildStartPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // Spieldateien
    auto* game = new Card("Spieldateien", QString(), content);
    game->header()->addWidget(new InfoButton(
        "Der Randomizer liest die Spieldaten aus deinem eigenen Dump. Benötigt wird Update 1.3.2 "
        "(Schwert oder Schild, mit oder ohne Erweiterungspass).", game));
    game->body()->addWidget(mutedLabel(
        "Wähle die Ordner „romfs“ und „exefs“ deines Dumps aus. Du kannst auch den übergeordneten Spielordner "
        "auswählen – beide Ordner werden dann automatisch erkannt.", game));
    game->body()->addLayout(pathRow(game, "RomFS",
        "Der Ordner „romfs“ aus deinem Dump (mit Update 1.3.2). Er enthält den Unterordner „bin“.",
        romfsEdit, true));
    game->body()->addLayout(pathRow(game, "ExeFS",
        "Der Ordner „exefs“ aus deinem Dump. Daraus wird erkannt, ob es Schwert oder Schild ist. "
        "Er enthält die Datei „main.npdm“.",
        exefsEdit, false));

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
    addStatus("Version", versionLabel, "Der Randomizer setzt Update 1.3.2 voraus, weil sich die Datenformate mit "
                                       "den Erweiterungen geändert haben.");
    addStatus("Inhalt", contentLabel, "Anzahl der gelesenen Pokémon, Attacken und Trainer. Die Gebiete des "
                                      "Erweiterungspasses sind im Update enthalten – der Randomizer funktioniert "
                                      "mit und ohne Erweiterungspass.");
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
        "Die Ergebnisse landen im Ordner Randomizers-Output → Schwert-Schild → Randomizer-1. Dort liegt ein Ordner "
        "„romfs“ mit nur den geänderten Dateien. Für Emulatoren kopierst du ihn in einen eigenen Mod-Ordner des "
        "Spiels, für die Switch nach atmosphere/contents/<Title-ID>/.", output));
    {
        auto* row = new QHBoxLayout();
        auto* open = new QPushButton("Ausgabeordner öffnen", output);
        open->setObjectName("secondary");
        open->setCursor(Qt::PointingHandCursor);
        row->addWidget(open);
        row->addStretch();
        output->body()->addLayout(row);
        connect(open, &QPushButton::clicked, this, []() {
            QString path = QDir::current().filePath("Randomizers-Output/Schwert-Schild");
            QDir().mkpath(path);
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
    }
    layout->addWidget(output);
    layout->addStretch(); // freier Platz nicht auf die Karten verteilen

    return wrapPage("Start", "Spieldateien, allgemeine Einstellungen und Ausgabe.", content);
}

QWidget* SwShRandomizerWindow::buildComingSoonPage(const QString& title, const QString& subtitle,
                                                   const QString& text) {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* card = new Card("In Arbeit", "Bald verfügbar", content);
    card->body()->addWidget(mutedLabel(text, card));
    layout->addWidget(card);
    return wrapPage(title, subtitle, content);
}

// ---------------------------------------------------------- Spieldateien

void SwShRandomizerWindow::browse(bool romfs) {
    QLineEdit* edit = romfs ? romfsEdit : exefsEdit;
    QString start = edit->text();
    if (start.isEmpty()) {
        start = (romfs ? exefsEdit : romfsEdit)->text();
    }
    QString dir = QFileDialog::getExistingDirectory(this, romfs ? "RomFS-Ordner wählen" : "ExeFS-Ordner wählen",
                                                    start);
    if (dir.isEmpty()) {
        return;
    }
    setDumpPaths(romfs ? dir : romfsEdit->text(), romfs ? exefsEdit->text() : dir);
}

void SwShRandomizerWindow::setDumpPaths(const QString& romfsIn, const QString& exefsIn) {
    QString romfs = QDir::fromNativeSeparators(romfsIn.trimmed());
    QString exefs = QDir::fromNativeSeparators(exefsIn.trimmed());

    // Spielordner ausgewaehlt (enthaelt romfs und/oder exefs)?
    auto findChild = [](const QString& folder, const QString& name) -> QString {
        if (folder.isEmpty()) {
            return QString();
        }
        QDir dir(folder);
        for (const QString& entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.compare(name, Qt::CaseInsensitive) == 0) {
                return dir.filePath(entry);
            }
        }
        return QString();
    };
    for (QString* p : {&romfs, &exefs}) {
        if (!p->isEmpty() && !QDir(*p + "/bin").exists() && !QFileInfo::exists(*p + "/main.npdm")) {
            QString r = findChild(*p, "romfs");
            QString e = findChild(*p, "exefs");
            if (!r.isEmpty() || !e.isEmpty()) {
                if (!r.isEmpty()) {
                    romfs = r;
                }
                if (!e.isEmpty()) {
                    exefs = e;
                }
            }
        }
    }
    // ExeFS liegt meist direkt neben dem RomFS
    if (exefs.isEmpty() && !romfs.isEmpty()) {
        exefs = findChild(QFileInfo(romfs).absolutePath(), "exefs");
    }

    romfsEdit->setText(QDir::toNativeSeparators(romfs));
    exefsEdit->setText(QDir::toNativeSeparators(exefs));
    checkPaths();
}

void SwShRandomizerWindow::checkPaths() {
    const QString romfs = QDir::fromNativeSeparators(romfsEdit->text().trimmed());
    const QString exefs = QDir::fromNativeSeparators(exefsEdit->text().trimmed());

    check = swsh::checkDump(romfs, exefs);
    files.reset();

    if (check.ok) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        auto loaded = std::make_unique<swsh::GameFiles>();
        QStringList errors;
        if (loaded->load(romfs, &errors)) {
            files = std::move(loaded);
        } else {
            check.ok = false;
            if (errors.isEmpty()) {
                errors << "Die Spieldaten konnten nicht gelesen werden.";
            }
            check.errors << errors;
        }
        QApplication::restoreOverrideCursor();
    }

    // Spiel
    if (check.version != swsh::Version::Unknown) {
        gameLabel->setText(swsh::versionName(check.version) + "  ·  Title-ID " + swsh::titleId(check.version));
        gameLabel->setObjectName("statusOk");
    } else {
        gameLabel->setText(exefs.isEmpty() ? "nicht erkannt (ExeFS fehlt)" : "nicht erkannt");
        gameLabel->setObjectName(exefs.isEmpty() ? "statusWarn" : "statusError");
    }

    // Version und Inhalt
    if (files) {
        versionLabel->setText("1.3.x ✓");
        versionLabel->setObjectName("statusOk");
        int species = 0;
        for (int i = 1; i <= swsh::kMaxSpecies; i++) {
            if (files->personal.isPresent(files->personal.indexOf(i, 0))) {
                species++;
            }
        }
        int trainers = 0;
        for (const swsh::Trainer& t : files->trainers.all()) {
            if (!t.isPlaceholder()) {
                trainers++;
            }
        }
        contentLabel->setText(QString("%1 Pokémon im Spiel  ·  %2 Attacken  ·  %3 Trainer")
                                  .arg(species).arg(files->moves.all().size()).arg(trainers));
        contentLabel->setObjectName("statusOk");
    } else {
        bool wrongVersion = false;
        for (const QString& e : check.errors) {
            wrongVersion |= e.contains("1.3");
        }
        versionLabel->setText(romfs.isEmpty() ? "–" : (wrongVersion ? "falsche Version" : "nicht geprüft"));
        versionLabel->setObjectName(wrongVersion ? "statusError" : "");
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

void SwShRandomizerWindow::showMessages() {
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
    const bool empty = romfsEdit->text().trimmed().isEmpty() && exefsEdit->text().trimmed().isEmpty();
    if (empty) {
        messageArea->hide();
        return;
    }
    for (const QString& e : check.errors) {
        add("✗  " + e, "statusError");
    }
    for (const QString& w : check.warnings) {
        add("!  " + w, "statusWarn");
    }
    messageArea->setVisible(messageBox->count() > 0);
}

QStringList SwShRandomizerWindow::activeAreas() const {
    QStringList areas;
    if (trainerSettings.enabled) areas << "Trainer";
    if (encounterSettings.starterMode != 0) areas << "Starter";
    if (encounterSettings.gifts) areas << "Geschenke";
    if (encounterSettings.statics || encounterSettings.overworld) areas << "Begegnungen";
    if (encounterSettings.trades) areas << "Tausch";
    if (wildSettings.enabled) areas << "Wilde Pokémon";
    if (raidSettings.enabled) areas << "Dyna-Raids";
    if (itemSettings.anyEnabled()) areas << "Items";
    if (dataSettings.anyEnabled()) areas << "Pokémon-Daten";
    if (facilitySettings.maxLair || facilitySettings.maxLairLegends) areas << "Dyna-Höhle";
    if (facilitySettings.tower) areas << "Kampfturm";
    return areas;
}

void SwShRandomizerWindow::updateStatus() {
    const QStringList areas = activeAreas();
    if (files) {
        QString game = check.version == swsh::Version::Unknown ? QString("Schwert/Schild")
                                                               : swsh::versionName(check.version);
        activeInfo->setText(areas.isEmpty() ? QString("Noch nichts ausgewählt") : "Aktiv: " + areas.join(", "));
        QString seed = seedEdit->text().trimmed();
        detailInfo->setText(game + "  ·  Seed: " + (seed.isEmpty() ? QString("zufällig") : seed));
        startButton->setEnabled(!areas.isEmpty());
        startButton->setToolTip(areas.isEmpty() ? QString("Wähle zuerst aus, was randomisiert werden soll (Trainer, Starter …).")
                                                : QString());
    } else {
        activeInfo->setText("Spieldateien fehlen");
        detailInfo->setText("Gib auf der Start-Seite den RomFS- und ExeFS-Ordner deines Dumps an.");
        startButton->setEnabled(false);
        startButton->setToolTip("Zuerst die Spieldateien auf der Start-Seite angeben.");
    }
}

void SwShRandomizerWindow::refreshAllPages() {
    refreshTrainerPage();
    refreshStartersPage();
    refreshWildPage();
    refreshExtraPages();
    updateStatus();
}

// ------------------------------------------------------------ Wilde Pokemon

QWidget* SwShRandomizerWindow::buildWildPage() {
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
        row->addWidget(new InfoButton("Pokémon im hohen Gras, beim Angeln und an Bäumen sowie die sichtbaren Pokémon in "
                                      "der Spielwelt – auf allen Routen, in der Naturzone, auf der Insel der Rüstung und "
                                      "in der Krone-Tundra. Gilt für jedes Wetter.", content));
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
            "Global 1:1: Jedes Pokémon wird überall durch dasselbe andere ersetzt (z. B. jedes Raffel wird zu Evoli).\n"
            "Jeder Platz: Jeder Eintrag wird einzeln ausgewürfelt – die größte Vielfalt.", card));
        row->addStretch();
        wildMode = new SegmentedControl({"Pro Gebiet", "Global 1:1", "Jeder Platz"}, card);
        row->addWidget(wildMode);
        card->body()->addLayout(row);
        connect(wildMode, &SegmentedControl::changed, this, [this](int v) { wildSettings.mode = v; });
    }
    wildLevel = new QCheckBox("Level-passende Entwicklungen", card);
    card->body()->addLayout(rowWithInfo(wildLevel,
        "Pokémon tauchen erst auf, wenn sie auf diesem Level schon entwickelt sein könnten (z. B. kein Garados vor "
        "Level 20). Pokémon ohne Entwicklung, wie Fossilien, sind immer erlaubt."));
    connect(wildLevel, &QCheckBox::toggled, this, [this](bool on) { wildSettings.levelAppropriate = on; });
    wildType = new QCheckBox("Gleicher Typ wie das Original", card);
    card->body()->addLayout(rowWithInfo(wildType,
        "Optional: Das neue Pokémon hat mindestens einen Typ mit dem alten gemeinsam – beim Angeln gibt es dann "
        "weiterhin Wasser-Pokémon."));
    connect(wildType, &QCheckBox::toggled, this, [this](bool on) { wildSettings.sameType = on; });
    wildStrength = new QCheckBox("Ähnlich starke Pokémon", card);
    card->body()->addLayout(rowWithInfo(wildStrength,
        "Optional: Das neue Pokémon hat ungefähr dieselbe Basiswerte-Summe wie das alte."));
    connect(wildStrength, &QCheckBox::toggled, this, [this](bool on) { wildSettings.similarStrength = on; });
    wildLegends = new QCheckBox("Legendäre Pokémon erlauben", card);
    card->body()->addLayout(rowWithInfo(wildLegends,
        "Legendäre, Mysteriöse und Ultrabestien dürfen auch als wilde Pokémon auftauchen."));
    connect(wildLegends, &QCheckBox::toggled, this, [this](bool on) { wildSettings.legendaries = on; });
    inner->addWidget(card);

    inner->addWidget(mutedLabel("Der Mod enthält die Tabellen für Schwert und für Schild. Versionsexklusive Pokémon "
                                "werden dabei für jede Version getrennt ausgewürfelt.", wildContent));
    layout->addStretch();

    refreshWildPage();
    return wrapPage("Wilde Pokémon", "Pokémon in hohem Gras, Gewässern, auf Bäumen und in der Naturzone.", content);
}

void SwShRandomizerWindow::refreshWildPage() {
    if (wildSwitch == nullptr) {
        return;
    }
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (QWidget* w : std::initializer_list<QWidget*>{wildSwitch, wildMode, wildLevel, wildType, wildStrength, wildLegends}) {
        blockers.push_back(std::make_unique<QSignalBlocker>(w));
    }
    wildSwitch->setChecked(wildSettings.enabled);
    wildMode->setCurrent(wildSettings.mode);
    wildLevel->setChecked(wildSettings.levelAppropriate);
    wildType->setChecked(wildSettings.sameType);
    wildStrength->setChecked(wildSettings.similarStrength);
    wildLegends->setChecked(wildSettings.legendaries);
    wildContent->setEnabled(wildSettings.enabled);
}

// ------------------------------------------- Dyna-Raids, Items, Daten

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

QWidget* SwShRandomizerWindow::buildRaidPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* row = new QHBoxLayout();
    raidSwitch = new QCheckBox("Dyna-Raids randomisieren", content);
    raidSwitch->setObjectName("masterSwitch");
    row->addWidget(raidSwitch);
    row->addWidget(new InfoButton("Die Pokémon in den Dyna-Raid-Nestern der Naturzone, auf der Insel der Rüstung und in "
                                  "der Krone-Tundra. Wie selten ein Pokémon ist und ab wie vielen Sternen es auftaucht, "
                                  "bleibt erhalten. Es kommen nur Pokémon, die dynamaximieren können.", content));
    row->addStretch();
    layout->addLayout(row);
    connect(raidSwitch, &QCheckBox::toggled, this, [this](bool on) {
        raidSettings.enabled = on;
        raidContent->setEnabled(on);
        updateStatus();
    });

    raidContent = new QWidget(content);
    auto* inner = new QVBoxLayout(raidContent);
    inner->setContentsMargins(0, 0, 0, 0);
    auto* card = new Card("Einstellungen", QString(), raidContent);
    raidGmax = addOption(card->body(), card, "Gigadynamax-Raids behalten",
        "Raids mit Gigadynamax-Pokémon bekommen wieder ein Pokémon mit Gigadynamax-Form.", &raidSettings.keepGigantamax, this);
    raidLevel = addOption(card->body(), card, "Level-passende Entwicklungen",
        "In 1- und 2-Sterne-Raids tauchen keine Endstufen auf, die auf diesem Level noch nicht entwickelt wären. "
        "Grundlage: 1★ ≈ Lv. 15, 2★ ≈ 25, 3★ ≈ 35, 4★ ≈ 45, 5★ ≈ 55.", &raidSettings.levelAppropriate, this);
    raidType = addOption(card->body(), card, "Gleicher Typ wie das Original",
        "Optional: Das neue Pokémon teilt einen Typ mit dem alten. Nester mit einem Typ-Thema behalten so ihr Thema.",
        &raidSettings.sameType, this);
    raidStrength = addOption(card->body(), card, "Ähnlich starke Pokémon",
        "Optional: ungefähr dieselbe Basiswerte-Summe wie das alte Pokémon.", &raidSettings.similarStrength, this);
    raidLegends = addOption(card->body(), card, "Legendäre Pokémon erlauben",
        "Legendäre, Mysteriöse und Ultrabestien dürfen in normalen Raids auftauchen.", &raidSettings.legendaries, this);
    inner->addWidget(card);
    layout->addWidget(raidContent);
    layout->addStretch();
    return wrapPage("Dyna-Raids", "Raid-Nester in der Naturzone und den Gebieten des Erweiterungspasses.", content);
}

QWidget* SwShRandomizerWindow::buildItemPage() {
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
            "Komplett zufällig: Jeder Ort bekommt ein zufälliges Item, das im Spiel vorkommt.", world));
        row->addStretch();
        itemMode = new SegmentedControl({"Mischen", "Komplett zufällig"}, world);
        row->addWidget(itemMode);
        world->body()->addLayout(row);
        connect(itemMode, &SegmentedControl::changed, this, [this](int v) { itemSettings.mode = v; });
    }
    auto status = [this]() { updateStatus(); };
    itemField = addOption(world->body(), world, "Items auf dem Boden",
        "Die Pokéball-Symbole auf Routen, in Städten und in der Naturzone. TMs (gelbe Bälle) bleiben, wo sie sind.",
        &itemSettings.fieldItems, this, status);
    itemHidden = addOption(world->body(), world, "Versteckte Items",
        "Die glitzernden Stellen auf dem Boden, z. B. mit Wunschbrocken, Federn oder Sternenstaub.",
        &itemSettings.hiddenItems, this, status);
    layout->addWidget(world);

    auto* other = new Card("Shops und Trainer", QString(), content);
    itemShops = addOption(other->body(), other, "Shop-Angebote zufällig",
        "Die Angebote in den Shops werden zufällig. Pokébälle, Tränke, Beleber, Heiler und Schutz bleiben immer "
        "erhältlich, TMs ebenfalls.", &itemSettings.shops, this, status);
    itemTrainers = addOption(other->body(), other, "Items der Trainer-Pokémon zufällig",
        "Trainer-Pokémon, die im Original ein Item tragen, bekommen ein zufälliges Kampf-Item "
        "(z. B. Überreste, Leben-Orb, Wahlschal, Fokusgurt oder typverstärkende Items).",
        &itemSettings.trainerItems, this, status);
    layout->addWidget(other);
    layout->addStretch();
    return wrapPage("Items", "Gegenstände in der Spielwelt, in Shops und bei Trainern.", content);
}

QWidget* SwShRandomizerWindow::buildDataPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto status = [this]() { updateStatus(); };
    auto* evo = new Card("Entwicklungen", QString(), content);
    dataTradeEvos = addOption(evo->body(), evo, "Tausch-Entwicklungen ohne Tausch",
        "Pokémon, die sich nur durch Tausch entwickeln, entwickeln sich jetzt allein:\n"
        "• Kadabra, Maschock, Alpollo, Sedimantur, Strepoli, Paragoni, Irrbis, Laukaps und Schnuthelm ab Level 37\n"
        "• Tausch mit Item (z. B. Onix mit Metallmantel): Level-Aufstieg, während es das Item trägt",
        &dataSettings.tradeEvolutions, this, status);
    layout->addWidget(evo);

    auto* values = new Card("Werte und Fähigkeiten", QString(), content);
    dataAbilities = addOption(values->body(), values, "Fähigkeiten zufällig",
        "Jedes Pokémon bekommt zufällige Fähigkeiten (normal und versteckt). Spezial-Fähigkeiten wie Wunderwache, "
        "Kostüm oder Trance-Modus werden weder vergeben noch weggenommen.", &dataSettings.abilities, this, status);
    dataTypes = addOption(values->body(), values, "Typen zufällig",
        "Die Typen werden neu verteilt. Einfache Pokémon bleiben einfach, doppelte bleiben doppelt. Ein Feuer-Pokémon "
        "wird also z. B. komplett zum Psycho-Pokémon.", &dataSettings.types, this, status);
    dataStats = addOption(values->body(), values, "Basiswerte mischen",
        "Die sechs Basiswerte werden untereinander vertauscht – die Summe bleibt gleich. Ein schneller Angreifer kann so "
        "zu einem langsamen Verteidiger werden. Ninjatom behält seinen 1 KP.", &dataSettings.stats, this, status);
    dataFamilies = addOption(values->body(), values, "Entwicklungsreihen einheitlich",
        "Empfohlen: Alle Pokémon einer Entwicklungsreihe bekommen dieselben Fähigkeiten, dieselbe Typ-Zuordnung und "
        "dieselbe Werte-Verteilung (Glumanda, Glutexo und Glurak passen also zusammen).", &dataSettings.keepFamilies, this);
    layout->addWidget(values);

    auto* moves = new Card("Attacken", QString(), content);
    dataMoves = addOption(moves->body(), moves, "Level-Attacken zufällig",
        "Jedes Pokémon lernt per Level zufällige Attacken – etwa zwei Drittel Schadensattacken, davon die Hälfte vom "
        "eigenen Typ. Schwache Attacken kommen früh, starke spät. Die erste Attacke ist immer eine Schadensattacke.",
        &dataSettings.levelMoves, this, status);
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("TM- und TP-Kompatibilität", moves));
        row->addWidget(new InfoButton("Original: wie im Spiel.\n"
                                      "Zufällig: jedes Pokémon kann genauso viele TMs/TPs lernen wie vorher, aber andere.\n"
                                      "Alle: jedes Pokémon kann jede TM und jede TP lernen.", moves));
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
    layout->addWidget(mutedLabel("Trainer, wilde Pokémon und Raids berücksichtigen die neuen Daten automatisch "
                                 "(z. B. Typ-Arenen und starke Movesets).", content));
    layout->addStretch();
    return wrapPage("Pokémon-Daten", "Entwicklungen, Typen, Werte, Fähigkeiten und Attacken der Pokémon.", content);
}

QWidget* SwShRandomizerWindow::buildFacilityPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto status = [this]() { updateStatus(); };

    auto* lairCard = new Card("Dynamax-Abenteuer", "Krone-Tundra", content);
    lairBox = addOption(lairCard->body(), lairCard, "Leih- und Gegner-Pokémon zufällig",
        "Die Pokémon, die du in der Dyna-Höhle ausleihst und gegen die du kämpfst. Es kommen nur Pokémon, die "
        "dynamaximieren können. Sie bekommen starke Movesets für ihr Level.", &facilitySettings.maxLair, this, status);
    lairLegendsBox = addOption(lairCard->body(), lairCard, "Legendäre am Ende der Höhle zufällig",
        "Die Legendären am Ende einer Tour werden durch andere Legendäre ersetzt (jedes nur einmal). Der Hinweis-Text "
        "vor der Tour nennt eventuell noch das ursprüngliche Pokémon.", &facilitySettings.maxLairLegends, this, status);
    layout->addWidget(lairCard);

    auto* towerCard = new Card("Kampfturm", QString(), content);
    towerBox = addOption(towerCard->body(), towerCard, "Kampfturm-Teams zufällig",
        "Alle Pokémon, aus denen die Gegner im Kampfturm ihre Teams zusammenstellen, werden zufällig. Sie bekommen "
        "starke Movesets für Level 50 und Kampf-Items.", &facilitySettings.tower, this, status);
    towerEvolvedBox = addOption(towerCard->body(), towerCard, "Nur voll entwickelte Pokémon",
        "Im Kampfturm treten nur Endstufen oder Pokémon ohne Entwicklung an – wie im Original.",
        &facilitySettings.towerFullyEvolved, this);
    towerLegendsBox = addOption(towerCard->body(), towerCard, "Legendäre Pokémon erlauben",
        "Gegner im Kampfturm dürfen auch Legendäre einsetzen.", &facilitySettings.towerLegendaries, this);
    layout->addWidget(towerCard);
    layout->addStretch();
    return wrapPage("Dyna-Höhle & Kampfturm", "Dynamax-Abenteuer in der Krone-Tundra und Kampfturm in Score City.", content);
}

void SwShRandomizerWindow::refreshExtraPages() {
    if (raidSwitch == nullptr || itemMode == nullptr || dataTradeEvos == nullptr || towerBox == nullptr) {
        return;
    }
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (QWidget* w : std::initializer_list<QWidget*>{raidSwitch, raidLevel, raidGmax, raidType, raidStrength, raidLegends,
                                                     itemMode, itemField, itemHidden, itemShops, itemTrainers, dataTradeEvos,
                                                     dataAbilities, dataTypes, dataStats, dataMoves, dataFamilies, dataTMs,
                                                     lairBox, lairLegendsBox, towerBox, towerEvolvedBox, towerLegendsBox}) {
        blockers.push_back(std::make_unique<QSignalBlocker>(w));
    }
    raidSwitch->setChecked(raidSettings.enabled);
    raidLevel->setChecked(raidSettings.levelAppropriate);
    raidGmax->setChecked(raidSettings.keepGigantamax);
    raidType->setChecked(raidSettings.sameType);
    raidStrength->setChecked(raidSettings.similarStrength);
    raidLegends->setChecked(raidSettings.legendaries);
    raidContent->setEnabled(raidSettings.enabled);
    itemMode->setCurrent(itemSettings.mode);
    itemField->setChecked(itemSettings.fieldItems);
    itemHidden->setChecked(itemSettings.hiddenItems);
    itemShops->setChecked(itemSettings.shops);
    itemTrainers->setChecked(itemSettings.trainerItems);
    dataTradeEvos->setChecked(dataSettings.tradeEvolutions);
    dataAbilities->setChecked(dataSettings.abilities);
    dataTypes->setChecked(dataSettings.types);
    dataStats->setChecked(dataSettings.stats);
    dataMoves->setChecked(dataSettings.levelMoves);
    dataFamilies->setChecked(dataSettings.keepFamilies);
    dataTMs->setCurrent(dataSettings.tmMode);
    lairBox->setChecked(facilitySettings.maxLair);
    lairLegendsBox->setChecked(facilitySettings.maxLairLegends);
    towerBox->setChecked(facilitySettings.tower);
    towerEvolvedBox->setChecked(facilitySettings.towerFullyEvolved);
    towerLegendsBox->setChecked(facilitySettings.towerLegendaries);
}

// ---------------------------------------------------- Starter & Geschenke

QWidget* SwShRandomizerWindow::buildStartersPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // --- Starter ---
    auto* starterCard = new Card("Starter", QString(), content);
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Starter-Pokémon", starterCard));
        row->addWidget(new InfoButton("Original: Chimpep, Hopplo und Memmeon.\n"
                                      "Zufällig: drei zufällige Pokémon.\n"
                                      "Wunsch: du legst die drei Starter selbst fest.\n"
                                      "Auf dem Tisch bei Delion stehen danach die neuen Starter.", starterCard));
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
            "Optional: Die neuen Starter haben ungefähr dieselbe Basiswerte-Summe wie Chimpep, Hopplo und Memmeon. "
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
        const QStringList labels = {"Statt Chimpep", "Statt Hopplo", "Statt Memmeon"};
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
                encounterSettings.wished[i] = {code / 100, code % 100};
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
        "Typ:Null, Toxel, die Fossil-Pokémon, Porygon, Cosmog, Venicro, die Alola-Pokémon aus der Krone-Tundra und mehr. "
        "Pokémon, die gigadynamaximieren können (z. B. Delions Glumanda), werden durch ein anderes Pokémon mit "
        "Gigadynamax-Form ersetzt. Dakuma sowie Polaross und Phantoross bleiben, weil die Geschichte sie braucht."));
    connect(giftsBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.gifts = on; updateStatus(); });
    layout->addWidget(giftCard);

    // --- Begegnungen ---
    auto* staticCard = new Card("Statische Begegnungen", QString(), content);
    staticsBox = new QCheckBox("Legendäre und Story-Begegnungen", staticCard);
    staticCard->body()->addLayout(rowWithInfo(staticsBox,
        "Regis, Galar-Vögel, Legendäre in Dynamax-Abenteuern, Dynamax-Kämpfe in der Geschichte und mehr. Legendäre "
        "werden wieder zu Legendären. Zacian, Zamazenta, Endynalos, Coronospa mit seinen Rössern und Dakuma bleiben, "
        "damit die Geschichte funktioniert."));
    connect(staticsBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.statics = on; updateStatus(); });
    overworldBox = new QCheckBox("Feste Pokémon in der Spielwelt", staticCard);
    staticCard->body()->addLayout(rowWithInfo(overworldBox,
        "Die starken Pokémon, die an festen Stellen in der Naturzone, auf der Insel der Rüstung und in der Krone-Tundra "
        "stehen (rund 600 Stück)."));
    connect(overworldBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.overworld = on; updateStatus(); });
    layout->addWidget(staticCard);

    // --- Tausch ---
    auto* tradeCard = new Card("Tausch", QString(), content);
    tradesBox = new QCheckBox("Tauschpartner geben zufällige Pokémon", tradeCard);
    tradeCard->body()->addLayout(rowWithInfo(tradesBox,
        "Was du bei Tauschgeschäften bekommst, wird zufällig. Was du abgeben musst, bleibt gleich. Im Dialog nennt der "
        "Tauschpartner weiterhin sein ursprüngliches Pokémon."));
    connect(tradesBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.trades = on; updateStatus(); });
    layout->addWidget(tradeCard);

    // --- Gemeinsame Regeln ---
    auto* rulesCard = new Card("Regeln für Geschenke, Begegnungen und Tausch", QString(), content);
    strengthBox = new QCheckBox("Ähnlich starke Pokémon", rulesCard);
    rulesCard->body()->addLayout(rowWithInfo(strengthBox,
        "Optional, standardmäßig aus: Das neue Pokémon hat ungefähr dieselbe Basiswerte-Summe wie das alte. "
        "So wird aus einem frühen Geschenk kein Drachenpokémon mit 600 Basiswerten. Gilt nicht für die Starter, "
        "die haben eine eigene Option."));
    connect(strengthBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.similarStrength = on; });
    legendBox = new QCheckBox("Legendäre Pokémon erlauben", rulesCard);
    rulesCard->body()->addLayout(rowWithInfo(legendBox,
        "Legendäre, Mysteriöse und Ultrabestien dürfen auch dort auftauchen, wo vorher keine waren – auch als "
        "zufällige Starter."));
    connect(legendBox, &QCheckBox::toggled, this, [this](bool on) { encounterSettings.legendaries = on; });
    layout->addWidget(rulesCard);
    layout->addStretch();

    refreshStartersPage();
    return wrapPage("Starter & Geschenke", "Starter, geschenkte Pokémon, statische Begegnungen und Tausch.", content);
}

void SwShRandomizerWindow::fillStarterCombos() {
    if (!files || wishCombos[0] == nullptr) {
        return;
    }
    swsh::GameTexts texts;
    texts.load(QDir::fromNativeSeparators(romfsEdit->text().trimmed()));
    const QList<swsh::StarterChoice> all = swsh::availablePokemon(*files);
    for (int i = 0; i < 3; i++) {
        QSignalBlocker block(wishCombos[i]);
        wishCombos[i]->clear();
        wishCombos[i]->addItem("– Original –", 0);
        for (const swsh::StarterChoice& c : all) {
            wishCombos[i]->addItem(QString("%1  ·  #%2").arg(texts.pokemonName(c.species, c.form)).arg(c.species, 3, 10, QChar('0')),
                                   c.species * 100 + c.form);
        }
        auto* completer = new QCompleter(wishCombos[i]->model(), wishCombos[i]);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        wishCombos[i]->setCompleter(completer);
    }
    refreshStartersPage();
}

void SwShRandomizerWindow::refreshStartersPage() {
    if (starterMode == nullptr) {
        return;
    }
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (QWidget* w : std::initializer_list<QWidget*>{starterMode, starterTypes, starterStages, starterStrength, giftsBox, staticsBox,
                                                     overworldBox, tradesBox, strengthBox, legendBox,
                                                     wishCombos[0], wishCombos[1], wishCombos[2]}) {
        blockers.push_back(std::make_unique<QSignalBlocker>(w));
    }
    starterMode->setCurrent(encounterSettings.starterMode);
    starterTypes->setCurrent(encounterSettings.starterTypes);
    starterStages->setChecked(encounterSettings.starterThreeStages);
    starterStrength->setChecked(encounterSettings.starterSimilarStrength);
    giftsBox->setChecked(encounterSettings.gifts);
    staticsBox->setChecked(encounterSettings.statics);
    overworldBox->setChecked(encounterSettings.overworld);
    tradesBox->setChecked(encounterSettings.trades);
    strengthBox->setChecked(encounterSettings.similarStrength);
    legendBox->setChecked(encounterSettings.legendaries);
    for (int i = 0; i < 3; i++) {
        const int code = encounterSettings.wished[i].species * 100 + encounterSettings.wished[i].form;
        const int index = wishCombos[i]->findData(code);
        wishCombos[i]->setCurrentIndex(index >= 0 ? index : 0);
        wishCombos[i]->setEnabled(files != nullptr);
    }
    starterRandomBox->setVisible(encounterSettings.starterMode == 1);
    starterWishBox->setVisible(encounterSettings.starterMode == 2);
}

void SwShRandomizerWindow::showPage(int index) {
    nav->setCurrentRow(index);
}

// ---------------------------------------------------------------- Trainer

QWidget* SwShRandomizerWindow::buildTrainerPage() {
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
        row->addWidget(new InfoButton("Schaltet die Randomisierung aller Trainer ein oder aus. Gilt für Schwert und "
                                      "Schild, mit und ohne Erweiterungspass.", content));
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
    baseEditor = new SwShTrainerEditor(&trainerSettings.base, baseCard);
    baseCard->body()->addWidget(baseEditor);
    inner->addWidget(baseCard);

    auto* typeCard = new Card("Typ-Trainer", QString(), trainerContent);
    typeThemeBox = new QCheckBox("Typ-Trainer behalten ihren Typ", typeCard);
    typeCard->body()->addLayout(rowWithInfo(typeThemeBox,
        "Trainer mit festem Typ bekommen nur zufällige Pokémon dieses Typs. Gilt automatisch für alle Gruppen."));
    typeCard->body()->addWidget(mutedLabel(
        "Betrifft: alle Arenen mit Arenatrainern und Revanchen (Yarro, Kate, Kabu, Saida/Nio, Papella, Mac/Mel, "
        "Nezz, Roy, Betys, Mary), die Arena-Challenger im Champ-Cup sowie Sophora und Saverio.", typeCard));
    connect(typeThemeBox, &QCheckBox::toggled, this, [this](bool on) { trainerSettings.keepTypeTheme = on; });
    inner->addWidget(typeCard);

    auto* groupsCard = new Card("Ausnahmen für einzelne Gruppen", "optional", trainerContent);
    groupsCard->body()->addWidget(mutedLabel("Normalerweise gelten die Grundeinstellungen für alle. Hier kannst du "
                                             "einzelnen Gruppen eigene Einstellungen geben oder sie unverändert lassen.",
                                             groupsCard));
    auto* collapsible = new Collapsible("Gruppen anzeigen", false, groupsCard);
    groupsCard->body()->addWidget(collapsible);

    for (int g = 0; g < swsh::GroupCount; g++) {
        if (g == swsh::GroupIsle) {
            auto* label = new QLabel("Erweiterungspass", groupsCard);
            label->setObjectName("sectionLabel");
            collapsible->body()->addWidget(label);
        }
        const swsh::GroupInfo info = swsh::groupInfo(g);
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
        groupEditors[g] = new SwShTrainerEditor(&trainerSettings.own[g], groupEditorBoxes[g]);
        boxLayout->addWidget(groupEditors[g]);
        groupEditorBoxes[g]->setVisible(false);
        rowLayout->addWidget(groupEditorBoxes[g]);
        collapsible->body()->addWidget(row);

        connect(groupSelectors[g], &SegmentedControl::changed, this, [this, g](int mode) {
            trainerSettings.modes[g] = mode;
            if (mode == swsh::OwnSettings && !ownInitialized[g]) {
                trainerSettings.own[g] = trainerSettings.base; // Startpunkt: die Grundeinstellungen
                ownInitialized[g] = true;
                groupEditors[g]->refresh();
            }
            groupEditorBoxes[g]->setVisible(mode == swsh::OwnSettings);
        });
    }
    inner->addWidget(groupsCard);
    layout->addStretch();

    refreshTrainerPage();
    return wrapPage("Trainer", "Lege fest, wie die Teams aller Trainer zufällig zusammengestellt werden.", content);
}

void SwShRandomizerWindow::refreshTrainerPage() {
    {
        QSignalBlocker b1(trainerSwitch), b2(typeThemeBox);
        trainerSwitch->setChecked(trainerSettings.enabled);
        typeThemeBox->setChecked(trainerSettings.keepTypeTheme);
    }
    trainerContent->setEnabled(trainerSettings.enabled);
    baseEditor->refresh();
    for (int g = 0; g < swsh::GroupCount; g++) {
        QSignalBlocker block(groupSelectors[g]);
        groupSelectors[g]->setCurrent(trainerSettings.modes[g]);
        groupEditors[g]->refresh();
        groupEditorBoxes[g]->setVisible(trainerSettings.modes[g] == swsh::OwnSettings);
    }
}

// ---------------------------------------------------------- Randomisieren

namespace {
quint64 seedFromText(const QString& text) {
    bool ok = false;
    quint64 value = text.toULongLong(&ok);
    if (ok) {
        return value;
    }
    QByteArray hash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
    quint64 result = 0;
    for (int i = 0; i < 8; i++) {
        result = (result << 8) | static_cast<quint8>(hash[i]);
    }
    return result;
}
} // namespace

bool SwShRandomizerWindow::randomizeTo(const QString& outputFolder, QString* error) {
    if (!files) {
        if (error) *error = "Die Spieldateien sind nicht geladen.";
        return false;
    }
    QString seedText = seedEdit->text().trimmed();
    if (seedText.isEmpty()) {
        seedText = QString::number(QRandomGenerator::global()->bounded(1000000000));
    }
    const quint64 seed = seedFromText(seedText);

    const QString romfs = QDir::fromNativeSeparators(romfsEdit->text().trimmed());
    swsh::GameTexts texts;
    texts.load(romfs);
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };

    swsh::GameFiles work = *files; // Original bleibt fuer weitere Durchlaeufe unveraendert

    // Pokemon-Daten zuerst, damit Trainer und wilde Pokemon die neuen Entwicklungen kennen
    QList<swsh::EvolutionChange> evolutionChanges;
    if (dataSettings.tradeEvolutions) {
        evolutionChanges = swsh::removeTradeEvolutions(work, texts);
    }
    const swsh::PokemonDataResult dataResult = swsh::randomizePokemonData(work, dataSettings, seed);

    swsh::TrainerRandomizerResult result = swsh::randomizeTrainers(work, trainerSettings, seed);

    QString stepError;
    swsh::EncounterResult encounters;
    if (!swsh::randomizeEncounters(romfs, work, encounterSettings, seed, encounters, &stepError)) return fail(stepError);

    swsh::WildResult wild;
    if (!swsh::randomizeWild(romfs, work, wildSettings, seed, wild, &stepError)) return fail(stepError);

    // data_table.gfpak: wilde Pokemon und Raids teilen sich die Datei
    QByteArray dataTable = wild.dataTable;
    swsh::RaidResult raids;
    if (raidSettings.enabled) {
        if (dataTable.isEmpty()) dataTable = swsh::readFile(romfs + "/bin/archive/field/resident/data_table.gfpak");
        if (!swsh::randomizeRaids(dataTable, work, raidSettings, seed, raids, &stepError)) return fail(stepError);
    }

    swsh::FacilityResult facilities;
    if (!swsh::randomizeFacilities(romfs, work, facilitySettings, seed, facilities, &stepError)) return fail(stepError);

    // placement.gfpak: Starter-Modelle und Items teilen sich die Datei
    QByteArray placement = encounters.placement;
    swsh::ItemResult items;
    QList<int> changedTrainers = result.changedIndexes;
    if (itemSettings.anyEnabled()) {
        const QByteArray input = placement.isEmpty() ? swsh::readFile(romfs + "/" + swsh::path::Placement) : placement;
        if (!swsh::randomizeItems(romfs, input, work, &changedTrainers, itemSettings, seed, items, &stepError)) {
            return fail(stepError);
        }
        if (!items.placement.isEmpty()) placement = items.placement;
    }
    encounters.placement = placement;

    QDir(outputFolder).removeRecursively();
    const QString romfsOut = outputFolder + "/romfs";
    bool written = work.trainers.save(romfsOut, changedTrainers) && swsh::writeEncounters(romfsOut, encounters) &&
                   work.evolutions.save(romfsOut);
    if (!dataTable.isEmpty()) {
        written &= swsh::writeFile(romfsOut + "/bin/archive/field/resident/data_table.gfpak", dataTable);
    }
    written &= swsh::writeFacilities(romfsOut, facilities);
    if (dataResult.personalChanged) written &= swsh::writeFile(romfsOut + "/" + swsh::path::Personal, work.personal.save());
    if (dataResult.learnsetsChanged) written &= swsh::writeFile(romfsOut + "/" + swsh::path::Learnsets, work.learnsets.save());
    if (!items.shops.isEmpty()) {
        written &= swsh::writeFile(romfsOut + "/bin/appli/shop/bin/shop_data.bin", items.shops);
    }
    if (!written) return fail("Die Dateien konnten nicht geschrieben werden:\n" + romfsOut);

    if (spoilerLog->isChecked()) {
        QList<swsh::SpoilerSection> sections = swsh::encounterSpoiler(encounters, work, texts);
        sections += swsh::wildSpoiler(wild, work, texts, swsh::readMessage(romfs, "place_name_indirect"), check.version);
        sections += swsh::extrasSpoiler(evolutionChanges, raids, items, texts, check.version);
        sections += swsh::dataSpoiler(dataResult, facilities, work, texts);
        swsh::writeSpoiler(outputFolder + "/Spoiler-Log.html", work, texts, trainerSettings, sections, seedText,
                           check.version);
    }

    QFile info(outputFolder + "/Info.txt");
    if (info.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&info);
        out << "Pokémon Schwert/Schild Randomizer\n";
        out << "Erstellt: " << QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm") << "\n";
        out << "Seed: " << seedText << "\n";
        out << "Spiel: " << swsh::versionName(check.version) << "\n";
        out << "Geänderte Trainer: " << result.randomized << "\n";
        out << "Geänderte Starter/Geschenke/Begegnungen/Tausch: " << encounters.changes.size() << "\n\n";
        out << "Trainer-Einstellungen:\n" << QJsonDocument(swsh::settingsToJson(trainerSettings)).toJson() << "\n";
        out << "Starter & Geschenke:\n" << QJsonDocument(swsh::encounterSettingsToJson(encounterSettings)).toJson() << "\n";
        out << "Wilde Pokémon:\n" << QJsonDocument(swsh::wildSettingsToJson(wildSettings)).toJson() << "\n";
        out << "Dyna-Raids:\n" << QJsonDocument(swsh::raidSettingsToJson(raidSettings)).toJson() << "\n";
        out << "Items:\n" << QJsonDocument(swsh::itemSettingsToJson(itemSettings)).toJson() << "\n";
        out << "Pokémon-Daten:\n" << QJsonDocument(swsh::pokemonDataSettingsToJson(dataSettings)).toJson() << "\n";
        out << "Dyna-Höhle & Kampfturm:\n" << QJsonDocument(swsh::facilitySettingsToJson(facilitySettings)).toJson();
    }
    lastSeed = seedText;
    return true;
}

void SwShRandomizerWindow::startRandomizer() {
    saveSettings();
    const QString output = QDir::current().filePath("Randomizers-Output/Schwert-Schild/Randomizer-1");
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = randomizeTo(output, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, "Randomisieren", error);
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle("Fertig");
    box.setIcon(QMessageBox::Information);
    QString tid = swsh::titleId(check.version);
    box.setText("Der Randomizer ist fertig (Seed " + lastSeed + ").");
    box.setInformativeText(
        "Kopiere den Ordner „romfs“ aus Randomizer-1 in einen Mod-Ordner deines Spiels:\n\n"
        "• Ryujinx: Rechtsklick auf das Spiel → „Mod-Verzeichnis öffnen“, dort einen Ordner (z. B. Randomizer) anlegen "
        "und „romfs“ hineinkopieren.\n"
        "• Switch (Atmosphère): atmosphere/contents/" + (tid.isEmpty() ? QString("<Title-ID>") : tid) + "/romfs");
    QPushButton* openFolder = box.addButton("Ordner öffnen", QMessageBox::ActionRole);
    QPushButton* openSpoiler = spoilerLog->isChecked() ? box.addButton("Spoiler-Log öffnen", QMessageBox::ActionRole) : nullptr;
    box.addButton(QMessageBox::Close);
    box.exec();
    if (box.clickedButton() == openFolder) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(output));
    } else if (openSpoiler != nullptr && box.clickedButton() == openSpoiler) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(output + "/Spoiler-Log.html"));
    }
}

// ------------------------------------------------------------ Speichern

void SwShRandomizerWindow::saveSettings() const {
    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.setValue("RomFS", romfsEdit->text().trimmed());
    settings.setValue("ExeFS", exefsEdit->text().trimmed());
    settings.setValue("Seed", seedEdit->text());
    settings.setValue("SpoilerLog", spoilerLog->isChecked());
    settings.setValue("Trainer", QString::fromUtf8(QJsonDocument(swsh::settingsToJson(trainerSettings)).toJson(QJsonDocument::Compact)));
    settings.setValue("Facilities", QString::fromUtf8(QJsonDocument(swsh::facilitySettingsToJson(facilitySettings)).toJson(QJsonDocument::Compact)));
    settings.setValue("Raids", QString::fromUtf8(QJsonDocument(swsh::raidSettingsToJson(raidSettings)).toJson(QJsonDocument::Compact)));
    settings.setValue("Items", QString::fromUtf8(QJsonDocument(swsh::itemSettingsToJson(itemSettings)).toJson(QJsonDocument::Compact)));
    settings.setValue("PokemonData", QString::fromUtf8(QJsonDocument(swsh::pokemonDataSettingsToJson(dataSettings)).toJson(QJsonDocument::Compact)));
    settings.setValue("Wild", QString::fromUtf8(QJsonDocument(swsh::wildSettingsToJson(wildSettings)).toJson(QJsonDocument::Compact)));
    settings.setValue("Encounters", QString::fromUtf8(QJsonDocument(swsh::encounterSettingsToJson(encounterSettings)).toJson(QJsonDocument::Compact)));
}

void SwShRandomizerWindow::loadSettings() {
    QSettings settings(kSettingsOrg, kSettingsApp);
    seedEdit->setText(settings.value("Seed").toString());
    spoilerLog->setChecked(settings.value("SpoilerLog", true).toBool());
    const QJsonDocument trainerJson = QJsonDocument::fromJson(settings.value("Trainer").toString().toUtf8());
    if (trainerJson.isObject()) {
        swsh::settingsFromJson(trainerJson.object(), trainerSettings);
        for (int g = 0; g < swsh::GroupCount; g++) {
            ownInitialized[g] = trainerSettings.modes[g] == swsh::OwnSettings;
        }
        refreshTrainerPage();
    }
    const QJsonDocument encounterJson = QJsonDocument::fromJson(settings.value("Encounters").toString().toUtf8());
    if (encounterJson.isObject()) {
        swsh::encounterSettingsFromJson(encounterJson.object(), encounterSettings);
        refreshStartersPage();
    }
    const QJsonDocument wildJson = QJsonDocument::fromJson(settings.value("Wild").toString().toUtf8());
    if (wildJson.isObject()) {
        swsh::wildSettingsFromJson(wildJson.object(), wildSettings);
        refreshWildPage();
    }
    const QJsonDocument raidJson = QJsonDocument::fromJson(settings.value("Raids").toString().toUtf8());
    if (raidJson.isObject()) swsh::raidSettingsFromJson(raidJson.object(), raidSettings);
    const QJsonDocument itemJson = QJsonDocument::fromJson(settings.value("Items").toString().toUtf8());
    if (itemJson.isObject()) swsh::itemSettingsFromJson(itemJson.object(), itemSettings);
    const QJsonDocument dataJson = QJsonDocument::fromJson(settings.value("PokemonData").toString().toUtf8());
    if (dataJson.isObject()) swsh::pokemonDataSettingsFromJson(dataJson.object(), dataSettings);
    const QJsonDocument facilityJson = QJsonDocument::fromJson(settings.value("Facilities").toString().toUtf8());
    if (facilityJson.isObject()) swsh::facilitySettingsFromJson(facilityJson.object(), facilitySettings);
    refreshExtraPages();
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
