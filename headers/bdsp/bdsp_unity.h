#ifndef BDSP_UNITY_H
#define BDSP_UNITY_H

// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Unity-Dateien
//
// Die Spieldaten liegen in Unity-Asset-Bundles (UnityFS, Unity 2019.4).
// Jedes Bundle enthaelt eine SerializedFile (Version 21) mit Type-Trees,
// darin die eigentlichen Tabellen als MonoBehaviour-Objekte.
//
//  - UnityBundle:    Bundle laden (LZ4/LZ4HC) und wieder speichern (LZ4)
//  - SerializedFile: Objekte lesen und ersetzen, Metadaten bleiben byte-genau
//  - UValue:         Objektinhalt als Baum nach dem Type-Tree (lesen/schreiben)
// ---------------------------------------------------------------------------

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QVector>
#include <memory>
#include <vector>

namespace bdsp {

// LZ4-Block komprimieren (einfacher, schneller Kompressor; gueltiges LZ4)
QByteArray lz4Compress(const QByteArray& raw);
QByteArray lz4Decompress(const QByteArray& src, int rawSize);

// ------------------------------------------------------------- Type-Tree

struct TypeNode {
    QByteArray type;
    QByteArray name;
    int level = 0;
    int byteSize = 0;
    int metaFlag = 0;
    bool isArray = false;
    QVector<int> children; // Indizes in TypeTree::nodes
    bool align() const { return (metaFlag & 0x4000) != 0; }
};

struct TypeTree {
    QVector<TypeNode> nodes; // nodes[0] = Wurzel
    int childIndex(int node, const QByteArray& name) const; // -1 = nicht gefunden
};

// Ein Wert nach dem Type-Tree: Zahl, Text, Liste oder Struktur
class UValue {
public:
    const TypeTree* tree = nullptr;
    int node = -1;
    qint64 i = 0;          // ganze Zahlen und bool
    double f = 0.0;        // float/double
    QByteArray s;          // string
    std::vector<UValue> items; // Struktur-Felder (in Type-Tree-Reihenfolge) oder Listen-Elemente

    bool isValid() const { return tree != nullptr && node >= 0; }
    const TypeNode& typeNode() const { return tree->nodes[node]; }

    // Struktur-Feld nach Namen (ungueltiger Wert, wenn nicht vorhanden)
    UValue& operator[](const char* name);
    const UValue& operator[](const char* name) const;
    bool has(const char* name) const;

    // Liste (vector/Array): Elemente
    std::vector<UValue>& list();
    const std::vector<UValue>& list() const;
    UValue newElement() const; // leeres Element mit passendem Typ (Standardwerte)

    int toInt() const { return static_cast<int>(i); }
    QString toString() const { return QString::fromUtf8(s); }
    void setInt(qint64 value) { i = value; }
    void setString(const QString& value) { s = value.toUtf8(); }
};

// ------------------------------------------------------- SerializedFile

class SerializedFile {
public:
    struct Object {
        qint64 pathId = 0;
        quint32 byteStart = 0;
        quint32 byteSize = 0;
        int typeIndex = 0;
        int tableOffset = 0; // Position des Eintrags in den Metadaten (zum Aktualisieren)
    };
    struct Type {
        int classId = 0;
        int scriptTypeIndex = -1;
        std::shared_ptr<TypeTree> tree;
    };

    bool load(const QByteArray& data, QString* error);
    QByteArray save() const;

    int objectCount() const { return objects.size(); }
    const Object& object(int index) const { return objects[index]; }
    int classId(int index) const { return types.value(objects[index].typeIndex).classId; }

    QByteArray objectData(int index) const;
    void setObjectData(int index, const QByteArray& data);

    // Name eines MonoBehaviour (m_Name), sonst leer
    QString objectName(int index) const;
    // Index des MonoBehaviour mit diesem Namen (-1 = nicht gefunden)
    int findObject(const QString& name) const;

    // Objekt als Baum lesen bzw. zurueckschreiben
    UValue read(int index) const;
    void write(int index, const UValue& value);

    bool isChanged() const { return !replaced.isEmpty(); }

private:
    QByteArray raw;
    quint32 metadataSize = 0;
    quint32 dataOffset = 0;
    QVector<Type> types;
    QVector<Object> objects;
    QHash<int, QByteArray> replaced; // Objekt-Index -> neue Daten
    mutable QHash<QString, int> nameIndex;
};

// ------------------------------------------------------------ UnityBundle

class UnityBundle {
public:
    struct Node {
        QString path;
        quint32 flags = 0;
        QByteArray data;
    };

    bool load(const QByteArray& data, QString* error);
    bool loadFile(const QString& path, QString* error);
    QByteArray save() const;

    QList<Node> nodes;

    // Die SerializedFile des Bundles (erster Knoten mit Flag 4)
    SerializedFile* serialized(QString* error = nullptr);
    // Geaenderte SerializedFile in den Knoten zurueckschreiben
    void commit();

private:
    QByteArray signature, unityVersion, unityRevision;
    quint32 formatVersion = 0;
    quint32 flags = 0;
    std::shared_ptr<SerializedFile> file;
    int fileNode = -1;
};

} // namespace bdsp

#endif // BDSP_UNITY_H
