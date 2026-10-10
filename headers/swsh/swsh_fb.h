#ifndef SWSH_FB_H
#define SWSH_FB_H

// ---------------------------------------------------------------------------
// Kleine Helfer zum Lesen und Schreiben von FlatBuffers ohne Schema-Compiler.
//  - FbReader: Felder, Vektoren und Strings einer Tabelle lesen; Positionen
//    liefern, damit vorhandene Werte direkt ueberschrieben werden koennen.
//  - FbWriter: baut einen neuen, gueltigen FlatBuffer im Vorwaerts-Layout
//    (Tabelle -> Vektor -> Kind-Tabellen, alle Offsets zeigen nach hinten).
// ---------------------------------------------------------------------------

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>
#include <QtEndian>
#include <algorithm>

namespace swsh {

// ------------------------------------------------------- FlatBuffer lesen

class FbReader {
public:
    explicit FbReader(const QByteArray& d) : data(d) {}
    bool valid(int pos, int size) const { return pos >= 0 && pos + size <= data.size(); }
    quint32 u32(int pos) const { return valid(pos, 4) ? qFromLittleEndian<quint32>(ptr(pos)) : 0; }
    int field(int table, int index) const {
        if (!valid(table, 4)) return 0;
        const int vt = table - static_cast<qint32>(u32(table));
        if (!valid(vt, 4)) return 0;
        const int vtSize = qFromLittleEndian<quint16>(ptr(vt));
        if (4 + 2 * index + 2 > vtSize || !valid(vt + 4 + 2 * index, 2)) return 0;
        const int off = qFromLittleEndian<quint16>(ptr(vt + 4 + 2 * index));
        return off ? table + off : 0;
    }
    qint64 scalar(int table, int index, int size) const {
        const int pos = field(table, index);
        if (!pos || !valid(pos, size)) return 0;
        switch (size) {
        case 1: return static_cast<quint8>(data[pos]);
        case 2: return qFromLittleEndian<quint16>(ptr(pos));
        case 4: return qFromLittleEndian<qint32>(ptr(pos));
        default: return qFromLittleEndian<qint64>(ptr(pos));
        }
    }
    QList<int> tables(int table, int index) const {
        QList<int> result;
        const int pos = field(table, index);
        if (!pos) return result;
        const int vec = pos + static_cast<int>(u32(pos));
        const int count = static_cast<int>(u32(vec));
        for (int i = 0; i < count && valid(vec + 4 + 4 * i, 4); i++) {
            const int slot = vec + 4 + 4 * i;
            result.append(slot + static_cast<int>(u32(slot)));
        }
        return result;
    }
    int root() const { return static_cast<int>(u32(0)); }

    // Position eines Vektors (zeigt auf die Anzahl) oder 0
    int vectorPos(int table, int index) const {
        const int pos = field(table, index);
        if (!pos) return 0;
        const int vec = pos + static_cast<int>(u32(pos));
        return valid(vec, 4) ? vec : 0;
    }
    int vectorCount(int vec) const { return vec ? static_cast<int>(u32(vec)) : 0; }
    // Unterobjekt (Tabelle) ueber ein Offset-Feld
    int child(int table, int index) const {
        const int pos = field(table, index);
        if (!pos) return 0;
        const int target = pos + static_cast<int>(u32(pos));
        return valid(target, 4) ? target : 0;
    }
    QString string(int table, int index) const {
        const int vec = vectorPos(table, index);
        const int len = vectorCount(vec);
        if (!vec || !valid(vec + 4, len)) return QString();
        return QString::fromUtf8(data.constData() + vec + 4, len);
    }
    quint64 u64(int pos) const { return valid(pos, 8) ? qFromLittleEndian<quint64>(ptr(pos)) : 0; }

private:
    const uchar* ptr(int pos) const { return reinterpret_cast<const uchar*>(data.constData() + pos); }
    const QByteArray& data;
};

// ---------------------------------------------------- FlatBuffer schreiben
// Vorwaerts-Layout: Tabelle -> Vektor -> Kind-Tabellen, alle Offsets zeigen nach hinten.

class FbWriter {
public:
    struct Field {
        int size = 4;        // 1, 2, 4, 8
        qint64 value = 0;
        bool offset = false; // Offset auf einen Vektor (wird spaeter gesetzt)
    };

    // Schreibt vtable + Tabelle, liefert die Positionen der Felder
    QVector<int> table(const QVector<Field>& fields) {
        QVector<int> order(fields.size());
        for (int i = 0; i < order.size(); i++) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return fields[a].size > fields[b].size; });
        QVector<int> offsets(fields.size());
        int size = 4;
        bool has8 = false;
        for (int f : order) {
            offsets[f] = size;
            size += fields[f].size;
            has8 |= fields[f].size == 8;
        }
        align(2);
        const int vt = buf.size();
        put(4 + 2 * fields.size(), 2);
        put(size, 2);
        for (int f = 0; f < fields.size(); f++) put(offsets[f], 2);
        if (has8) { while (buf.size() % 8 != 4) buf.append('\0'); } else align(4);
        const int table = buf.size();
        put(table - vt, 4);
        for (int f : order) put(fields[f].value, fields[f].size);
        QVector<int> positions(fields.size());
        for (int f = 0; f < fields.size(); f++) positions[f] = table + offsets[f];
        return positions;
    }

    // Vektor mit count Platzhaltern; setzt den Offset an "from" auf den Vektor
    int vector(int from, int count) {
        align(4);
        const int vec = buf.size();
        patch(from, vec);
        put(count, 4);
        buf.append(QByteArray(4 * count, '\0'));
        return vec + 4;
    }

    // Vektor aus Zahlen (z. B. [uint]); setzt den Offset an "from"
    void vectorScalars(int from, const QVector<qint64>& values, int size) {
        align(4);
        const int vec = buf.size();
        patch(from, vec);
        put(values.size(), 4);
        for (qint64 v : values) put(v, size);
    }

    void patch(int at, int target) {
        qToLittleEndian<quint32>(static_cast<quint32>(target - at), reinterpret_cast<uchar*>(buf.data() + at));
    }
    QByteArray buf = QByteArray(4, '\0');

private:
    void align(int a) { while (buf.size() % a) buf.append('\0'); }
    void put(qint64 v, int size) {
        for (int i = 0; i < size; i++) buf.append(static_cast<char>((static_cast<quint64>(v) >> (8 * i)) & 0xFF));
    }
};


} // namespace swsh

#endif // SWSH_FB_H
