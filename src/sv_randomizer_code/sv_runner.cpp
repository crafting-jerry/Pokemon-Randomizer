#include "headers/sv_randomizer_headers/sv_runner.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <random>
#include <stdexcept>

bool SVRandomizerRunner::run(QString* error){
    // Die Bereiche melden Fehler (z. B. zu wenige erlaubte Pokemon) per Ausnahme statt das Programm abzubrechen
    try {
        return runSteps(error);
    } catch (const std::exception& e) {
        if (error) *error = "Beim Randomisieren ist ein Fehler aufgetreten:\n" + QString::fromUtf8(e.what());
    } catch (...) {
        if (error) *error = "Beim Randomisieren ist ein unbekannter Fehler aufgetreten.";
    }
    QDir(QDir::current().filePath("output")).removeRecursively(); // halbfertiges Ergebnis entfernen
    return false;
}

bool SVRandomizerRunner::runSteps(QString* error){
    lastOutputFolder.clear();
    qDebug()<<"Running Randomizer";
    qDebug()<<"Generating seed value for: "<<randomizer.seed;

    quint64 seed;

    if(randomizer.seed.isEmpty()){
        seed = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    }else{
        QByteArray seedArray = randomizer.seed.toUtf8();
        QByteArray hash = QCryptographicHash::hash(seedArray, QCryptographicHash::Sha256);
        seed = *reinterpret_cast<const quint64*>(hash.constData());
    }

    quint32 seed_low = static_cast<quint32>(seed & 0xFFFFFFFF);
    quint32 seed_high = static_cast<quint32>(seed >> 32);

    // Jeder Bereich bekommt einen eigenen, aus dem Seed abgeleiteten Zufallsgenerator
    auto seedModule = [&](QRandomGenerator& rng, quint32 salt) {
        std::seed_seq moduleSeq{seed_low, seed_high, salt};
        rng = QRandomGenerator(moduleSeq);
    };
    seedModule(randomizer.svRandomizerStarters.randNum, 1);
    seedModule(randomizer.svRandomizerPersonal.randNum, 2);
    seedModule(randomizer.svRandomizerItems.randNum, 3);
    seedModule(randomizer.svRandomizerFixed.randNum, 4);
    seedModule(randomizer.svRandomizerWilds.randNum, 5);
    seedModule(randomizer.svRandomizerRaids.randNum, 6);
    seedModule(randomizer.svRandomizerTrainers.randNum, 7);
    seedModule(randomizer.svRandomizerBosses.randNum, 8);

    // Eigener Unterordner fuer Karmesin/Purpur: nur die alten Ergebnisse (Randomizer-1, -2, ...) werden
    // geloescht, Vorlagen und die Ordner der anderen Spiele bleiben erhalten
    const QString outputFolderPath = QDir::current().filePath("Randomizers-Output/Karmesin-Purpur");
    if (!QDir().mkpath(outputFolderPath)) {
        if (error) *error = "Der Ausgabeordner konnte nicht angelegt werden:\n" + QDir::toNativeSeparators(outputFolderPath);
        return false;
    }
    for (const QString& old : QDir(outputFolderPath).entryList({"Randomizer-*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!QDir(outputFolderPath + "/" + old).removeRecursively()) {
            if (error) *error = "Der alte Ordner konnte nicht gelöscht werden (ist eine Datei darin noch geöffnet?):\n" +
                                QDir::toNativeSeparators(outputFolderPath + "/" + old);
            return false;
        }
    }
    // Reste eines abgebrochenen Durchlaufs entfernen
    QDir(QDir::current().filePath("output")).removeRecursively();

    const QString newOutputFolder = outputFolderPath + "/Randomizer-1";

    if(randomizer.svRandomizerStarters.randomizeStarters == true ||
        randomizer.svRandomizerStarters.randomizeGifts == true){

        randomizer.svRandomizerStarters.randomize();
    }

    if(randomizer.svRandomizerPersonal.randomizeAbilities == true||
        randomizer.svRandomizerPersonal.randomizeBST == true ||
        randomizer.svRandomizerPersonal.randomizeEvolutions == true ||
        randomizer.svRandomizerPersonal.randomizeMoveset == true ||
        randomizer.svRandomizerPersonal.randomizeTypes == true ||
        randomizer.svRandomizerPersonal.randomizeTMs == true ||
        randomizer.svRandomizerPersonal.fixEvolutions == true){

        randomizer.svRandomizerPersonal.randomize();
        // Trainer sollen die veraenderten Typen/Attacken/Entwicklungen kennen
        randomizer.svRandomizerTrainers.personalOverride = &randomizer.svRandomizerPersonal.personalMaps;
    }else{
        randomizer.svRandomizerTrainers.personalOverride = nullptr;
    }

    if(randomizer.svRandomizerItems.randomizeItems == true){
        randomizer.svRandomizerItems.randomize();
    }

    if(randomizer.svRandomizerFixed.randomizeFixedEncounters == true){
        randomizer.svRandomizerFixed.randomize();
    }

    if(randomizer.svRandomizerWilds.randomizePaldeaWild == true ||
        randomizer.svRandomizerWilds.randomizeKitakamiWild == true ||
        randomizer.svRandomizerWilds.randomizeBlueberryWild == true){
        randomizer.svRandomizerWilds.randomize();
    }

    // Tera-Raids (vor den Trainern, damit sie im Spoiler-Log der Trainer mit erscheinen)
    randomizer.svRandomizerTrainers.raidSpoiler.clear();
    if (randomizer.svRandomizerRaids.anyEnabled()) {
        randomizer.svRandomizerRaids.personalOverride = randomizer.svRandomizerTrainers.personalOverride;
        QString raidError;
        if (!randomizer.svRandomizerRaids.randomize(&raidError)) {
            if (error) *error = raidError;
            return false;
        }
        randomizer.svRandomizerTrainers.raidSpoiler = randomizer.svRandomizerRaids.spoilerTables;
    }

    bool paldeaTrainers = false;
    bool kitakamiTrainers = false;
    bool blueberryTrainers = false;
    bool trainerBosses = randomizer.svRandomizerBosses.randomize_bosses;
    bool randomizeTrainers = randomizer.svRandomizerTrainers.checkRandomization(paldeaTrainers, kitakamiTrainers, blueberryTrainers);

    if(randomizeTrainers == true){
        randomizer.svRandomizerTrainers.randomize(paldeaTrainers, kitakamiTrainers, blueberryTrainers, trainerBosses);
    }

    if (trainerBosses == true){
        randomizer.svRandomizerBosses.randomizeBosses();
    }

    // Nur Raids randomisiert: Spoiler-Log trotzdem schreiben (ohne Trainer)
    if (randomizeTrainers == false && randomizer.svRandomizerRaids.anyEnabled() &&
        randomizer.svRandomizerTrainers.writeSpoiler) {
        randomizer.svRandomizerTrainers.spoilerTrainers = false;
        randomizer.svRandomizerTrainers.smartData = TrainerSmartData(); // mit den aktuellen Pokemon-Daten neu aufbauen
        randomizer.svRandomizerTrainers.writeSpoilerLog();
        randomizer.svRandomizerTrainers.spoilerTrainers = true;
    }

    if(randomizer.auto_patch == true){
        qDebug()<<"Auto Patching Randomizer";
        randomizer.patchFileDescriptor();

        randomizer.generateBinary(randomizer.qBaseDir.filePath("SV_DATA_FLATBUFFERS/data.fbs").toStdString(),
                       randomizer.qBaseDir.filePath("SV_DATA_FLATBUFFERS/data.json").toStdString(),
                       "arc/", true);

        QDir().remove(randomizer.qBaseDir.filePath("SV_DATA_FLATBUFFERS/data.json"));
    }

    const QString outputFolder = QDir::current().filePath("output");

    if (!QDir(outputFolder).exists()) {
        if (error) *error = "Der Randomizer hat keine Dateien erzeugt (Ordner „output“ fehlt).";
        return false;
    }
    if (!QDir().rename(outputFolder, newOutputFolder)) {
        if (error) *error = "Das Ergebnis konnte nicht verschoben werden nach:\n" + QDir::toNativeSeparators(newOutputFolder);
        return false;
    }
    lastOutputFolder = newOutputFolder;
    return true;
}
