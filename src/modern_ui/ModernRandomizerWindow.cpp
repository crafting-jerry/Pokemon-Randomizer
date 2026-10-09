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

using namespace modernui;

namespace {
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
    updateStatus();
}

ModernRandomizerWindow::~ModernRandomizerWindow() {
    autosave();
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
        "Erstellt zusätzlich die Datei Spoiler-Log.html mit allen Trainer-Teams. Öffnet sich im Browser und ist durchsuchbar."));
    layout->addWidget(general);

    // Vorlagen
    auto* presets = new Card("Vorlagen", QString(), content);
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
    output->body()->addWidget(mutedLabel("Die Ergebnisse landen im Ordner Randomizers-Output → Randomizer-1. "
                                         "In den Mod-Ordner deines Spiels kopierst du nur den Ordner romfs.", output));
    {
        auto* row = new QHBoxLayout();
        auto* open = new QPushButton("Ausgabeordner öffnen", output);
        open->setObjectName("secondary");
        open->setCursor(Qt::PointingHandCursor);
        row->addWidget(open);
        row->addStretch();
        output->body()->addLayout(row);
        connect(open, &QPushButton::clicked, this, []() {
            QString path = QDir::current().filePath("Randomizers-Output");
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
    classic->runRandomizer();
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
        // Fehlende Schluessel (aeltere Vorlagen) setzen die Option zurueck
        *b.field = bools[b.key].toBool(false);
    }
    QJsonObject limiters = pagesObject["allowedPokemon"].toObject();
    for (auto& l : limiterBindings) {
        *l.field = allowedPokemonLimiter();
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
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Purpur Randomizer Vorlagen";
    QDir().mkpath(dir);
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
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Purpur Randomizer Vorlagen";
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
