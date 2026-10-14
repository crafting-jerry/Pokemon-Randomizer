#ifndef MOD_OUTPUT_H
#define MOD_OUTPUT_H

// ---------------------------------------------------------------------------
// Gemeinsame Ausgabe-Funktionen fuer alle Spiele
//  - jedes Spiel hat seinen eigenen Unterordner in Randomizers-Output
//    (Ergebnisse und Vorlagen), damit sich die Spiele nicht gegenseitig loeschen
//  - Ergebnis direkt in den Mod-Ordner eines Emulators kopieren
//  - Emulatoren (Ryujinx, yuzu und Ableger) automatisch finden
// ---------------------------------------------------------------------------

#include <QList>
#include <QString>
#include <QStringList>

class QWidget;

namespace modoutput {

// Unterordner der Spiele in Randomizers-Output
const QString kScarletViolet = "Karmesin-Purpur";
const QString kSwordShield = "Schwert-Schild";
const QString kBdsp = "Diamant-Perle";

// Randomizers-Output/<Spiel> (absolut, relativ zum Arbeitsordner)
QString gameFolder(const QString& game);
// Randomizers-Output/<Spiel>/Vorlagen (wird angelegt)
QString templateFolder(const QString& game);
// Startordner fuer "Einstellungen laden": eigener Vorlagen-Ordner; ist er leer, ein frueherer Ort mit Vorlagen
QString templateLoadFolder(const QString& game, const QStringList& olderFolders = QStringList());

struct Title {
    QString name; // z. B. "Purpur"
    QString id;   // Title-ID, z. B. "01008F6008C5E000"
};

// Sucht die Mod-Ordner der installierten Emulatoren und fragt bei mehreren Treffern nach.
// Gibt den gewaehlten Ordner zurueck (leer = nichts gefunden oder abgebrochen; Hinweise wurden angezeigt).
QString askEmulatorModFolder(QWidget* parent, const QList<Title>& titles);

bool copyFolder(const QString& from, const QString& to);

// Kopiert <output>/romfs (und <output>/exefs, falls vorhanden) in den Mod-Ordner; alte Ordner werden ersetzt.
// replaceExefs: einen alten exefs-Ordner auch dann entfernen, wenn es diesmal keinen gibt (veralteter Patch).
// protectedFolders: z. B. der eigene Dump – diese Ordner werden niemals ersetzt.
bool copyToModFolder(const QString& output, const QString& modFolder, const QStringList& protectedFolders,
                     bool replaceExefs, QString* error);

} // namespace modoutput

#endif // MOD_OUTPUT_H
