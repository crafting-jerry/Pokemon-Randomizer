#include "headers/swsh/SwShRandomizerWindow.h"
#include "headers/modern_ui/modern_widgets.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QScrollArea>
#include <QSettings>
#include <QStyle>
#include <QUrl>

using namespace modernui;

namespace {
const QStringList kPages = {"Start", "Trainer", "Starter & Geschenke", "Wilde Pokémon"};

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
    pages->addWidget(buildComingSoonPage("Trainer", "Teams aller Trainer, Arenaleiter und Champs.",
        "Kommt im nächsten Schritt: Typ-Arenen behalten ihren Typ (Yarro, Kate, Kabu, Saida/Nio, Papella, "
        "Mac/Mel, Nezz und Roy), voll entwickelte Pokémon ab einstellbarem Level, sinnvolle Attacken, "
        "Dynamax/Gigadynamax-Regeln und ein Spoiler-Log."));
    pages->addWidget(buildComingSoonPage("Starter & Geschenke", "Starter, geschenkte und statische Pokémon.",
        "Kommt später: Chimpep, Hopplo und Memmeon sowie Geschenk- und Legendären-Begegnungen."));
    pages->addWidget(buildComingSoonPage("Wilde Pokémon", "Pokémon in hohem Gras, Gewässern und der Naturzone.",
        "Kommt später: Begegnungen auf Routen, in der Naturzone, auf der Insel der Rüstung und in der Krone-Tundra."));
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
    startButton->setToolTip("Die Randomisierung für Schwert/Schild folgt in den nächsten Schritten.");
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

void SwShRandomizerWindow::updateStatus() {
    if (files) {
        activeInfo->setText(swsh::versionName(check.version == swsh::Version::Unknown ? swsh::Version::Sword
                                                                                       : check.version)
                            + (check.version == swsh::Version::Unknown ? " oder Schild" : QString())
                            + " bereit");
        detailInfo->setText("Spieldateien geladen. Die Randomizer-Bereiche folgen in den nächsten Schritten.");
    } else {
        activeInfo->setText("Spieldateien fehlen");
        detailInfo->setText("Gib auf der Start-Seite den RomFS- und ExeFS-Ordner deines Dumps an.");
    }
}

// ------------------------------------------------------------ Speichern

void SwShRandomizerWindow::saveSettings() const {
    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.setValue("RomFS", romfsEdit->text().trimmed());
    settings.setValue("ExeFS", exefsEdit->text().trimmed());
    settings.setValue("Seed", seedEdit->text());
    settings.setValue("SpoilerLog", spoilerLog->isChecked());
}

void SwShRandomizerWindow::loadSettings() {
    QSettings settings(kSettingsOrg, kSettingsApp);
    seedEdit->setText(settings.value("Seed").toString());
    spoilerLog->setChecked(settings.value("SpoilerLog", true).toBool());
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
