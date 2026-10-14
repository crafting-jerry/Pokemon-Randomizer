// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Programmcode (ExeFS)
// ---------------------------------------------------------------------------

#include "headers/bdsp/bdsp_exefs.h"
#include "headers/bdsp/bdsp_unity.h" // lz4Decompress

#include <QFile>
#include <QtEndian>
#include <cstring>

namespace bdsp {

namespace {

constexpr quint32 kNsoHeaderSize = 0x100; // IPS-Offsets zaehlen ab Dateianfang inkl. NSO-Kopf
constexpr quint32 F = 0xFFFFFFFFu;

// ARM64: MOVZ Wd, #imm16
quint32 movz(int rd, int imm) { return 0x52800000u | (static_cast<quint32>(imm & 0xFFFF) << 5) | static_cast<quint32>(rd); }
// CMP Wn, #imm12
quint32 cmpImm(int rn, int imm) { return 0x7100001Fu | (static_cast<quint32>(imm) << 10) | (static_cast<quint32>(rn) << 5); }

const QPair<quint32, quint32> kBne = {0x54000001u, 0xFF00001Fu}; // B.NE (beliebiges Ziel)
const quint32 kCselW0W9W8Eq = 0x1A880120u;                       // CSEL W0, W9, W8, EQ
const quint32 kStrW8X23_38 = 0xB9003AE8u;                        // STR W8, [X23, #0x38]
const quint32 kStrW8X20_38 = 0xB9003A88u;                        // STR W8, [X20, #0x38]

} // namespace

bool ExeFsPatcher::load(const QString& exefs, QString* error) {
    auto fail = [error](const QString& m) { if (error) *error = m; return false; };
    text.clear();
    patches.clear();
    QFile file(exefs + "/main");
    if (!file.open(QIODevice::ReadOnly)) return fail("Im ExeFS-Ordner fehlt die Datei „main“.");
    const QByteArray nso = file.readAll();
    if (nso.size() < 0x100 || !nso.startsWith("NSO0")) return fail("Die Datei „main“ ist keine gültige NSO-Datei.");
    auto u32 = [&](int pos) { return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(nso.constData() + pos)); };
    const quint32 flags = u32(0x0C);
    const quint32 fileOffset = u32(0x10);
    const quint32 size = u32(0x18);
    const quint32 compressed = u32(0x60);
    buildId = nso.mid(0x40, 0x20);
    if (static_cast<qint64>(fileOffset) + compressed > nso.size() || size == 0 || size > 0x10000000u) {
        return fail("Die Datei „main“ ist beschädigt.");
    }
    const QByteArray packed = nso.mid(static_cast<int>(fileOffset), static_cast<int>(compressed));
    text = (flags & 1) ? lz4Decompress(packed, static_cast<int>(size)) : packed;
    if (text.size() != static_cast<int>(size)) return fail("Der Programmcode konnte nicht entpackt werden.");
    return true;
}

QString ExeFsPatcher::buildIdName() const {
    int length = buildId.size();
    while (length > 0 && buildId[length - 1] == 0) length--;
    return QString::fromLatin1(buildId.left(qMax(length, 8)).toHex().toUpper());
}

QList<int> ExeFsPatcher::find(const QList<QPair<quint32, quint32>>& pattern) const {
    QList<int> result;
    const int words = text.size() / 4;
    const uchar* data = reinterpret_cast<const uchar*>(text.constData());
    const int n = pattern.size();
    for (int i = 0; i + n <= words; i++) {
        bool match = true;
        for (int k = 0; k < n && match; k++) {
            const quint32 w = qFromLittleEndian<quint32>(data + 4 * (i + k));
            match = (w & pattern[k].second) == pattern[k].first;
        }
        if (match) result.append(i);
    }
    return result;
}

void ExeFsPatcher::setImmediate(int wordIndex, int value) {
    const quint32 address = static_cast<quint32>(wordIndex) * 4;
    quint32 word = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(text.constData() + address));
    word = (word & ~(0xFFFFu << 5)) | (static_cast<quint32>(value & 0xFFFF) << 5);
    qToLittleEndian(word, text.data() + address);
    for (Patch& p : patches) {
        if (p.address == address) { p.word = word; return; }
    }
    patches.append({address, word});
}

bool ExeFsPatcher::patchStarters(int slot0, int slot1, int slot2, QString* error) {
    auto fail = [error](const QString& m) { if (error) *error = m; return false; };
    // Koffer: drei Pokemon mit Art 387/390/393 (MOVZ W8 + STR W8,[X23,#0x38])
    const QList<int> s0 = find({{movz(8, 387), F}, {kStrW8X23_38, F}});
    const QList<int> s1 = find({{movz(8, 390), F}, {kStrW8X23_38, F}});
    const QList<int> s2 = find({{movz(8, 393), F}, {kStrW8X23_38, F}});
    // Spieler-Starter nach Koffer-Platz (11 -> Platz 0, 10 -> Platz 2, sonst Platz 1)
    const QList<int> d0 = find({{cmpImm(8, 11), F}, kBne, {movz(0, 387), F}});
    const QList<int> d1 = find({{cmpImm(8, 10), F}, {movz(8, 390), F}, {movz(9, 393), F}, {kCselW0W9W8Eq, F}});
    // Rivale (Platz 0 -> Platz 1, Platz 2 -> Platz 0, Platz 1 -> Platz 2)
    const QList<int> r0 = find({{cmpImm(8, 11), F}, kBne, {movz(0, 390), F}});
    const QList<int> r1 = find({{cmpImm(8, 10), F}, {movz(8, 393), F}, {movz(9, 387), F}, {kCselW0W9W8Eq, F}});
    // Lucius/Lucia (gleiche Zuordnung wie der Rivale)
    const QList<int> p0 = find({{cmpImm(0, 11), F}, kBne, {movz(0, 390), F}});
    const QList<int> p1 = find({{cmpImm(0, 10), F}, {movz(8, 393), F}, {movz(9, 387), F}, {kCselW0W9W8Eq, F}});
    for (const QList<int>* l : {&s0, &s1, &s2, &d0, &d1, &r0, &r1, &p0, &p1}) {
        if (l->size() != 1) return fail("Die Starter wurden im Programmcode nicht eindeutig gefunden (falsches Update?).");
    }
    const int species[3] = {slot0, slot1, slot2};
    auto newFor = [&](int original) { return original == 387 ? species[0] : (original == 390 ? species[1] : species[2]); };
    setImmediate(s0[0], newFor(387));
    setImmediate(s1[0], newFor(390));
    setImmediate(s2[0], newFor(393));
    setImmediate(d0[0] + 2, newFor(387));
    setImmediate(d1[0] + 1, newFor(390));
    setImmediate(d1[0] + 2, newFor(393));
    setImmediate(r0[0] + 2, newFor(390));
    setImmediate(r1[0] + 1, newFor(393));
    setImmediate(r1[0] + 2, newFor(387));
    setImmediate(p0[0] + 2, newFor(390));
    setImmediate(p1[0] + 1, newFor(393));
    setImmediate(p1[0] + 2, newFor(387));
    return true;
}

bool ExeFsPatcher::patchEevee(int species, QString* error) {
    // RenkeiData.Get_3: MOVZ W8,#133 ; STR W8,[X20,#0x38] ; MOVZ W8,#5 (Level)
    const QList<int> e = find({{movz(8, 133), F}, {kStrW8X20_38, F}, {movz(8, 5), F}});
    if (e.size() != 1) {
        if (error) *error = "Das Evoli-Geschenk wurde im Programmcode nicht eindeutig gefunden (falsches Update?).";
        return false;
    }
    setImmediate(e[0], species);
    return true;
}

QByteArray ExeFsPatcher::ips() const {
    QByteArray out("IPS32");
    for (const Patch& p : patches) {
        char buf[4];
        qToBigEndian(p.address + kNsoHeaderSize, buf);
        out.append(buf, 4);
        qToBigEndian(static_cast<quint16>(4), buf);
        out.append(buf, 2);
        qToLittleEndian(p.word, buf);
        out.append(buf, 4);
    }
    out.append("EEOF");
    return out;
}

QByteArray patchMetadataStarters(const QByteArray& metadata, int slot0, int slot1, int slot2) {
    auto triple = [](int a, int b, int c) {
        QByteArray t(12, '\0');
        qToLittleEndian<qint32>(a, t.data());
        qToLittleEndian<qint32>(b, t.data() + 4);
        qToLittleEndian<qint32>(c, t.data() + 8);
        return t;
    };
    const QByteArray original = triple(387, 390, 393);
    const int pos = metadata.indexOf(original);
    if (pos < 0 || metadata.indexOf(original, pos + 1) >= 0) return QByteArray(); // nicht oder mehrfach gefunden
    QByteArray out = metadata;
    out.replace(pos, 12, triple(slot0, slot1, slot2));
    return out;
}

} // namespace bdsp
