// ---------------------------------------------------------------------------
// Gemeinsame Ausgabe-Funktionen fuer alle Spiele (siehe mod_output.h)
// ---------------------------------------------------------------------------

#include "headers/common/mod_output.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QWidget>
#include <algorithm>

namespace modoutput {

QString gameFolder(const QString& game) {
    return QDir::current().filePath("Randomizers-Output/" + game);
}

QString templateFolder(const QString& game) {
    const QString dir = gameFolder(game) + "/Vorlagen";
    QDir().mkpath(dir);
    return dir;
}

QString templateLoadFolder(const QString& game, const QStringList& olderFolders) {
    const QString own = templateFolder(game);
    if (!QDir(own).entryList({"*.json"}, QDir::Files).isEmpty()) return own;
    for (const QString& older : olderFolders) {
        if (!QDir(older).entryList({"*.json"}, QDir::Files).isEmpty()) return older;
    }
    return own;
}

QString askEmulatorModFolder(QWidget* parent, const QList<Title>& titles) {
    if (titles.isEmpty()) return QString();
    // Moegliche Datenordner der Emulatoren (Windows: %APPDATA%, Linux: ~/.config bzw. ~/.local/share, macOS)
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
    struct Hit { QString label, path; bool known; };
    QList<Hit> hits;
    QStringList seen;
    for (const QString& root : roots) {
        for (const Emulator& e : emulators) {
            const QString base = QDir::cleanPath(root + "/" + e.folder);
            if (!QDir(base).exists()) continue;
            for (const Title& t : titles) {
                const QString gameDir = QDir::cleanPath(base + "/" + e.sub + "/" + (e.lower ? t.id.toLower() : t.id));
                const QString path = gameDir + "/Randomizer";
                if (seen.contains(path)) continue;
                seen << path;
                QString label = e.name;
                if (titles.size() > 1) label += " · " + t.name;
                hits.append({label + "  –  " + QDir::toNativeSeparators(path), path, QDir(gameDir).exists()});
            }
        }
    }
    if (hits.isEmpty()) {
        QMessageBox::information(parent, "Emulator suchen",
                                 "Es wurde kein Emulator gefunden (Ryujinx, yuzu, suyu, sudachi, citron, Eden).\n\n"
                                 "Wähle den Mod-Ordner bitte über „Durchsuchen“ aus. In Ryujinx findest du ihn per "
                                 "Rechtsklick auf das Spiel → „Mod-Verzeichnis öffnen“.");
        return QString();
    }
    // Ordner, die es fuer das Spiel schon gibt (Spiel wurde im Emulator schon mit Mods genutzt), zuerst
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.known && !b.known; });
    if (hits.size() == 1) return hits.first().path;
    QStringList labels;
    for (const Hit& h : hits) labels << h.label;
    bool ok = false;
    const QString picked = QInputDialog::getItem(parent, "Emulator suchen",
                                                 titles.size() > 1 ? "Welcher Emulator und welches Spiel?"
                                                                   : "Mehrere Emulatoren gefunden:",
                                                 labels, 0, false, &ok);
    if (!ok) return QString();
    return hits[static_cast<int>(labels.indexOf(picked))].path;
}

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

bool copyToModFolder(const QString& output, const QString& modFolder, const QStringList& protectedFolders,
                     bool replaceExefs, QString* error) {
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };
    const QString folder = QDir::cleanPath(QDir::fromNativeSeparators(modFolder.trimmed()));
    if (folder.isEmpty() || folder == "." || QDir(folder).isRoot()) return fail("Bitte gib einen Mod-Ordner an.");

    const QString romfsTarget = folder + "/romfs";
    const QString exefsTarget = folder + "/exefs";
    // Schutz: niemals den eigenen Dump (oder einen Ordner, der ihn enthaelt) ersetzen
    for (const QString& p : protectedFolders) {
        const QString dump = QFileInfo(QDir::fromNativeSeparators(p.trimmed())).canonicalFilePath();
        if (dump.isEmpty()) continue;
        for (const QString& target : {romfsTarget, exefsTarget}) {
            const QString t = QFileInfo(target).canonicalFilePath();
            if (!t.isEmpty() && (dump == t || dump.startsWith(t + "/"))) {
                return fail("Der Mod-Ordner zeigt auf deinen Dump. Bitte wähle einen eigenen Mod-Ordner.");
            }
        }
    }
    if (QDir(romfsTarget).exists() && !QDir(romfsTarget).removeRecursively()) {
        return fail("Der alte Ordner „romfs“ im Mod-Ordner konnte nicht gelöscht werden:\n" +
                    QDir::toNativeSeparators(romfsTarget));
    }
    if (!copyFolder(output + "/romfs", romfsTarget)) {
        return fail("Die Dateien konnten nicht in den Mod-Ordner kopiert werden:\n" + QDir::toNativeSeparators(romfsTarget));
    }
    const bool hasExefs = QDir(output + "/exefs").exists();
    if ((hasExefs || replaceExefs) && QDir(exefsTarget).exists() && !QDir(exefsTarget).removeRecursively()) {
        return fail("Der alte Ordner „exefs“ im Mod-Ordner konnte nicht gelöscht werden.");
    }
    if (hasExefs && !copyFolder(output + "/exefs", exefsTarget)) {
        return fail("Der ExeFS-Patch konnte nicht in den Mod-Ordner kopiert werden.");
    }
    return true;
}

} // namespace modoutput
