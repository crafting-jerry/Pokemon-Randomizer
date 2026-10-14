#include "headers/modern_ui/ModernRandomizerWindow.h"

#include <QScrollArea>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSettings>
#include <QTextStream>

#include "headers/common/mod_output.h"

using namespace modernui;

namespace {
const char* kSettingsOrg = "Pokemon Randomizer";
const char* kSettingsApp = "KarmesinPurpur";
// Title-IDs fuer Mod-Ordner (Karmesin und Purpur nutzen dieselben Dateien)
const QString kScarletId = "0100A3D008C5C000";
const QString kVioletId = "01008F6008C5E000";

const QStringList kPages = {
    "Start", "Starter & Geschenke", "Wilde Pokémon", "Statische Begegnungen",
    "Pokémon-Daten", "Items", "Trainer", "Raids & Bosse"
};
} // namespace

ModernRandomizerWindow::ModernRandomizerWindow(SVRandomizerWindow* classicWindow, QWidget* parent)
    : QWidget(parent), classic(classicWindow), code(classicWindow->code()) {
    setObjectName("modernRoot");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(modernui::styleSheet());

    setupGroups();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* middle = new QHBoxLayout();
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(0);
    middle->addWidget(buildSidebar());

    pages = new QStackedWidget(this);
    pages->addWidget(buildStartPage());
    pages->addWidget(buildStartersPage());
    pages->addWidget(buildWildsPage());
    pages->addWidget(buildFixedPage());
    pages->addWidget(buildPersonalPage());
    pages->addWidget(buildItemsPage());
    pages->addWidget(buildTrainerPage());
    pages->addWidget(buildRaidsBossesPage());
    middle->addWidget(pages, 1);

    root->addLayout(middle, 1);
    root->addWidget(buildBottomBar());

    connect(nav, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    nav->setCurrentRow(0);

    loadAutosave();
    loadOutputSettings();
    updateStatus();
}

ModernRandomizerWindow::~ModernRandomizerWindow() {
    autosave();
    saveOutputSettings();
}

// ------------------------------------------------------------------ Gruppen

void ModernRandomizerWindow::setupGroups() {
    svTrainers& t = code.svRandomizerTrainers;
    auto add = [&](const QString& id, const QString& region, const QString& title, const QString& info,
                   trainerSettings* target, bool inAll = true, int mode = SameAsBase) {
        auto g = std::make_unique<GroupState>();
        g->id = id;
        g->region = region;
        g->title = title;
        g->info = info;
        g->target = target;
        g->inRegionAll = inAll;
        g->mode = mode;
        groups.push_back(std::move(g));
    };

    add("paldea_rivals", "paldea", "Rivalen und Freunde",
        "Nemila, Pepper, Cosima, Direktor Clavel und die Bosse von Team Star.", &t.rivalTrainers);
    add("paldea_gyms", "paldea", "Arenen",
        "Alle Arenatrainer und Arenaleiter, auch die Revanchen.", &t.gymTrainers);
    add("paldea_e4", "paldea", "Top Vier",
        "Cay, Poppy, Aoki und Sinius.", &t.e4Trainers);
    add("paldea_champion", "paldea", "Top-Champ",
        "Sagaria.", &t.championTrainers);
    add("paldea_routes", "paldea", "Routentrainer",
        "Alle normalen Trainer auf den Routen und in den Orten.", &t.routeTrainers);
    add("paldea_raids", "paldea", "Raid-Trainer",
        "Die Trainer, die mit dir an Tera-Raids teilnehmen.", &t.raidTrainers);
    add("paldea_paradise", "paldea", "Koraidon/Miraidon-Kampf",
        "Der Kampf im Areal Null (Paradiesschutzprotokoll). Gehört nicht zu den übrigen Trainern und bleibt "
        "standardmäßig unverändert. „Wie oben“ randomisiert ihn mit den Grundeinstellungen.",
        &t.paradisePokemon, false, KeepOriginal);

    add("kitakami_rivals", "kitakami", "Rivalen",
        "Die beiden Geschwister aus Kitakami in allen Kämpfen.", &t.kitakamiRivals);
    add("kitakami_ogre", "kitakami", "Bande (Oger-Clan)",
        "Die vier Bandenmitglieder rund um Ogerpon.", &t.ogreClanTrainers);
    add("kitakami_routes", "kitakami", "Routentrainer",
        "Alle normalen Trainer in Kitakami.", &t.kitakamiRouteTrainers);
    add("kitakami_raids", "kitakami", "Raid-Trainer",
        "Die Trainer, die in Kitakami mit dir an Tera-Raids teilnehmen.", &t.kitakamiRaidTrainers);

    add("blueberry_rivals", "blueberry", "Rivalen",
        "Die Rivalen-Kämpfe in der Blaubeer-Akademie.", &t.blueberryRivals);
    add("blueberry_bb4", "blueberry", "Blaubeer-Top-Vier",
        "Die Blaubeer-Top-Vier und ihre Clubmitglieder.", &t.bb4Trainers);
    add("blueberry_routes", "blueberry", "Routentrainer",
        "Alle normalen Trainer im Tera-Dom.", &t.blueberryRouteTrainers);
    add("blueberry_raids", "blueberry", "Raid-Trainer",
        "Die Trainer, die im Tera-Dom mit dir an Tera-Raids teilnehmen.", &t.blueberryRaidTrainers);
}

// --------------------------------------------------------------- Seitenleiste

QWidget* ModernRandomizerWindow::buildSidebar() {
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
    connect(back, &QPushButton::clicked, this, &ModernRandomizerWindow::backRequested);
    layout->addWidget(back, 0, Qt::AlignLeft);
    layout->addSpacing(6);

    auto* title = new QLabel("Purpur Randomizer", sidebar);
    title->setObjectName("appTitle");
    auto* subtitle = new QLabel("Karmesin und Purpur", sidebar);
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

QWidget* ModernRandomizerWindow::buildBottomBar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("bottomBar");
    bar->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(20, 10, 20, 10);

    auto* infoColumn = new QVBoxLayout();
    infoColumn->setSpacing(2);
    activeInfo = new QLabel("", bar);
    seedInfo = mutedLabel("", bar);
    seedInfo->setWordWrap(false);
    infoColumn->addWidget(activeInfo);
    infoColumn->addWidget(seedInfo);
    layout->addLayout(infoColumn);
    layout->addStretch();

    auto* start = new QPushButton("Randomisieren", bar);
    start->setObjectName("primary");
    start->setCursor(Qt::PointingHandCursor);
    connect(start, &QPushButton::clicked, this, &ModernRandomizerWindow::startRandomizer);
    layout->addWidget(start);
    return bar;
}

QWidget* ModernRandomizerWindow::wrapPage(const QString& title, const QString& subtitle, QWidget* content) {
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

// ------------------------------------------------------------------- Start

QWidget* ModernRandomizerWindow::buildStartPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // Allgemein
    auto* general = new Card("Allgemein", QString(), content);
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Seed", general));
        row->addWidget(new InfoButton("Mit demselben Seed und denselben Einstellungen entsteht immer genau dasselbe Ergebnis. "
                                      "Leer lassen für ein zufälliges Ergebnis.", general));
        row->addStretch();
        seedEdit = new QLineEdit(general);
        seedEdit->setPlaceholderText("leer = zufällig");
        seedEdit->setFixedWidth(260);
        row->addWidget(seedEdit);
        general->body()->addLayout(row);
        connect(seedEdit, &QLineEdit::textChanged, this, &ModernRandomizerWindow::updateSeedInfo);
    }
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Anzahl Durchläufe", general));
        row->addWidget(new InfoButton("Erstellt mehrere unterschiedliche Randomizer auf einmal. "
                                      "Jeder landet in einem eigenen Ordner (Randomizer-1, Randomizer-2, …).", general));
        row->addStretch();
        runsSpin = new QSpinBox(general);
        runsSpin->setRange(1, 50);
        runsSpin->setFixedWidth(80);
        runsSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        row->addWidget(runsSpin);
        general->body()->addLayout(row);
    }
    autoPatch = new QCheckBox("Automatisch patchen", general);
    autoPatch->setChecked(true);
    general->body()->addLayout(rowWithInfo(autoPatch,
        "Passt die Dateiliste des Spiels (data.trpfd) an, damit es die geänderten Dateien lädt. "
        "Sollte eingeschaltet bleiben, außer du patchst selbst."));
    spoilerLog = new QCheckBox("Spoiler-Log erstellen", general);
    spoilerLog->setChecked(true);
    general->body()->addLayout(rowWithInfo(spoilerLog,
        "Erstellt zusätzlich die Datei Spoiler-Log.html mit allen Trainer-Teams und Tera-Raids. Öffnet sich im Browser und ist durchsuchbar."));
    general->body()->addWidget(mutedLabel("Bleibt der Seed leer, wird ein zufälliger ausgewürfelt und nach dem Randomisieren "
                                          "angezeigt (auch in der Info.txt) – so kannst du ein Ergebnis später nachbauen.",
                                          general));
    layout->addWidget(general);

    // Vorlagen
    auto* presets = new Card("Vorlagen", QString(), content);
    presets->header()->addWidget(new InfoButton(
        "Speichert alle Einstellungen (alle Seiten, Seed, Durchläufe und Spoiler-Log) in einer Datei. Vorlagen liegen in "
        "Randomizers-Output → Karmesin-Purpur → Vorlagen. Der Mod-Ordner wird nicht mitgespeichert.", presets));
    presets->body()->addWidget(mutedLabel("Speichere deine Einstellungen als Datei, um sie später wieder zu laden oder zu teilen. "
                                          "Die zuletzt verwendeten Einstellungen werden außerdem automatisch gemerkt.", presets));
    {
        auto* row = new QHBoxLayout();
        auto* save = new QPushButton("Einstellungen speichern …", presets);
        auto* load = new QPushButton("Einstellungen laden …", presets);
        save->setObjectName("secondary");
        load->setObjectName("secondary");
        save->setCursor(Qt::PointingHandCursor);
        load->setCursor(Qt::PointingHandCursor);
        row->addWidget(save);
        row->addWidget(load);
        row->addStretch();
        presets->body()->addLayout(row);
        connect(save, &QPushButton::clicked, this, &ModernRandomizerWindow::saveSettingsDialog);
        connect(load, &QPushButton::clicked, this, &ModernRandomizerWindow::loadSettingsDialog);
    }
    layout->addWidget(presets);

    // Ausgabe
    auto* output = new Card("Ausgabe", QString(), content);
    output->body()->addWidget(mutedLabel(
        "Die Ergebnisse landen im Ordner Randomizers-Output → Karmesin-Purpur → Randomizer-1 (bei mehreren Durchläufen "
        "auch Randomizer-2, -3 …). In den Mod-Ordner deines Spiels kopierst du den Ordner „romfs“ – oder lässt ihn unten "
        "direkt dorthin schreiben.", output));
    modDirect = new QCheckBox("Direkt in den Mod-Ordner des Emulators schreiben", output);
    output->body()->addLayout(rowWithInfo(modDirect,
        "Nach dem Randomisieren wird der Ordner „romfs“ aus Randomizer-1 zusätzlich direkt in den Mod-Ordner kopiert. "
        "Ein älterer Ordner dort wird dabei ersetzt – du musst also nichts mehr von Hand kopieren.\n\n"
        "„Emulator suchen“ findet Ryujinx und yuzu (sowie Ableger wie suyu, sudachi, citron und Eden) automatisch und "
        "legt den passenden Ordner …/<Title-ID>/Randomizer an – für Karmesin oder Purpur. Für eine echte Switch bleibt es "
        "beim Kopieren nach atmosphere/contents/<Title-ID>/."));
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
            "Der Ordner deines Mods, z. B. …/mods/contents/<Title-ID>/Randomizer (Ryujinx) oder …/load/<Title-ID>/Randomizer "
            "(yuzu). Title-IDs: Karmesin " + kScarletId + ", Purpur " + kVioletId + ". Darin wird der Ordner „romfs“ "
            "angelegt bzw. ersetzt.", modRow));
        connect(browseButton, &QPushButton::clicked, this, [this]() {
            const QString start = modFolderEdit->text().trimmed().isEmpty() ? QDir::homePath() : modFolderEdit->text().trimmed();
            const QString dir = QFileDialog::getExistingDirectory(this, "Mod-Ordner wählen", start);
            if (!dir.isEmpty()) modFolderEdit->setText(QDir::toNativeSeparators(dir));
            saveOutputSettings();
        });
        connect(searchButton, &QPushButton::clicked, this, [this]() { findEmulator(); });
        connect(modFolderEdit, &QLineEdit::editingFinished, this, [this]() { saveOutputSettings(); });
    }
    output->body()->addWidget(modRow);
    modRow->setEnabled(false);
    connect(modDirect, &QCheckBox::toggled, modRow, &QWidget::setEnabled);
    connect(modDirect, &QCheckBox::toggled, this, [this]() { saveOutputSettings(); });
    {
        auto* row = new QHBoxLayout();
        auto* open = new QPushButton("Ausgabeordner öffnen", output);
        open->setObjectName("secondary");
        open->setCursor(Qt::PointingHandCursor);
        row->addWidget(open);
        row->addStretch();
        output->body()->addLayout(row);
        connect(open, &QPushButton::clicked, this, []() {
            QString path = modoutput::gameFolder(modoutput::kScarletViolet);
            QDir().mkpath(path);
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
    }
    layout->addWidget(output);

    return wrapPage("Start", "Allgemeine Einstellungen, Vorlagen und Ausgabe.", content);
}

// ----------------------------------------------------------------- Trainer

QWidget* ModernRandomizerWindow::buildTrainerPage() {
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // Hauptschalter und Regionen
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(8);
        masterSwitch = new QCheckBox("Trainer randomisieren", content);
        masterSwitch->setObjectName("masterSwitch");
        row->addWidget(masterSwitch);
        row->addWidget(new InfoButton("Schaltet die Randomisierung aller Trainer ein oder aus.", content));
        row->addStretch();
        row->addWidget(mutedLabel("Regionen:", content));
        paldeaBox = new QCheckBox("Paldea", content);
        kitakamiBox = new QCheckBox("Kitakami", content);
        blueberryBox = new QCheckBox("Blaubeer-Akademie", content);
        row->addWidget(paldeaBox);
        row->addWidget(kitakamiBox);
        row->addWidget(blueberryBox);
        row->addWidget(new InfoButton("Kitakami ist die Region aus „Die türkisgrüne Maske“ (DLC 1), "
                                      "die Blaubeer-Akademie stammt aus „Die Indigoblaue Scheibe“ (DLC 2).", content));
        layout->addLayout(row);

        connect(masterSwitch, &QCheckBox::toggled, this, [this](bool on) { trainersEnabled = on; updateTrainerStates(); updateStatus(); });
        connect(paldeaBox, &QCheckBox::toggled, this, [this](bool on) { regionPaldea = on; updateTrainerStates(); updateStatus(); });
        connect(kitakamiBox, &QCheckBox::toggled, this, [this](bool on) { regionKitakami = on; updateTrainerStates(); updateStatus(); });
        connect(blueberryBox, &QCheckBox::toggled, this, [this](bool on) { regionBlueberry = on; updateTrainerStates(); updateStatus(); });
    }

    trainerContent = new QWidget(content);
    auto* inner = new QVBoxLayout(trainerContent);
    inner->setContentsMargins(0, 0, 0, 0);
    inner->setSpacing(12);
    layout->addWidget(trainerContent);

    // Grundeinstellungen
    auto* baseCard = new Card("Grundeinstellungen", "gilt für alle Trainer", trainerContent);
    baseEditor = new TrainerSettingsEditor(&base, baseCard);
    baseCard->body()->addWidget(baseEditor);
    inner->addWidget(baseCard);

    // Typ-Trainer
    auto* typeCard = new Card("Typ-Trainer", QString(), trainerContent);
    typeThemeBox = new QCheckBox("Typ-Trainer behalten ihren Typ", typeCard);
    typeCard->body()->addLayout(rowWithInfo(typeThemeBox,
        "Trainer mit festem Typ bekommen nur zufällige Pokémon dieses Typs. Das Ass eines Arenaleiters "
        "terakristallisiert weiterhin in den Arenatyp. Gilt automatisch für alle Gruppen."));
    typeCard->body()->addWidget(mutedLabel("Betrifft: Arenen, Top Vier, Team Star und Blaubeer-Top-Vier.", typeCard));
    connect(typeThemeBox, &QCheckBox::toggled, this, [this](bool on) { typeTheme = on; });
    inner->addWidget(typeCard);

    // Ausnahmen
    auto* exceptionsCard = new Card("Ausnahmen für einzelne Gruppen", "optional", trainerContent);
    exceptionsCard->body()->addWidget(mutedLabel("Normalerweise gelten die Grundeinstellungen für alle. Hier kannst du einzelnen "
                                                 "Gruppen eigene Einstellungen geben oder sie unverändert lassen.", exceptionsCard));
    auto* collapsible = new Collapsible("Gruppen anzeigen", false, exceptionsCard);
    exceptionsCard->body()->addWidget(collapsible);

    QString lastRegion;
    const QMap<QString, QString> regionNames = {
        {"paldea", "Paldea"}, {"kitakami", "Kitakami"}, {"blueberry", "Blaubeer-Akademie"}
    };
    for (auto& gp : groups) {
        GroupState* g = gp.get();
        if (g->region != lastRegion) {
            auto* label = new QLabel(regionNames.value(g->region), exceptionsCard);
            label->setObjectName("sectionLabel");
            collapsible->body()->addWidget(label);
            lastRegion = g->region;
        }

        g->row = new QWidget(exceptionsCard);
        g->row->setObjectName("groupRow");
        g->row->setAttribute(Qt::WA_StyledBackground, true);
        auto* rowLayout = new QVBoxLayout(g->row);
        rowLayout->setContentsMargins(0, 6, 0, 6);
        rowLayout->setSpacing(6);

        auto* head = new QHBoxLayout();
        head->addWidget(new QLabel(g->title, g->row));
        head->addWidget(new InfoButton(g->info, g->row));
        head->addStretch();
        g->selector = new SegmentedControl({"Wie oben", "Eigene", "Nicht ändern"}, g->row);
        g->selector->setCurrent(g->mode);
        head->addWidget(g->selector);
        rowLayout->addLayout(head);

        g->editorBox = new QWidget(g->row);
        g->editorBox->setObjectName("subEditor");
        g->editorBox->setAttribute(Qt::WA_StyledBackground, true);
        auto* boxLayout = new QVBoxLayout(g->editorBox);
        boxLayout->setContentsMargins(14, 12, 14, 12);
        g->editor = new TrainerSettingsEditor(&g->own, g->editorBox);
        boxLayout->addWidget(g->editor);
        g->editorBox->setVisible(g->mode == Own);
        rowLayout->addWidget(g->editorBox);

        collapsible->body()->addWidget(g->row);

        connect(g->selector, &SegmentedControl::changed, this, [g, this](int mode) {
            g->mode = mode;
            if (mode == Own && !g->ownInitialized) {
                g->own = base; // Startpunkt: die Grundeinstellungen
                g->ownInitialized = true;
                g->editor->refresh();
            }
            g->editorBox->setVisible(mode == Own);
        });
    }
    inner->addWidget(exceptionsCard);

    // Startwerte
    {
        QSignalBlocker b1(masterSwitch), b2(paldeaBox), b3(kitakamiBox), b4(blueberryBox), b5(typeThemeBox);
        masterSwitch->setChecked(trainersEnabled);
        paldeaBox->setChecked(regionPaldea);
        kitakamiBox->setChecked(regionKitakami);
        blueberryBox->setChecked(regionBlueberry);
        typeThemeBox->setChecked(typeTheme);
    }
    updateTrainerStates();

    return wrapPage("Trainer", "Lege fest, wie die Teams aller Trainer zufällig zusammengestellt werden.", content);
}

void ModernRandomizerWindow::updateTrainerStates() {
    if (trainerContent == nullptr) {
        return;
    }
    trainerContent->setEnabled(trainersEnabled);
    paldeaBox->setEnabled(trainersEnabled);
    kitakamiBox->setEnabled(trainersEnabled);
    blueberryBox->setEnabled(trainersEnabled);
    for (auto& g : groups) {
        bool regionOn = (g->region == "paldea" && regionPaldea) ||
                        (g->region == "kitakami" && regionKitakami) ||
                        (g->region == "blueberry" && regionBlueberry);
        g->row->setEnabled(regionOn);
    }
}

void ModernRandomizerWindow::updateSeedInfo() {
    if (seedInfo == nullptr || seedEdit == nullptr) {
        return;
    }
    QString seed = seedEdit->text().trimmed();
    seedInfo->setText(seed.isEmpty() ? "Seed: zufällig" : "Seed: " + seed);
}

// ------------------------------------------------------ An den Randomizer

void ModernRandomizerWindow::applyToRandomizer() {
    code.seed = seedEdit->text().trimmed();
    code.bulk_amount = static_cast<unsigned int>(runsSpin->value());
    code.auto_patch = autoPatch->isChecked();

    svTrainers& t = code.svRandomizerTrainers;
    t.writeSpoiler = spoilerLog->isChecked();
    t.paldeaForAll = false;

    auto assign = [this](trainerSettings& target, const trainerSettings& source) {
        QList<int> indexes = target.randomizedIndex;
        target = source;
        target.randomizedIndex = indexes;
        target.keepTypeTheme = typeTheme;
        target.randomize = false;
        target.keepOriginal = false;
    };

    for (trainerSettings* g : t.allGroups()) {
        g->randomize = false;
        g->keepOriginal = false;
    }

    if (!trainersEnabled) {
        return;
    }

    if (regionPaldea) {
        assign(t.allTrainers, base);
        t.allTrainers.randomize = true;
    }
    if (regionKitakami) {
        assign(t.allKitakamiTrainers, base);
        t.allKitakamiTrainers.randomize = true;
    }
    if (regionBlueberry) {
        assign(t.allBlueberryTrainers, base);
        t.allBlueberryTrainers.randomize = true;
    }

    for (auto& g : groups) {
        bool regionOn = (g->region == "paldea" && regionPaldea) ||
                        (g->region == "kitakami" && regionKitakami) ||
                        (g->region == "blueberry" && regionBlueberry);
        if (!regionOn) {
            continue;
        }
        switch (g->mode) {
        case SameAsBase:
            if (!g->inRegionAll) {
                assign(*g->target, base);
                g->target->randomize = true;
            }
            break;
        case Own:
            assign(*g->target, g->own);
            g->target->randomize = true;
            break;
        case KeepOriginal:
            g->target->keepOriginal = true;
            break;
        }
    }
}

void ModernRandomizerWindow::startRandomizer() {
    if (activeAreas().isEmpty()) {
        QMessageBox::information(this, "Nichts ausgewählt",
                                 "Du hast noch keinen Bereich zum Randomisieren eingeschaltet. "
                                 "Wähle links einen Bereich aus und schalte ihn oben auf der Seite ein.");
        return;
    }
    applyToRandomizer();
    autosave();
    saveOutputSettings();

    // Leerer Seed: zufaelligen Seed auswuerfeln und anzeigen, damit das Ergebnis nachgebaut werden kann
    QString seedText = seedEdit->text().trimmed();
    if (seedText.isEmpty()) {
        seedText = QString::number(QRandomGenerator::global()->bounded(1000000000));
        code.seed = seedText;
    }
    lastSeed = seedText;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = classic->runRandomizerRuns(false, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, "Randomisieren", error);
        return;
    }
    const QStringList folders = classic->lastOutputFolders;
    if (folders.isEmpty()) return;
    writeInfoFiles(folders, seedText);
    const QString first = folders.first();

    bool copied = false;
    QString copyError;
    if (modDirect->isChecked()) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        copied = modoutput::copyToModFolder(first, modFolderEdit->text(), {}, false, &copyError);
        QApplication::restoreOverrideCursor();
    }

    QMessageBox box(this);
    box.setWindowTitle("Fertig");
    box.setIcon(copyError.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    box.setText(folders.size() > 1
                    ? QString("Der Randomizer ist fertig: %1 Durchläufe (Seed %2).").arg(folders.size()).arg(seedText)
                    : "Der Randomizer ist fertig (Seed " + seedText + ").");
    if (copied) {
        box.setInformativeText("Der Ordner „romfs“ aus Randomizer-1 wurde direkt in deinen Mod-Ordner geschrieben:\n" +
                               QDir::toNativeSeparators(modFolderEdit->text().trimmed()) +
                               "\n\nStarte das Spiel im Emulator (neu) – der Mod ist sofort aktiv. Alle Ergebnisse liegen "
                               "außerdem in Randomizers-Output → Karmesin-Purpur.");
    } else if (!copyError.isEmpty()) {
        box.setInformativeText(copyError + "\n\nDie Dateien liegen trotzdem in Randomizers-Output → Karmesin-Purpur und "
                               "können von Hand kopiert werden.");
    } else {
        box.setInformativeText(
            "Kopiere den Ordner „romfs“ aus Randomizer-1 in einen Mod-Ordner deines Spiels:\n\n"
            "• Ryujinx: Rechtsklick auf das Spiel → „Mod-Verzeichnis öffnen“, dort einen Ordner (z. B. Randomizer) anlegen "
            "und „romfs“ hineinkopieren.\n"
            "• Switch (Atmosphère): atmosphere/contents/" + kScarletId + "/romfs (Karmesin) bzw. atmosphere/contents/" +
            kVioletId + "/romfs (Purpur).");
    }
    const QString spoilerPath = first + "/Spoiler-Log.html";
    QPushButton* openMod = copied ? box.addButton("Mod-Ordner öffnen", QMessageBox::ActionRole) : nullptr;
    QPushButton* openFolder = box.addButton("Ordner öffnen", QMessageBox::ActionRole);
    QPushButton* openSpoiler = QFileInfo::exists(spoilerPath) ? box.addButton("Spoiler-Log öffnen", QMessageBox::ActionRole)
                                                              : nullptr;
    box.addButton(QMessageBox::Close);
    box.exec();
    if (openMod != nullptr && box.clickedButton() == openMod) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::fromNativeSeparators(modFolderEdit->text().trimmed())));
    } else if (box.clickedButton() == openFolder) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folders.size() > 1 ? modoutput::gameFolder(modoutput::kScarletViolet)
                                                                         : first));
    } else if (openSpoiler != nullptr && box.clickedButton() == openSpoiler) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(spoilerPath));
    }
}

void ModernRandomizerWindow::writeInfoFiles(const QStringList& folders, const QString& seedText) const {
    const QString settings = QString::fromUtf8(QJsonDocument(settingsToJson()).toJson(QJsonDocument::Indented));
    const QString created = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm");
    for (int i = 0; i < folders.size(); i++) {
        QFile info(folders[i] + "/Info.txt");
        if (!info.open(QIODevice::WriteOnly | QIODevice::Text)) continue;
        QTextStream out(&info);
        out << "Pokémon Karmesin/Purpur Randomizer\n";
        out << "Erstellt: " << created << "\n";
        out << "Seed: " << seedText << "\n";
        if (folders.size() > 1) {
            out << "Durchlauf: " << (i + 1) << " von " << folders.size()
                << " (zum Nachbauen denselben Seed mit derselben Anzahl an Durchläufen verwenden)\n";
        }
        out << "Aktive Bereiche: " << activeAreas().join(", ") << "\n";
        out << "\nEinstellungen:\n" << settings;
    }
}

void ModernRandomizerWindow::findEmulator() {
    const QString path = modoutput::askEmulatorModFolder(this, {{"Karmesin", kScarletId}, {"Purpur", kVioletId}});
    if (path.isEmpty()) return;
    modFolderEdit->setText(QDir::toNativeSeparators(path));
    modDirect->setChecked(true);
    saveOutputSettings();
}

void ModernRandomizerWindow::saveOutputSettings() const {
    if (modFolderEdit == nullptr) return;
    QSettings settings(kSettingsOrg, kSettingsApp);
    settings.setValue("ModFolder", modFolderEdit->text().trimmed());
    settings.setValue("ModDirect", modDirect->isChecked());
}

void ModernRandomizerWindow::loadOutputSettings() {
    QSettings settings(kSettingsOrg, kSettingsApp);
    QSignalBlocker block(modDirect);
    modFolderEdit->setText(settings.value("ModFolder").toString());
    modDirect->setChecked(settings.value("ModDirect", false).toBool());
    modRow->setEnabled(modDirect->isChecked());
}

// ---------------------------------------------------------------- Vorlagen

QJsonObject ModernRandomizerWindow::settingsToJson() const {
    QJsonObject general;
    general["seed"] = seedEdit->text();
    general["runs"] = runsSpin->value();
    general["autoPatch"] = autoPatch->isChecked();
    general["spoilerLog"] = spoilerLog->isChecked();

    QJsonObject trainer;
    trainer["enabled"] = trainersEnabled;
    trainer["paldea"] = regionPaldea;
    trainer["kitakami"] = regionKitakami;
    trainer["blueberry"] = regionBlueberry;
    trainer["typeTheme"] = typeTheme;
    trainer["base"] = trainerSettingsToJson(base);

    QJsonObject groupObject;
    for (const auto& g : groups) {
        QJsonObject entry;
        entry["mode"] = g->mode;
        if (g->ownInitialized) {
            entry["own"] = trainerSettingsToJson(g->own);
        }
        groupObject[g->id] = entry;
    }
    trainer["groups"] = groupObject;

    QJsonObject bools;
    for (const auto& b : boolBindings) {
        bools[b.key] = *b.field;
    }
    QJsonObject limiters;
    for (const auto& l : limiterBindings) {
        limiters[l.key] = limiterToJson(*l.field);
    }
    QJsonObject pagesObject;
    pagesObject["options"] = bools;
    pagesObject["allowedPokemon"] = limiters;
    pagesObject["starters"] = startersToJson();

    QJsonObject root;
    root["format"] = "purpur-randomizer-vorlage";
    root["version"] = 2;
    root["pages"] = pagesObject;
    root["general"] = general;
    root["trainer"] = trainer;
    return root;
}

void ModernRandomizerWindow::settingsFromJson(const QJsonObject& o) {
    QJsonObject general = o["general"].toObject();
    {
        QSignalBlocker b1(seedEdit), b2(runsSpin), b3(autoPatch), b4(spoilerLog);
        seedEdit->setText(general["seed"].toString());
        runsSpin->setValue(general["runs"].toInt(1));
        autoPatch->setChecked(general["autoPatch"].toBool(true));
        spoilerLog->setChecked(general["spoilerLog"].toBool(true));
    }

    QJsonObject trainer = o["trainer"].toObject();
    trainersEnabled = trainer["enabled"].toBool(false);
    regionPaldea = trainer["paldea"].toBool(true);
    regionKitakami = trainer["kitakami"].toBool(true);
    regionBlueberry = trainer["blueberry"].toBool(true);
    typeTheme = trainer["typeTheme"].toBool(true);
    trainerSettingsFromJson(trainer["base"].toObject(), base);

    QJsonObject pagesObject = o["pages"].toObject();
    QJsonObject bools = pagesObject["options"].toObject();
    for (auto& b : boolBindings) {
        // Fehlende Schluessel (aeltere Vorlagen) setzen die Option auf ihren Standardwert zurueck
        *b.field = bools[b.key].toBool(b.defaultValue);
    }
    QJsonObject limiters = pagesObject["allowedPokemon"].toObject();
    for (auto& l : limiterBindings) {
        *l.field = l.defaultValue;
        if (limiters.contains(l.key)) {
            limiterFromJson(limiters[l.key].toObject(), *l.field);
        }
    }
    startersFromJson(pagesObject["starters"].toArray());

    QJsonObject groupObject = trainer["groups"].toObject();
    for (auto& g : groups) {
        if (!groupObject.contains(g->id)) {
            continue;
        }
        QJsonObject entry = groupObject[g->id].toObject();
        g->mode = entry["mode"].toInt(g->mode);
        if (entry.contains("own")) {
            g->own = trainerSettings();
            trainerSettingsFromJson(entry["own"].toObject(), g->own);
            g->ownInitialized = true;
        }
    }
    refreshAll();
}

void ModernRandomizerWindow::refreshAll() {
    {
        QSignalBlocker b1(masterSwitch), b2(paldeaBox), b3(kitakamiBox), b4(blueberryBox), b5(typeThemeBox);
        masterSwitch->setChecked(trainersEnabled);
        paldeaBox->setChecked(regionPaldea);
        kitakamiBox->setChecked(regionKitakami);
        blueberryBox->setChecked(regionBlueberry);
        typeThemeBox->setChecked(typeTheme);
    }
    baseEditor->refresh();
    for (auto& g : groups) {
        QSignalBlocker block(g->selector);
        g->selector->setCurrent(g->mode);
        g->editor->refresh();
        g->editorBox->setVisible(g->mode == Own);
    }
    for (auto& b : boolBindings) {
        {
            QSignalBlocker block(b.box);
            b.box->setChecked(*b.field);
        }
        if (b.extra) {
            b.extra(*b.field);
        }
    }
    for (auto& l : limiterBindings) {
        l.editor->refresh();
    }
    refreshStarters();
    updateTrainerStates();
    updateStatus();
}

void ModernRandomizerWindow::saveSettingsDialog() {
    const QString dir = modoutput::templateFolder(modoutput::kScarletViolet);
    QString path = QFileDialog::getSaveFileName(this, "Einstellungen speichern", dir + "/Meine Einstellungen.json",
                                                "Randomizer-Vorlage (*.json)");
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Speichern fehlgeschlagen", "Die Datei konnte nicht geschrieben werden.");
        return;
    }
    file.write(QJsonDocument(settingsToJson()).toJson(QJsonDocument::Indented));
}

void ModernRandomizerWindow::loadSettingsDialog() {
    // Fruehere Vorlagen (Dokumente/Purpur Randomizer Vorlagen) werden weiterhin gefunden
    const QString dir = modoutput::templateLoadFolder(
        modoutput::kScarletViolet,
        {QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Purpur Randomizer Vorlagen"});
    QString path = QFileDialog::getOpenFileName(this, "Einstellungen laden", dir, "Randomizer-Vorlage (*.json)");
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Laden fehlgeschlagen", "Die Datei konnte nicht geöffnet werden.");
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject() || doc.object()["format"].toString() != "purpur-randomizer-vorlage") {
        QMessageBox::warning(this, "Laden fehlgeschlagen", "Das ist keine gültige Vorlage für diesen Randomizer.");
        return;
    }
    settingsFromJson(doc.object());
}

QString ModernRandomizerWindow::autosavePath() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/zuletzt-verwendet.json";
}

void ModernRandomizerWindow::autosave() const {
    QFile file(autosavePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(settingsToJson()).toJson(QJsonDocument::Indented));
    }
}

void ModernRandomizerWindow::loadAutosave() {
    QFile file(autosavePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject()) {
        settingsFromJson(doc.object());
    }
}
