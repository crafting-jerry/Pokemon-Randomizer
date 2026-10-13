#ifndef COMMON_SPOILER_HTML_H
#define COMMON_SPOILER_HTML_H

// ---------------------------------------------------------------------------
// Gemeinsames Grundgeruest der HTML-Spoiler-Logs (Schwert/Schild, BDSP):
// Kopf mit Stil und Suchleiste, Fuss mit Such- und Filter-Script.
// ---------------------------------------------------------------------------

#include <QString>

namespace spoiler {

// pageTitle: Fenstertitel (z. B. "Schwert/Schild Randomizer"), gameTitle: Ueberschrift,
// info: Zeile unter der Ueberschrift (bereits HTML-sicher)
QString head(const QString& pageTitle, const QString& gameTitle, const QString& info);
QString foot();
bool write(const QString& path, const QString& html);

} // namespace spoiler

#endif // COMMON_SPOILER_HTML_H
