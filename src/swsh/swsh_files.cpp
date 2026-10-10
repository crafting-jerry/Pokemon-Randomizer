#include "headers/swsh/swsh_files.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <algorithm>

namespace swsh {

// ------------------------------------------------------------------ Helfer

namespace {

quint16 readU16(const QByteArray& data, int offset) {
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(data.constData() + offset));
}

quint32 readU32(const QByteArray& data, int offset) {
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + offset));
}

void writeU16(QByteArray& data, int offset, quint16 value) {
    qToLittleEndian<quint16>(value, reinterpret_cast<uchar*>(data.data() + offset));
}

void writeU32(QByteArray& data, int offset, quint32 value) {
    qToLittleEndian<quint32>(value, reinterpret_cast<uchar*>(data.data() + offset));
}

// Liest ein Feld aus einer FlatBuffer-Tabelle (Root-Tabelle). Fehlende Felder -> Standardwert.
template <typename T>
T flatField(const QByteArray& buffer, int fieldIndex, T fallback = T()) {
    if (buffer.size() < 8) {
        return fallback;
    }
    const int root = static_cast<int>(readU32(buffer, 0));
    if (root + 4 > buffer.size()) {
        return fallback;
    }
    const qint32 vtableOffset = static_cast<qint32>(readU32(buffer, root));
    const int vtable = root - vtableOffset;
    if (vtable < 0 || vtable + 4 > buffer.size()) {
        return fallback;
    }
    const int vtableSize = readU16(buffer, vtable);
    const int entry = 4 + 2 * fieldIndex;
    if (entry + 2 > vtableSize) {
        return fallback;
    }
    const int fieldOffset = readU16(buffer, vtable + entry);
    if (fieldOffset == 0 || root + fieldOffset + static_cast<int>(sizeof(T)) > buffer.size()) {
        return fallback;
    }
    return qFromLittleEndian<T>(reinterpret_cast<const uchar*>(buffer.constData() + root + fieldOffset));
}

} // namespace

QByteArray readFile(const QString& path, bool* ok) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        return QByteArray();
    }
    if (ok) *ok = true;
    return file.readAll();
}

bool writeFile(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(data) == data.size();
}

// ------------------------------------------------------------- Spieltexte

// Textformat der Switch-Spiele (Gen 7/8): Zeilentabelle, jede Zeile mit
// einem rollierenden 16-Bit-Schluessel verschluesselt.
QStringList decodeMessage(const QByteArray& data) {
    QStringList lines;
    if (data.size() < 0x14) {
        return lines;
    }
    const int lineCount = readU16(data, 0x02);
    const int sectionOffset = static_cast<int>(readU32(data, 0x0C));
    quint16 key = 0x7C89;
    for (int i = 0; i < lineCount; i++) {
        const int entry = sectionOffset + 4 + i * 8;
        if (entry + 8 > data.size()) {
            break;
        }
        const int offset = sectionOffset + static_cast<qint32>(readU32(data, entry));
        const int length = readU16(data, entry + 4);

        QVector<quint16> chars;
        quint16 k = key;
        for (int j = 0; j < length && offset + j * 2 + 2 <= data.size(); j++) {
            chars.append(readU16(data, offset + j * 2) ^ k);
            k = static_cast<quint16>((k << 3) | (k >> 13));
        }
        key = static_cast<quint16>(key + 0x2983);

        QString text;
        for (int j = 0; j < chars.size(); j++) {
            const quint16 c = chars[j];
            if (c == 0) {
                break;
            }
            if (c == 0x10) { // Variable: [0x10, Laenge, ...]
                j += (j + 1 < chars.size()) ? chars[j + 1] + 1 : 1;
                continue;
            }
            switch (c) {
            case 0xE07F: text += ' '; break;
            case 0xE08D: text += QChar(0x2026); break;
            case 0xE08E: text += QChar(0x2642); break;
            case 0xE08F: text += QChar(0x2640); break;
            default: text += QChar(c); break;
            }
        }
        lines.append(text);
    }
    return lines;
}

QStringList readMessage(const QString& romfs, const QString& name) {
    for (const QString& language : {QString("German"), QString("English")}) {
        bool ok = false;
        QByteArray data = readFile(romfs + "/bin/message/" + language + "/common/" + name + ".dat", &ok);
        if (ok) {
            QStringList lines = decodeMessage(data);
            if (!lines.isEmpty()) {
                return lines;
            }
        }
    }
    return QStringList();
}

// ------------------------------------------------------------- Dump-Pruefung

QString versionName(Version version) {
    switch (version) {
    case Version::Sword: return "Pokémon Schwert";
    case Version::Shield: return "Pokémon Schild";
    default: return "unbekannt";
    }
}

QString titleId(Version version) {
    switch (version) {
    case Version::Sword: return "0100ABF008968000";
    case Version::Shield: return "01008DB008C2C000";
    default: return QString();
    }
}

DumpCheck checkDump(const QString& romfs, const QString& exefs) {
    DumpCheck result;

    // --- RomFS ---
    if (romfs.isEmpty()) {
        result.errors << "Kein RomFS-Ordner ausgewählt.";
    } else if (!QDir(romfs).exists()) {
        result.errors << "Der RomFS-Ordner existiert nicht.";
    } else if (!QDir(romfs + "/bin").exists()) {
        result.errors << "Im RomFS-Ordner fehlt der Unterordner „bin“. Wähle den Ordner „romfs“ selbst aus.";
    } else {
        const QStringList required = {path::Personal, path::Learnsets, path::Evolutions, path::Moves,
                                      path::TrainerData, path::TrainerPoke};
        QStringList missing;
        for (const QString& r : required) {
            if (!QFileInfo::exists(romfs + "/" + r)) {
                missing << r;
            }
        }
        if (!missing.isEmpty()) {
            result.errors << "Im RomFS fehlen Dateien: " + missing.join(", ");
        } else {
            qint64 size = QFileInfo(romfs + "/" + path::Personal).size();
            if (size != static_cast<qint64>(kPersonalCount132) * kPersonalSize) {
                result.errors << "Der Dump stammt nicht von Version 1.3.x. Bitte installiere Update 1.3.2 und "
                                 "erstelle den Dump neu.";
            }
        }
    }

    // --- ExeFS (Spielversion) ---
    if (exefs.isEmpty()) {
        result.warnings << "Kein ExeFS-Ordner ausgewählt. Ohne ExeFS kann nicht erkannt werden, ob es Schwert oder "
                           "Schild ist – für Trainer reicht das, für wilde Pokémon wird es später gebraucht.";
    } else {
        bool ok = false;
        QByteArray npdm = readFile(exefs + "/main.npdm", &ok);
        int aci = npdm.indexOf("ACI0");
        if (!ok) {
            result.errors << "Im ExeFS-Ordner fehlt die Datei „main.npdm“.";
        } else if (aci < 0 || aci + 0x18 > npdm.size()) {
            result.errors << "Die Datei „main.npdm“ ist ungültig.";
        } else {
            quint64 id = qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(npdm.constData() + aci + 0x10));
            if (id == 0x0100ABF008968000ULL) {
                result.version = Version::Sword;
            } else if (id == 0x01008DB008C2C000ULL) {
                result.version = Version::Shield;
            } else {
                result.errors << QString("Das ExeFS gehört nicht zu Schwert oder Schild (Title-ID %1).")
                                     .arg(id, 16, 16, QChar('0')).toUpper();
            }
        }
    }

    result.ok = result.errors.isEmpty();
    return result;
}

// -------------------------------------------------- FlatBuffer-Archive

namespace gift {
const QVector<int> kSizes = {4, 1, 1, 4, 1, 8, 1, 4, 1, 4, 1, 1, 2, 1, 1, 8, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 4, 4};
}
namespace encounter {
const QVector<int> kSizes = {8, 8, 1, 1, 1, 1, 1, 1, 1, 1, 4, 8, 1, 1, 4, 1, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 4, 4, 4, 4, 4};
}
namespace trade {
const QVector<int> kSizes = {1, 1, 4, 4, 8, 1, 4, 1, 4, 8, 4, 1, 2, 1, 1, 8, 1, 1, 4, 4, 1, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2};
}

namespace {

qint64 readScalar(const QByteArray& data, int pos, int size) {
    if (pos < 0 || pos + size > data.size()) {
        return 0;
    }
    const uchar* p = reinterpret_cast<const uchar*>(data.constData() + pos);
    switch (size) {
    case 1: return static_cast<qint8>(p[0]);
    case 2: return qFromLittleEndian<qint16>(p);
    case 4: return qFromLittleEndian<qint32>(p);
    default: return qFromLittleEndian<qint64>(p);
    }
}

void appendScalar(QByteArray& data, qint64 value, int size) {
    for (int i = 0; i < size; i++) {
        data.append(static_cast<char>((static_cast<quint64>(value) >> (8 * i)) & 0xFF));
    }
}

void putU32(QByteArray& data, int pos, quint32 value) {
    qToLittleEndian<quint32>(value, reinterpret_cast<uchar*>(data.data() + pos));
}

void alignTo(QByteArray& data, int alignment, int remainder = 0) {
    while (data.size() % alignment != remainder) {
        data.append('\0');
    }
}

} // namespace

QList<FlatRecord> readFlatArchive(const QByteArray& data, const QVector<int>& fieldSizes, bool* ok) {
    QList<FlatRecord> records;
    if (ok) *ok = false;
    if (data.size() < 8) {
        return records;
    }
    auto vtableOf = [&](int table) { return table - static_cast<qint32>(readU32(data, table)); };
    auto fieldOffset = [&](int table, int field) -> int {
        const int vt = vtableOf(table);
        if (vt < 0 || vt + 4 > data.size()) return 0;
        const int vtSize = readU16(data, vt);
        if (4 + 2 * field + 2 > vtSize) return 0;
        return readU16(data, vt + 4 + 2 * field);
    };

    const int root = static_cast<int>(readU32(data, 0));
    const int vecField = fieldOffset(root, 0);
    if (vecField == 0) {
        return records;
    }
    const int vecPos = root + vecField + static_cast<int>(readU32(data, root + vecField));
    if (vecPos < 0 || vecPos + 4 > data.size()) {
        return records;
    }
    const int count = static_cast<int>(readU32(data, vecPos));
    for (int i = 0; i < count; i++) {
        const int slot = vecPos + 4 + 4 * i;
        if (slot + 4 > data.size()) {
            return QList<FlatRecord>();
        }
        const int table = slot + static_cast<int>(readU32(data, slot));
        FlatRecord r;
        r.values.resize(fieldSizes.size());
        for (int f = 0; f < fieldSizes.size(); f++) {
            const int off = fieldOffset(table, f);
            r.values[f] = off ? readScalar(data, table + off, fieldSizes[f]) : 0;
        }
        records.append(r);
    }
    if (ok) *ok = true;
    return records;
}

QByteArray writeFlatArchive(const QList<FlatRecord>& records, const QVector<int>& fieldSizes) {
    QByteArray buf(4, '\0'); // Root-Offset, wird am Ende gesetzt

    // Root: vtable + Tabelle mit einem Feld (Offset auf den Vektor)
    const int rootVt = buf.size();
    appendScalar(buf, 6, 2);   // vtable-Groesse
    appendScalar(buf, 8, 2);   // Tabellengroesse
    appendScalar(buf, 4, 2);   // Feld 0 bei +4
    alignTo(buf, 4);
    const int rootTable = buf.size();
    appendScalar(buf, rootTable - rootVt, 4);
    const int vecRefPos = buf.size();
    appendScalar(buf, 0, 4);
    putU32(buf, 0, static_cast<quint32>(rootTable));

    // Vektor mit Platzhaltern
    const int vecPos = buf.size();
    putU32(buf, vecRefPos, static_cast<quint32>(vecPos - vecRefPos));
    appendScalar(buf, records.size(), 4);
    const int slotBase = buf.size();
    buf.append(QByteArray(4 * records.size(), '\0'));

    // Felder nach Groesse absteigend anordnen (Ausrichtung ohne Luecken)
    QVector<int> order(fieldSizes.size());
    for (int i = 0; i < order.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return fieldSizes[a] > fieldSizes[b]; });
    QVector<int> offsets(fieldSizes.size());
    int tableSize = 4;
    bool has8 = false;
    for (int f : order) {
        offsets[f] = tableSize;
        tableSize += fieldSizes[f];
        has8 |= fieldSizes[f] == 8;
    }

    for (int i = 0; i < records.size(); i++) {
        alignTo(buf, 2);
        const int vt = buf.size();
        appendScalar(buf, 4 + 2 * fieldSizes.size(), 2);
        appendScalar(buf, tableSize, 2);
        for (int f = 0; f < fieldSizes.size(); f++) {
            appendScalar(buf, offsets[f], 2);
        }
        if (has8) alignTo(buf, 8, 4); else alignTo(buf, 4);
        const int table = buf.size();
        appendScalar(buf, table - vt, 4);
        for (int f : order) {
            appendScalar(buf, records[i].get(f), fieldSizes[f]);
        }
        const int slot = slotBase + 4 * i;
        putU32(buf, slot, static_cast<quint32>(table - slot));
    }
    alignTo(buf, 4);
    return buf;
}

// ---------------------------------------------------------------- GFPAK

namespace {
quint64 fnv1a64(const QByteArray& text) {
    quint64 h = 0xCBF29CE484222645ULL;
    for (char c : text) {
        h ^= static_cast<quint8>(c);
        h *= 0x100000001B3ULL;
    }
    return h;
}
} // namespace

QByteArray lz4Decompress(const QByteArray& src, int rawSize) {
    QByteArray out;
    out.reserve(rawSize);
    const uchar* s = reinterpret_cast<const uchar*>(src.constData());
    const int n = src.size();
    int i = 0;
    while (i < n) {
        const int token = s[i++];
        int literal = token >> 4;
        if (literal == 15) {
            int b = 255;
            while (b == 255 && i < n) { b = s[i++]; literal += b; }
        }
        if (i + literal > n) return QByteArray();
        out.append(reinterpret_cast<const char*>(s + i), literal);
        i += literal;
        if (i >= n) break;
        if (i + 2 > n) return QByteArray();
        const int offset = s[i] | (s[i + 1] << 8);
        i += 2;
        int match = token & 15;
        if (match == 15) {
            int b = 255;
            while (b == 255 && i < n) { b = s[i++]; match += b; }
        }
        match += 4;
        if (offset <= 0 || offset > out.size()) return QByteArray();
        const int start = out.size() - offset;
        for (int k = 0; k < match; k++) {
            out.append(out[start + k]);
        }
    }
    return out.size() == rawSize ? out : QByteArray();
}

QByteArray lz4StoreUncompressed(const QByteArray& raw) {
    // Ein einziger Block, der nur aus Literalen besteht (gueltiges LZ4)
    QByteArray out;
    int length = raw.size();
    out.append(static_cast<char>(length >= 15 ? 0xF0 : (length << 4)));
    if (length >= 15) {
        length -= 15;
        while (length >= 255) { out.append(static_cast<char>(255)); length -= 255; }
        out.append(static_cast<char>(length));
    }
    out.append(raw);
    return out;
}

bool GfPak::load(const QByteArray& data) {
    entries.clear();
    nameHashes.clear();
    if (data.size() < 0x28 || !data.startsWith("GFLXPACK")) {
        return false;
    }
    const int fileCount = static_cast<qint32>(readU32(data, 0x10));
    const int folderCount = static_cast<qint32>(readU32(data, 0x14));
    const qint64 ptrFiles = qFromLittleEndian<qint64>(reinterpret_cast<const uchar*>(data.constData() + 0x18));
    if (fileCount <= 0 || ptrFiles <= 0 || ptrFiles + 0x18LL * fileCount > data.size()) {
        return false;
    }
    for (int f = 0; f < folderCount; f++) {
        const qint64 folder = qFromLittleEndian<qint64>(reinterpret_cast<const uchar*>(data.constData() + 0x28 + 8 * f));
        const int count = static_cast<qint32>(readU32(data, static_cast<int>(folder) + 8));
        for (int k = 0; k < count; k++) {
            const int pos = static_cast<int>(folder) + 16 + 16 * k;
            nameHashes.append({qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(data.constData() + pos)),
                               static_cast<qint32>(readU32(data, pos + 8))});
        }
    }
    header = data.left(static_cast<int>(ptrFiles));
    for (int i = 0; i < fileCount; i++) {
        const int pos = static_cast<int>(ptrFiles) + 0x18 * i;
        Entry e;
        e.level = readU16(data, pos);
        e.type = static_cast<quint8>(data[pos + 2]);
        e.rawSize = static_cast<qint32>(readU32(data, pos + 4));
        const int size = static_cast<qint32>(readU32(data, pos + 8));
        const int offset = static_cast<qint32>(readU32(data, pos + 0x10));
        if (offset < 0 || offset + size > data.size()) {
            return false;
        }
        e.stored = data.mid(offset, size);
        entries.append(e);
    }
    return true;
}

int GfPak::indexOf(const QString& fileName) const {
    const quint64 hash = fnv1a64(fileName.toUtf8());
    for (const auto& h : nameHashes) {
        if (h.first == hash) return h.second;
    }
    return -1;
}

QByteArray GfPak::file(int index) const {
    if (index < 0 || index >= entries.size()) return QByteArray();
    const Entry& e = entries[index];
    switch (e.type) {
    case 0: return e.stored;
    case 2: return lz4Decompress(e.stored, e.rawSize);
    default: return QByteArray(); // andere Kompressionen kommen in Schwert/Schild nicht vor
    }
}

void GfPak::setFile(int index, const QByteArray& data) {
    if (index < 0 || index >= entries.size()) return;
    Entry& e = entries[index];
    e.type = 2;
    e.rawSize = data.size();
    e.stored = lz4StoreUncompressed(data);
}

QByteArray GfPak::save() const {
    QByteArray out = header;
    const int tablePos = out.size();
    out.append(QByteArray(0x18 * entries.size(), '\0'));
    for (int i = 0; i < entries.size(); i++) {
        alignTo(out, 16);
        const int offset = out.size();
        out.append(entries[i].stored);
        const int pos = tablePos + 0x18 * i;
        qToLittleEndian<quint16>(entries[i].level, reinterpret_cast<uchar*>(out.data() + pos));
        out[pos + 2] = static_cast<char>(entries[i].type);
        out[pos + 3] = 0;
        putU32(out, pos + 4, static_cast<quint32>(entries[i].rawSize));
        putU32(out, pos + 8, static_cast<quint32>(entries[i].stored.size()));
        putU32(out, pos + 0x0C, 0xCC);
        putU32(out, pos + 0x10, static_cast<quint32>(offset));
        putU32(out, pos + 0x14, 0);
    }
    alignTo(out, 16);
    return out;
}

// ----------------------------------------------------------- Pokemon-Daten

bool PersonalTable::load(const QByteArray& data) {
    if (data.isEmpty() || data.size() % kPersonalSize != 0 || data.size() / kPersonalSize <= kMaxSpecies) {
        return false;
    }
    raw = data;
    buildIndex();
    return true;
}

int PersonalTable::byteAt(int index, int offset) const {
    if (index < 0 || index >= count()) {
        return 0;
    }
    return static_cast<quint8>(raw[index * kPersonalSize + offset]);
}

int PersonalTable::u16At(int index, int offset) const {
    if (index < 0 || index >= count()) {
        return 0;
    }
    return readU16(raw, index * kPersonalSize + offset);
}

void PersonalTable::buildIndex() {
    reverse.clear();
    for (int species = 0; species <= kMaxSpecies; species++) {
        reverse[species] = qMakePair(species, 0);
    }
    for (int species = 1; species <= kMaxSpecies; species++) {
        int forms = formCount(species);
        int first = u16At(species, 0x1E);
        if (forms <= 1 || first <= 0) {
            continue;
        }
        for (int form = 1; form < forms; form++) {
            int index = first + form - 1;
            if (index < count()) {
                reverse[index] = qMakePair(species, form);
            }
        }
    }
}

int PersonalTable::indexOf(int species, int form) const {
    if (species <= 0 || species > kMaxSpecies) {
        return -1;
    }
    if (form == 0) {
        return species;
    }
    int forms = formCount(species);
    int first = u16At(species, 0x1E);
    if (form >= forms || first <= 0) {
        return -1;
    }
    int index = first + form - 1;
    return index < count() ? index : -1;
}

int PersonalTable::stat(int index, int which) const {
    // Reihenfolge in der Datei: KP, Ang, Vert, Init, SpAng, SpVert
    return byteAt(index, qBound(0, which, 5));
}

int PersonalTable::ability(int index, int slot) const {
    return u16At(index, 0x18 + 2 * qBound(0, slot, 2));
}

bool PersonalTable::canLearnTM(int index, int tm) const {
    if (tm < 0 || tm >= 100) {
        return false;
    }
    return (byteAt(index, 0x28 + tm / 8) >> (tm % 8)) & 1;
}

bool PersonalTable::canLearnTR(int index, int tr) const {
    if (tr < 0 || tr >= 100) {
        return false;
    }
    return (byteAt(index, 0x3C + tr / 8) >> (tr % 8)) & 1;
}

const QList<int> kTMMoves = {
    5, 25, 6, 7, 8, 9, 19, 42, 63, 416, 345, 76, 669, 83, 86, 91, 103, 113, 115, 219,
    120, 156, 157, 168, 173, 182, 184, 196, 202, 204, 211, 213, 201, 240, 241, 258, 250, 251, 261, 263,
    129, 270, 279, 280, 286, 291, 311, 313, 317, 328, 331, 333, 340, 341, 350, 362, 369, 371, 372, 374,
    384, 385, 683, 409, 419, 421, 422, 423, 424, 427, 433, 472, 478, 440, 474, 490, 496, 506, 512, 514,
    521, 523, 527, 534, 541, 555, 566, 577, 580, 581, 604, 678, 595, 598, 206, 403, 684, 693, 707, 784
};

const QList<int> kTRMoves = {
    14, 34, 53, 56, 57, 58, 59, 67, 85, 87, 89, 94, 97, 116, 118, 126, 127, 133, 141, 161,
    164, 179, 188, 191, 200, 473, 203, 214, 224, 226, 227, 231, 242, 247, 248, 253, 257, 269, 271, 276,
    285, 299, 304, 315, 322, 330, 334, 337, 339, 347, 348, 349, 360, 370, 390, 394, 396, 398, 399, 402,
    404, 405, 406, 408, 411, 412, 413, 414, 417, 428, 430, 437, 438, 441, 442, 444, 446, 447, 482, 484,
    486, 492, 500, 502, 503, 526, 528, 529, 535, 542, 583, 599, 605, 663, 667, 675, 676, 706, 710, 776
};

const QList<int> kGigantamaxSpecies = {
    6, 12, 25, 52, 68, 94, 99, 131, 133, 143, 569, 809, 823, 826, 834, 839, 841, 842, 844, 849,
    851, 858, 861, 869, 879, 884,
    3, 9, 812, 815, 818, 892 // DLC
};

// -------------------------------------------------------------- Lernsets

bool LearnsetTable::load(const QByteArray& data) {
    if (data.isEmpty() || data.size() % kLearnsetSize != 0) {
        return false;
    }
    raw = data;
    return true;
}

QList<QPair<int,int>> LearnsetTable::get(int index) const {
    QList<QPair<int,int>> result;
    if (index < 0 || index >= count()) {
        return result;
    }
    const int base = index * kLearnsetSize;
    for (int i = 0; i < kLearnsetSize / 4; i++) {
        const int offset = base + i * 4;
        if (static_cast<quint8>(raw[offset + 3]) == 0xFF) {
            break;
        }
        result.append(qMakePair(static_cast<int>(readU16(raw, offset)), static_cast<int>(readU16(raw, offset + 2))));
    }
    return result;
}

void LearnsetTable::set(int index, const QList<QPair<int,int>>& moves) {
    if (index < 0 || index >= count()) {
        return;
    }
    const int base = index * kLearnsetSize;
    for (int i = 0; i < kLearnsetSize / 4; i++) {
        const int offset = base + i * 4;
        if (i < moves.size()) {
            writeU16(raw, offset, static_cast<quint16>(moves[i].first));
            writeU16(raw, offset + 2, static_cast<quint16>(moves[i].second));
        } else {
            writeU32(raw, offset, 0xFFFFFFFFu);
        }
    }
}

// --------------------------------------------------------- Entwicklungen

QString EvolutionTable::fileName(int index) {
    return QString("evo_%1.bin").arg(index, 3, 10, QChar('0'));
}

bool EvolutionTable::load(const QString& folder, int count) {
    files.clear();
    files.resize(count);
    for (int i = 0; i < count; i++) {
        bool ok = false;
        files[i] = readFile(folder + "/" + fileName(i), &ok);
        if (!ok) {
            return false;
        }
    }
    return true;
}

QList<Evolution> EvolutionTable::get(int index) const {
    QList<Evolution> result;
    if (index < 0 || index >= files.size()) {
        return result;
    }
    const QByteArray& data = files[index];
    for (int offset = 0; offset + 8 <= data.size(); offset += 8) {
        Evolution e;
        e.method = readU16(data, offset);
        if (e.method == 0) {
            continue;
        }
        e.argument = readU16(data, offset + 2);
        e.species = readU16(data, offset + 4);
        e.form = static_cast<quint8>(data[offset + 6]);
        e.level = static_cast<quint8>(data[offset + 7]);
        result.append(e);
    }
    return result;
}

bool EvolutionTable::set(int index, const QList<Evolution>& evolutions) {
    if (index < 0 || index >= files.size()) {
        return false;
    }
    QByteArray data = files[index];
    const int capacity = data.size() / 8;
    if (evolutions.size() > capacity) {
        return false; // passt nicht in die Datei
    }
    data.fill('\0');
    for (int i = 0; i < evolutions.size(); i++) {
        const Evolution& e = evolutions[i];
        writeU16(data, i * 8, static_cast<quint16>(e.method));
        writeU16(data, i * 8 + 2, static_cast<quint16>(e.argument));
        writeU16(data, i * 8 + 4, static_cast<quint16>(e.species));
        data[i * 8 + 6] = static_cast<char>(e.form);
        data[i * 8 + 7] = static_cast<char>(e.level);
    }
    files[index] = data;
    if (!changed.contains(index)) {
        changed.append(index);
    }
    return true;
}

bool EvolutionTable::save(const QString& outRomfs) const {
    bool ok = true;
    for (int index : changed) {
        ok &= writeFile(outRomfs + "/" + path::Evolutions + "/" + fileName(index), files[index]);
    }
    return ok;
}

// --------------------------------------------------------------- Attacken

bool MoveTable::load(const QString& folder) {
    moves.clear();
    QDir dir(folder);
    const QStringList names = dir.entryList({"waza*.wazabin"}, QDir::Files, QDir::Name);
    if (names.isEmpty()) {
        return false;
    }
    for (const QString& name : names) {
        bool ok = false;
        QByteArray data = readFile(dir.filePath(name), &ok);
        if (!ok) {
            return false;
        }
        // Feldnummern laut Schema (Waza.fbs fuer Schwert/Schild)
        Move m;
        m.id = static_cast<int>(flatField<quint32>(data, 1));
        m.canUse = flatField<quint8>(data, 2) != 0;
        m.type = flatField<quint8>(data, 3);
        m.category = flatField<quint8>(data, 5);
        m.power = flatField<quint8>(data, 6);
        m.accuracy = flatField<quint8>(data, 7);
        m.priority = flatField<qint8>(data, 9);
        m.hitMax = flatField<quint8>(data, 10);
        m.hitMin = flatField<quint8>(data, 11);
        m.charge = flatField<quint8>(data, 34) != 0;
        m.recharge = flatField<quint8>(data, 35) != 0;
        moves.append(m);
    }
    return true;
}

Move MoveTable::get(int id) const {
    for (const Move& m : moves) {
        if (m.id == id) {
            return m;
        }
    }
    return Move();
}

// ----------------------------------------------------------------- Trainer

int TrainerPoke::level() const { return readU16(raw, 0x0A); }
void TrainerPoke::setLevel(int value) { writeU16(raw, 0x0A, static_cast<quint16>(qBound(1, value, 100))); }
int TrainerPoke::species() const { return readU16(raw, 0x0C); }
void TrainerPoke::setSpecies(int value) { writeU16(raw, 0x0C, static_cast<quint16>(value)); }
int TrainerPoke::form() const { return readU16(raw, 0x0E); }
void TrainerPoke::setForm(int value) { writeU16(raw, 0x0E, static_cast<quint16>(value)); }
int TrainerPoke::heldItem() const { return readU16(raw, 0x10); }
void TrainerPoke::setHeldItem(int value) { writeU16(raw, 0x10, static_cast<quint16>(value)); }
int TrainerPoke::move(int slot) const { return readU16(raw, 0x12 + 2 * qBound(0, slot, 3)); }
void TrainerPoke::setMove(int slot, int value) { writeU16(raw, 0x12 + 2 * qBound(0, slot, 3), static_cast<quint16>(value)); }

void TrainerPoke::setGender(int value) {
    raw[0x00] = static_cast<char>((static_cast<quint8>(raw[0x00]) & 0xFC) | (value & 0x3));
}

void TrainerPoke::setAbility(int value) {
    raw[0x00] = static_cast<char>((static_cast<quint8>(raw[0x00]) & 0xCF) | ((value & 0x3) << 4));
}

bool TrainerPoke::shiny() const {
    return (readU32(raw, 0x1C) >> 30) & 1;
}

void TrainerPoke::setShiny(bool value) {
    quint32 iv = readU32(raw, 0x1C);
    iv = (iv & ~0x40000000u) | (value ? 0x40000000u : 0u);
    writeU32(raw, 0x1C, iv);
}

void TrainerPoke::setPerfectIVs() {
    quint32 iv = readU32(raw, 0x1C);
    iv = (iv & 0xC0000000u) | 0x3FFFFFFFu; // 6 x 31, Schillernd/Dynamax-Bits bleiben
    writeU32(raw, 0x1C, iv);
}

bool TrainerPoke::dynamaxAllowed() const {
    return (readU32(raw, 0x1C) >> 31) & 1;
}

void TrainerPoke::setDynamaxAllowed(bool value) {
    quint32 iv = readU32(raw, 0x1C);
    iv = (iv & ~0x80000000u) | (value ? 0x80000000u : 0u);
    writeU32(raw, 0x1C, iv);
}

int Trainer::trainerClass() const { return readU16(data, 0x00); }
quint32 Trainer::ai() const { return readU32(data, 0x0C); }
void Trainer::setAi(quint32 value) { writeU32(data, 0x0C, value); }

QByteArray Trainer::pokeFile() const {
    QByteArray out;
    for (const TrainerPoke& p : team) {
        out.append(p.raw);
    }
    return out;
}

QString TrainerTable::dataFile(int index) {
    return QString("trainer_data_%1.bin").arg(index, 3, 10, QChar('0'));
}

QString TrainerTable::pokeFile(int index) {
    return QString("trainer_poke_%1.bin").arg(index, 3, 10, QChar('0'));
}

bool TrainerTable::load(const QString& romfs) {
    trainers.clear();
    const QString dataDir = romfs + "/" + path::TrainerData;
    const QString pokeDir = romfs + "/" + path::TrainerPoke;
    for (int i = 0;; i++) {
        if (!QFileInfo::exists(dataDir + "/" + dataFile(i))) {
            break;
        }
        bool ok1 = false;
        bool ok2 = false;
        Trainer t;
        t.index = i;
        t.data = readFile(dataDir + "/" + dataFile(i), &ok1);
        QByteArray team = readFile(pokeDir + "/" + pokeFile(i), &ok2);
        if (!ok1 || !ok2 || t.data.size() != kTrainerDataSize || team.size() % kTrainerPokeSize != 0) {
            return false;
        }
        for (int offset = 0; offset < team.size(); offset += kTrainerPokeSize) {
            TrainerPoke p;
            p.raw = team.mid(offset, kTrainerPokeSize);
            t.team.append(p);
        }
        t.loadedTeamSize = t.team.size();
        trainers.append(t);
    }
    return !trainers.isEmpty();
}

bool TrainerTable::save(const QString& outRomfs, const QList<int>& indexes) const {
    bool ok = true;
    for (int index : indexes) {
        if (index < 0 || index >= trainers.size()) {
            continue;
        }
        const Trainer& t = trainers[index];
        QByteArray data = t.data;
        if (t.team.size() != t.loadedTeamSize) {
            data[3] = static_cast<char>(t.team.size()); // Teamgroesse muss zur Datei passen
        }
        ok &= writeFile(outRomfs + "/" + path::TrainerData + "/" + dataFile(index), data);
        ok &= writeFile(outRomfs + "/" + path::TrainerPoke + "/" + pokeFile(index), t.pokeFile());
    }
    return ok;
}

// ------------------------------------------------------------ Gesamtpaket

bool GameFiles::load(const QString& romfs, QStringList* errors) {
    auto fail = [errors](const QString& message) {
        if (errors) {
            errors->append(message);
        }
        return false;
    };

    if (!personal.load(readFile(romfs + "/" + path::Personal))) {
        return fail("Die Pokémon-Daten konnten nicht gelesen werden.");
    }
    if (!learnsets.load(readFile(romfs + "/" + path::Learnsets))) {
        return fail("Die Lernsets konnten nicht gelesen werden.");
    }
    if (!evolutions.load(romfs + "/" + path::Evolutions, personal.count())) {
        return fail("Die Entwicklungen konnten nicht gelesen werden.");
    }
    if (!moves.load(romfs + "/" + path::Moves)) {
        return fail("Die Attacken konnten nicht gelesen werden.");
    }
    if (!trainers.load(romfs)) {
        return fail("Die Trainer konnten nicht gelesen werden.");
    }
    return true;
}

GameData GameFiles::toGameData() const {
    GameData data;

    for (int index = 1; index < personal.count(); index++) {
        QPair<int,int> sf = personal.speciesForm(index);
        if (sf.first <= 0) {
            continue;
        }

        QList<Evolution> evos = evolutions.get(index);

        GameData::Form form;
        form.natdex = sf.first;
        form.form = sf.second;
        form.stage = personal.evoStage(index);
        form.fullyEvolved = evos.isEmpty();
        data.forms.append(form);

        GameData::Stats stats;
        stats.natdex = sf.first;
        stats.form = sf.second;
        stats.type1 = personal.type1(index);
        stats.type2 = personal.type2(index);
        stats.atk = personal.stat(index, 1);
        stats.spa = personal.stat(index, 4);
        stats.levelMoves = learnsets.get(index);
        for (int i = 0; i < kTMMoves.size(); i++) {
            if (personal.canLearnTM(index, i)) {
                stats.tmMoves.append(kTMMoves[i]);
            }
        }
        for (int i = 0; i < kTRMoves.size(); i++) {
            if (personal.canLearnTR(index, i)) {
                stats.tmMoves.append(kTRMoves[i]);
            }
        }
        data.stats.append(stats);

        for (const Evolution& e : evos) {
            GameData::Evolution ge;
            ge.fromNatdex = sf.first;
            ge.fromForm = sf.second;
            ge.toNatdex = e.species;
            ge.toForm = e.form;
            ge.level = e.level;
            ge.condition = e.method;
            data.evolutions.append(ge);
        }
    }

    for (const Move& m : moves.all()) {
        GameData::Move gm;
        gm.id = m.id;
        gm.type = m.type;
        gm.category = m.category;
        gm.power = m.power;
        gm.accuracy = m.accuracy;
        gm.hitMin = m.hitMin;
        gm.hitMax = m.hitMax;
        gm.priority = m.priority;
        gm.canUse = m.canUse;
        gm.charge = m.charge;
        gm.recharge = m.recharge;
        data.moves.append(gm);
    }
    return data;
}

} // namespace swsh
