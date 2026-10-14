#ifndef BDSP_EXEFS_H
#define BDSP_EXEFS_H

// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Programmcode (ExeFS)
//
// Die Starter und Evoli (Geschenk von Bebe) stehen fest im Programmcode.
// Die Stellen werden per Mustersuche im entpackten Code (NSO) gefunden, damit
// es mit Diamant und Perle funktioniert. Ergebnis ist ein IPS32-Patch
// (exefs/<Build-ID>.ips), wie ihn Ryujinx, yuzu und Atmosphere laden.
//
// Gefundene Funktionen (Il2Cpp): Dpr.Demo.Demo_Gosanke.Enter (Koffer),
// PlayerWork.get_defaultPokeNo / get_rivalPokeNo / get_supportPokeNo und
// RenkeiData.Get_3 (Evoli). Die Starter-Liste Demo_Gosanke.monsNos steht
// zusaetzlich in Data/Managed/Metadata/global-metadata.dat.
// ---------------------------------------------------------------------------

#include <QByteArray>
#include <QList>
#include <QString>

namespace bdsp {

class ExeFsPatcher {
public:
    // exefs: Ordner mit "main"
    bool load(const QString& exefs, QString* error);

    // Build-ID als Dateiname (Hex, ohne abschliessende Null-Bytes)
    QString buildIdName() const;

    // Starter ersetzen: neue Arten fuer die drei Koffer-Plaetze (Pflanze, Feuer, Wasser im Original)
    bool patchStarters(int slot0, int slot1, int slot2, QString* error);
    // Evoli-Geschenk ersetzen
    bool patchEevee(int species, QString* error);

    bool hasPatches() const { return !patches.isEmpty(); }
    QByteArray ips() const; // IPS32-Datei

private:
    struct Patch { quint32 address; quint32 word; };
    QList<int> find(const QList<QPair<quint32, quint32>>& pattern) const; // (Wert, Maske) -> Wortindizes
    void setImmediate(int wordIndex, int value);

    QByteArray text;    // entpacktes Code-Segment
    QByteArray buildId; // 32 Byte
    QList<Patch> patches;
};

// Starter-Liste in global-metadata.dat ersetzen. Leeres Ergebnis = nicht gefunden.
QByteArray patchMetadataStarters(const QByteArray& metadata, int slot0, int slot1, int slot2);

} // namespace bdsp

#endif // BDSP_EXEFS_H
