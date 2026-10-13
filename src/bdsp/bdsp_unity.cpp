// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Unity-Dateien
// (UnityFS-Bundles, SerializedFile Version 21, Type-Tree-Werte)
// ---------------------------------------------------------------------------

#include "headers/bdsp/bdsp_unity.h"

#include <QFile>
#include <QtEndian>
#include <algorithm>
#include <cstring>

namespace bdsp {

namespace {

// Unitys eingebaute Type-Tree-Strings (Offset = Position im Puffer)
const char kCommonStrings[] =
"AABB\0AnimationClip\0AnimationCurve\0AnimationState\0Array\0Base\0BitField\0bitset\0bool\0char\0"
    "ColorRGBA\0Component\0data\0deque\0double\0dynamic_array\0FastPropertyName\0first\0float\0Font\0"
    "GameObject\0Generic Mono\0GradientNEW\0GUID\0GUIStyle\0int\0list\0long long\0map\0Matrix4x4f\0"
    "MdFour\0MonoBehaviour\0MonoScript\0m_ByteSize\0m_Curve\0m_EditorClassIdentifier\0m_EditorHideFlags\0"
    "m_Enabled\0m_ExtensionPtr\0m_GameObject\0m_Index\0m_IsArray\0m_IsStatic\0m_MetaFlag\0m_Name\0"
    "m_ObjectHideFlags\0m_PrefabInternal\0m_PrefabParentObject\0m_Script\0m_StaticEditorFlags\0m_Type\0"
    "m_Version\0Object\0pair\0PPtr<Component>\0PPtr<GameObject>\0PPtr<Material>\0PPtr<MonoBehaviour>\0"
    "PPtr<MonoScript>\0PPtr<Object>\0PPtr<Prefab>\0PPtr<Sprite>\0PPtr<TextAsset>\0PPtr<Texture>\0"
    "PPtr<Texture2D>\0PPtr<Transform>\0Prefab\0Quaternionf\0Rectf\0RectInt\0RectOffset\0second\0set\0"
    "short\0size\0SInt16\0SInt32\0SInt64\0SInt8\0staticvector\0string\0TextAsset\0TextMesh\0Texture\0"
    "Texture2D\0Transform\0TypelessData\0UInt16\0UInt32\0UInt64\0UInt8\0unsigned int\0"
    "unsigned long long\0unsigned short\0vector\0Vector2f\0Vector3f\0Vector4f\0"
    "m_ScriptingClassIdentifier\0Gradient\0Type*\0int2_storage\0int3_storage\0BoundsInt\0"
    "m_CorrespondingSourceObject\0m_PrefabInstance\0m_PrefabAsset\0FileSize\0Hash128\0"
    "RenderingLayerMask\0fixed_array\0EntityId\0LoadableObjectId\0LoadableSceneId\0";

QByteArray commonString(quint32 offset) {
    if (offset >= sizeof(kCommonStrings)) return QByteArray::number(offset);
    return QByteArray(kCommonStrings + offset);
}

// Einfacher Leser mit Positions- und Grenzpruefung
class Reader {
public:
    Reader(const QByteArray& data, int pos = 0) : d(reinterpret_cast<const uchar*>(data.constData())), n(data.size()), p(pos) {}
    bool ok() const { return good; }
    int pos() const { return p; }
    void seek(int position) { p = position; }
    bool need(qint64 bytes) {
        if (!good || bytes < 0 || p + bytes > n) { good = false; return false; }
        return true;
    }
    void align(int a) { p = (p + a - 1) / a * a; }
    void skip(int bytes) { if (need(bytes)) p += bytes; }
    quint8 u8() { if (!need(1)) return 0; return d[p++]; }
    quint16 u16le() { if (!need(2)) return 0; quint16 v = qFromLittleEndian<quint16>(d + p); p += 2; return v; }
    quint32 u32le() { if (!need(4)) return 0; quint32 v = qFromLittleEndian<quint32>(d + p); p += 4; return v; }
    quint64 u64le() { if (!need(8)) return 0; quint64 v = qFromLittleEndian<quint64>(d + p); p += 8; return v; }
    quint16 u16be() { if (!need(2)) return 0; quint16 v = qFromBigEndian<quint16>(d + p); p += 2; return v; }
    quint32 u32be() { if (!need(4)) return 0; quint32 v = qFromBigEndian<quint32>(d + p); p += 4; return v; }
    quint64 u64be() { if (!need(8)) return 0; quint64 v = qFromBigEndian<quint64>(d + p); p += 8; return v; }
    QByteArray bytes(int count) {
        if (!need(count)) return QByteArray();
        QByteArray v(reinterpret_cast<const char*>(d + p), count);
        p += count;
        return v;
    }
    QByteArray cstring() {
        int e = p;
        while (e < n && d[e] != 0) e++;
        if (e >= n) { good = false; return QByteArray(); }
        QByteArray v(reinterpret_cast<const char*>(d + p), e - p);
        p = e + 1;
        return v;
    }

private:
    const uchar* d;
    int n;
    int p;
    bool good = true;
};

void putU16be(QByteArray& out, quint16 v) { char b[2]; qToBigEndian(v, b); out.append(b, 2); }
void putU32be(QByteArray& out, quint32 v) { char b[4]; qToBigEndian(v, b); out.append(b, 4); }
void putU64be(QByteArray& out, quint64 v) { char b[8]; qToBigEndian(v, b); out.append(b, 8); }
void putU32le(QByteArray& out, quint32 v) { char b[4]; qToLittleEndian(v, b); out.append(b, 4); }
void alignOut(QByteArray& out, int a, int base = 0) {
    while ((out.size() - base) % a) out.append('\0');
}

// Primitive Typen: Groesse und Art (0 = keine Primitive)
enum PrimKind { NotPrim, PrimSigned, PrimUnsigned, PrimFloat, PrimDouble };
PrimKind primitive(const QByteArray& type, int* size) {
    struct P { const char* name; int size; PrimKind kind; };
    static const P table[] = {
        {"bool", 1, PrimUnsigned}, {"SInt8", 1, PrimSigned}, {"UInt8", 1, PrimUnsigned}, {"char", 1, PrimSigned},
        {"SInt16", 2, PrimSigned}, {"short", 2, PrimSigned}, {"UInt16", 2, PrimUnsigned}, {"unsigned short", 2, PrimUnsigned},
        {"SInt32", 4, PrimSigned}, {"int", 4, PrimSigned}, {"UInt32", 4, PrimUnsigned}, {"unsigned int", 4, PrimUnsigned},
        {"Type*", 4, PrimUnsigned}, {"float", 4, PrimFloat},
        {"SInt64", 8, PrimSigned}, {"long long", 8, PrimSigned}, {"UInt64", 8, PrimUnsigned},
        {"unsigned long long", 8, PrimUnsigned}, {"FileSize", 8, PrimUnsigned}, {"double", 8, PrimDouble},
    };
    for (const P& p : table) {
        if (type == p.name) { *size = p.size; return p.kind; }
    }
    return NotPrim;
}

UValue readValue(const TypeTree* tree, int index, Reader& r, int depth) {
    UValue v;
    v.tree = tree;
    v.node = index;
    const TypeNode& n = tree->nodes[index];
    if (depth > 64) return v;
    bool align = n.align();
    int size = 0;
    const PrimKind kind = primitive(n.type, &size);
    if (n.isArray && n.children.size() >= 2) {
        const qint32 count = static_cast<qint32>(r.u32le());
        if (count < 0 || !r.need(count)) return v; // jedes Element ist mindestens 1 Byte (oder leer)
        const int elem = n.children[1];
        v.items.reserve(static_cast<size_t>(count));
        for (int k = 0; k < count && r.ok(); k++) v.items.push_back(readValue(tree, elem, r, depth + 1));
    } else if (n.type == "string") {
        const qint32 len = static_cast<qint32>(r.u32le());
        v.s = r.bytes(len);
        if (!n.children.isEmpty() && tree->nodes[n.children[0]].align()) align = true;
    } else if (kind != NotPrim) {
        switch (size) {
        case 1: v.i = kind == PrimSigned ? static_cast<qint8>(r.u8()) : r.u8(); break;
        case 2: { const quint16 x = r.u16le(); v.i = kind == PrimSigned ? static_cast<qint16>(x) : x; break; }
        case 4: {
            const quint32 x = r.u32le();
            if (kind == PrimFloat) { float fl; std::memcpy(&fl, &x, 4); v.f = fl; }
            else v.i = kind == PrimSigned ? static_cast<qint32>(x) : static_cast<qint64>(x);
            break;
        }
        default: {
            const quint64 x = r.u64le();
            if (kind == PrimDouble) { double db; std::memcpy(&db, &x, 8); v.f = db; }
            else v.i = static_cast<qint64>(x);
            break;
        }
        }
    } else {
        v.items.reserve(static_cast<size_t>(n.children.size()));
        for (int c : n.children) {
            if (!r.ok()) break;
            v.items.push_back(readValue(tree, c, r, depth + 1));
        }
    }
    if (align) r.align(4);
    return v;
}

void writeValue(const UValue& v, QByteArray& out, int base) {
    const TypeTree* tree = v.tree;
    const TypeNode& n = tree->nodes[v.node];
    bool align = n.align();
    int size = 0;
    const PrimKind kind = primitive(n.type, &size);
    if (n.isArray && n.children.size() >= 2) {
        putU32le(out, static_cast<quint32>(v.items.size()));
        for (const UValue& e : v.items) writeValue(e, out, base);
    } else if (n.type == "string") {
        putU32le(out, static_cast<quint32>(v.s.size()));
        out.append(v.s);
        if (!n.children.isEmpty() && tree->nodes[n.children[0]].align()) align = true;
    } else if (kind != NotPrim) {
        char b[8];
        switch (size) {
        case 1: out.append(static_cast<char>(v.i & 0xFF)); break;
        case 2: qToLittleEndian(static_cast<quint16>(v.i), b); out.append(b, 2); break;
        case 4:
            if (kind == PrimFloat) { const float fl = static_cast<float>(v.f); std::memcpy(b, &fl, 4); }
            else qToLittleEndian(static_cast<quint32>(v.i), b);
            out.append(b, 4);
            break;
        default:
            if (kind == PrimDouble) std::memcpy(b, &v.f, 8);
            else qToLittleEndian(static_cast<quint64>(v.i), b);
            out.append(b, 8);
            break;
        }
    } else {
        for (const UValue& c : v.items) writeValue(c, out, base);
    }
    if (align) alignOut(out, 4, base);
}

UValue defaultValue(const TypeTree* tree, int index) {
    UValue v;
    v.tree = tree;
    v.node = index;
    const TypeNode& n = tree->nodes[index];
    int size = 0;
    if (n.isArray || n.type == "string" || primitive(n.type, &size) != NotPrim) return v;
    for (int c : n.children) v.items.push_back(defaultValue(tree, c));
    return v;
}

} // namespace

// ------------------------------------------------------------------ LZ4

QByteArray lz4Decompress(const QByteArray& src, int rawSize) {
    QByteArray out;
    out.reserve(rawSize);
    const uchar* s = reinterpret_cast<const uchar*>(src.constData());
    const int n = src.size();
    int p = 0;
    while (p < n) {
        const int token = s[p++];
        int lit = token >> 4;
        if (lit == 15) {
            int b;
            do { if (p >= n) return QByteArray(); b = s[p++]; lit += b; } while (b == 255);
        }
        if (p + lit > n) return QByteArray();
        out.append(reinterpret_cast<const char*>(s + p), lit);
        p += lit;
        if (p >= n) break; // letzte Sequenz: nur Literale
        if (p + 2 > n) return QByteArray();
        const int offset = s[p] | (s[p + 1] << 8);
        p += 2;
        int len = (token & 15);
        if (len == 15) {
            int b;
            do { if (p >= n) return QByteArray(); b = s[p++]; len += b; } while (b == 255);
        }
        len += 4;
        const int start = out.size() - offset;
        if (offset <= 0 || start < 0) return QByteArray();
        for (int k = 0; k < len; k++) out.append(out.at(start + k)); // Ueberlappung erlaubt
    }
    if (out.size() != rawSize) return QByteArray();
    return out;
}

QByteArray lz4Compress(const QByteArray& raw) {
    const uchar* src = reinterpret_cast<const uchar*>(raw.constData());
    const int n = raw.size();
    QByteArray out;
    out.reserve(n / 2 + 64);
    auto putLength = [&](int len) {
        while (len >= 255) { out.append(static_cast<char>(255)); len -= 255; }
        out.append(static_cast<char>(len));
    };
    auto emitSeq = [&](int litStart, int litLen, int offset, int matchLen) {
        const int litTok = litLen >= 15 ? 15 : litLen;
        const int mTok = matchLen < 0 ? 0 : (matchLen - 4 >= 15 ? 15 : matchLen - 4);
        out.append(static_cast<char>((litTok << 4) | mTok));
        if (litLen >= 15) putLength(litLen - 15);
        out.append(reinterpret_cast<const char*>(src + litStart), litLen);
        if (matchLen < 0) return;
        out.append(static_cast<char>(offset & 0xFF));
        out.append(static_cast<char>(offset >> 8));
        if (matchLen - 4 >= 15) putLength(matchLen - 4 - 15);
    };
    auto read32 = [&](int i) { quint32 v; std::memcpy(&v, src + i, 4); return v; };

    int anchor = 0;
    if (n >= 13) {
        std::vector<int> table(1 << 16, -1);
        const int limit = n - 12;
        int i = 0;
        while (i < limit) {
            const quint32 seq = read32(i);
            const quint32 h = (seq * 2654435761u) >> 16;
            const int ref = table[h];
            table[h] = i;
            if (ref >= 0 && i - ref <= 65535 && read32(ref) == seq) {
                int len = 4;
                while (i + len < n - 5 && src[ref + len] == src[i + len]) len++;
                emitSeq(anchor, i - anchor, i - ref, len);
                i += len;
                anchor = i;
            } else {
                i++;
            }
        }
    }
    emitSeq(anchor, n - anchor, 0, -1);
    return out;
}

// ------------------------------------------------------------- Type-Tree

int TypeTree::childIndex(int node, const QByteArray& name) const {
    const TypeNode& n = nodes[node];
    for (int k = 0; k < n.children.size(); k++) {
        if (nodes[n.children[k]].name == name) return k;
    }
    return -1;
}

UValue& UValue::operator[](const char* name) {
    static UValue invalid;
    if (!isValid()) { invalid = UValue(); return invalid; }
    const int k = tree->childIndex(node, QByteArray(name));
    if (k < 0 || k >= static_cast<int>(items.size())) { invalid = UValue(); return invalid; }
    return items[static_cast<size_t>(k)];
}

const UValue& UValue::operator[](const char* name) const {
    static const UValue invalid;
    if (!isValid()) return invalid;
    const int k = tree->childIndex(node, QByteArray(name));
    if (k < 0 || k >= static_cast<int>(items.size())) return invalid;
    return items[static_cast<size_t>(k)];
}

bool UValue::has(const char* name) const {
    return isValid() && tree->childIndex(node, QByteArray(name)) >= 0;
}

std::vector<UValue>& UValue::list() {
    if (isValid() && !typeNode().isArray && !items.empty() && items[0].isValid() && items[0].typeNode().isArray) {
        return items[0].items;
    }
    return items;
}

const std::vector<UValue>& UValue::list() const {
    if (isValid() && !typeNode().isArray && !items.empty() && items[0].isValid() && items[0].typeNode().isArray) {
        return items[0].items;
    }
    return items;
}

UValue UValue::newElement() const {
    if (!isValid()) return UValue();
    int arrayNode = node;
    if (!typeNode().isArray && !typeNode().children.isEmpty()) arrayNode = typeNode().children[0];
    const TypeNode& a = tree->nodes[arrayNode];
    if (!a.isArray || a.children.size() < 2) return UValue();
    return defaultValue(tree, a.children[1]);
}

// ------------------------------------------------------- SerializedFile

bool SerializedFile::load(const QByteArray& data, QString* error) {
    auto fail = [error](const QString& m) { if (error) *error = m; return false; };
    raw = data;
    types.clear();
    objects.clear();
    replaced.clear();
    nameIndex.clear();

    Reader r(raw);
    metadataSize = r.u32be();
    const quint32 fileSize = r.u32be();
    const quint32 version = r.u32be();
    dataOffset = r.u32be();
    const quint8 bigEndian = r.u8();
    r.skip(3);
    if (!r.ok() || version < 19 || version > 21 || bigEndian != 0 || fileSize > static_cast<quint32>(raw.size())) {
        return fail(QString("Unbekanntes Unity-Dateiformat (Version %1).").arg(version));
    }

    r.cstring(); // Unity-Version
    r.u32le();   // Plattform
    const bool typeTrees = r.u8() != 0;
    if (!typeTrees) return fail("Die Unity-Datei enthält keine Type-Trees.");

    const qint32 typeCount = static_cast<qint32>(r.u32le());
    if (typeCount < 0 || typeCount > 10000) return fail("Ungültige Typ-Tabelle.");
    for (int t = 0; t < typeCount && r.ok(); t++) {
        Type type;
        type.classId = static_cast<qint32>(r.u32le());
        r.u8(); // isStrippedType
        type.scriptTypeIndex = static_cast<qint16>(r.u16le());
        if (type.classId < 0 || type.classId == 114) r.skip(16); // ScriptID
        r.skip(16);                                                 // OldTypeHash

        auto tree = std::make_shared<TypeTree>();
        const qint32 nodeCount = static_cast<qint32>(r.u32le());
        const qint32 stringSize = static_cast<qint32>(r.u32le());
        if (nodeCount < 0 || stringSize < 0 || !r.need(static_cast<qint64>(nodeCount) * 32 + stringSize)) {
            return fail("Ungültiger Type-Tree.");
        }
        const int nodeStart = r.pos();
        const QByteArray strings = raw.mid(nodeStart + nodeCount * 32, stringSize);
        auto str = [&](quint32 offset) -> QByteArray {
            if (offset & 0x80000000u) return commonString(offset & 0x7FFFFFFFu);
            if (offset >= static_cast<quint32>(strings.size())) return QByteArray();
            return QByteArray(strings.constData() + offset);
        };
        QVector<int> stack;
        for (int k = 0; k < nodeCount; k++) {
            TypeNode node;
            r.u16le(); // version
            node.level = r.u8();
            node.isArray = (r.u8() & 1) != 0;
            node.type = str(r.u32le());
            node.name = str(r.u32le());
            node.byteSize = static_cast<qint32>(r.u32le());
            r.u32le(); // index
            node.metaFlag = static_cast<qint32>(r.u32le());
            r.u64le(); // RefTypeHash
            while (!stack.isEmpty() && tree->nodes[stack.last()].level >= node.level) stack.removeLast();
            const int index = tree->nodes.size();
            if (!stack.isEmpty()) tree->nodes[stack.last()].children.append(index);
            tree->nodes.append(node);
            stack.append(index);
        }
        r.skip(stringSize);
        if (version >= 21) {
            const qint32 deps = static_cast<qint32>(r.u32le());
            if (deps < 0) return fail("Ungültige Typ-Abhängigkeiten.");
            r.skip(deps * 4);
        }
        type.tree = tree;
        types.append(type);
    }

    const qint32 objectCount = static_cast<qint32>(r.u32le());
    if (!r.ok() || objectCount < 0 || objectCount > 1000000) return fail("Ungültige Objekt-Tabelle.");
    for (int k = 0; k < objectCount && r.ok(); k++) {
        r.align(4);
        Object o;
        o.pathId = static_cast<qint64>(r.u64le());
        o.tableOffset = r.pos();
        o.byteStart = r.u32le();
        o.byteSize = r.u32le();
        o.typeIndex = static_cast<qint32>(r.u32le());
        if (static_cast<quint64>(dataOffset) + o.byteStart + o.byteSize > static_cast<quint64>(raw.size()) ||
            o.typeIndex < 0 || o.typeIndex >= types.size()) {
            return fail("Ungültiger Objekt-Eintrag.");
        }
        objects.append(o);
    }
    if (!r.ok()) return fail("Die Unity-Datei ist unvollständig.");
    return true;
}

QByteArray SerializedFile::objectData(int index) const {
    auto it = replaced.constFind(index);
    if (it != replaced.constEnd()) return it.value();
    const Object& o = objects[index];
    return raw.mid(static_cast<int>(dataOffset + o.byteStart), static_cast<int>(o.byteSize));
}

void SerializedFile::setObjectData(int index, const QByteArray& data) {
    replaced[index] = data;
}

QString SerializedFile::objectName(int index) const {
    if (classId(index) != 114) return QString();
    const Object& o = objects[index];
    // MonoBehaviour: m_GameObject (PPtr, 12) + m_Enabled (1, auf 4) + m_Script (PPtr, 12) -> m_Name
    const int start = static_cast<int>(dataOffset + o.byteStart);
    if (o.byteSize < 32) return QString();
    Reader r(raw, start + 28);
    const qint32 len = static_cast<qint32>(r.u32le());
    if (len < 0 || len > 1024 || static_cast<quint32>(32 + len) > o.byteSize) return QString();
    return QString::fromUtf8(r.bytes(len));
}

int SerializedFile::findObject(const QString& name) const {
    if (nameIndex.isEmpty()) {
        for (int k = 0; k < objects.size(); k++) {
            const QString n = objectName(k);
            if (!n.isEmpty() && !nameIndex.contains(n)) nameIndex.insert(n, k);
        }
    }
    return nameIndex.value(name, -1);
}

UValue SerializedFile::read(int index) const {
    const QByteArray data = objectData(index);
    Reader r(data);
    const TypeTree* tree = types[objects[index].typeIndex].tree.get();
    if (tree->nodes.isEmpty()) return UValue();
    UValue v = readValue(tree, 0, r, 0);
    if (!r.ok()) return UValue();
    return v;
}

void SerializedFile::write(int index, const UValue& value) {
    QByteArray out;
    writeValue(value, out, 0);
    setObjectData(index, out);
}

QByteArray SerializedFile::save() const {
    QByteArray out = raw.left(static_cast<int>(dataOffset));
    // Objekte in der urspruenglichen Reihenfolge im Datenbereich, jeweils auf 8 Byte ausgerichtet
    QVector<int> order(objects.size());
    for (int k = 0; k < order.size(); k++) order[k] = k;
    std::sort(order.begin(), order.end(), [this](int a, int b) { return objects[a].byteStart < objects[b].byteStart; });
    quint32 lastEnd = 0;
    for (int k : order) lastEnd = qMax(lastEnd, objects[k].byteStart + objects[k].byteSize);
    const int trailing = qMax(0, static_cast<int>(raw.size()) - static_cast<int>(dataOffset + lastEnd));

    for (int k : order) {
        alignOut(out, 8, static_cast<int>(dataOffset));
        const quint32 start = static_cast<quint32>(out.size()) - dataOffset;
        const QByteArray data = objectData(k);
        out.append(data);
        qToLittleEndian(start, out.data() + objects[k].tableOffset);
        qToLittleEndian(static_cast<quint32>(data.size()), out.data() + objects[k].tableOffset + 4);
    }
    out.append(QByteArray(trailing, '\0'));
    qToBigEndian(static_cast<quint32>(out.size()), out.data() + 4);
    return out;
}

// ------------------------------------------------------------ UnityBundle

bool UnityBundle::loadFile(const QString& path, QString* error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = "Datei nicht gefunden: " + path;
        return false;
    }
    return load(f.readAll(), error);
}

bool UnityBundle::load(const QByteArray& data, QString* error) {
    auto fail = [error](const QString& m) { if (error) *error = m; return false; };
    nodes.clear();
    file.reset();
    fileNode = -1;

    Reader r(data);
    signature = r.cstring();
    if (signature != "UnityFS") return fail("Keine UnityFS-Datei.");
    formatVersion = r.u32be();
    unityVersion = r.cstring();
    unityRevision = r.cstring();
    r.u64be(); // Gesamtgroesse
    const quint32 infoCompressed = r.u32be();
    const quint32 infoSize = r.u32be();
    flags = r.u32be();
    if (!r.ok() || formatVersion < 6 || formatVersion > 8) return fail("Unbekannte UnityFS-Version.");
    if (formatVersion >= 7) r.align(16);

    int infoPos = r.pos();
    if (flags & 0x80) infoPos = data.size() - static_cast<int>(infoCompressed);
    const QByteArray infoPacked = data.mid(infoPos, static_cast<int>(infoCompressed));
    QByteArray info;
    switch (flags & 0x3F) {
    case 0: info = infoPacked; break;
    case 2: case 3: info = lz4Decompress(infoPacked, static_cast<int>(infoSize)); break;
    default: return fail("Nicht unterstützte Kompression im Bundle.");
    }
    if (info.size() != static_cast<int>(infoSize)) return fail("Die Bundle-Informationen sind beschädigt.");
    if (!(flags & 0x80)) r.seek(infoPos + static_cast<int>(infoCompressed));
    if (flags & 0x200) r.align(16);

    Reader ir(info, 16);
    const qint32 blockCount = static_cast<qint32>(ir.u32be());
    if (blockCount < 0 || blockCount > 100000) return fail("Ungültige Blockanzahl im Bundle.");
    QByteArray stream;
    int pos = r.pos();
    for (int b = 0; b < blockCount && ir.ok(); b++) {
        const quint32 usize = ir.u32be();
        const quint32 csize = ir.u32be();
        const quint16 bflags = ir.u16be();
        if (pos + static_cast<qint64>(csize) > data.size()) return fail("Das Bundle ist unvollständig.");
        const QByteArray packed = data.mid(pos, static_cast<int>(csize));
        pos += static_cast<int>(csize);
        switch (bflags & 0x3F) {
        case 0: stream.append(packed); break;
        case 2: case 3: {
            const QByteArray block = lz4Decompress(packed, static_cast<int>(usize));
            if (block.size() != static_cast<int>(usize)) return fail("Ein Block im Bundle ist beschädigt.");
            stream.append(block);
            break;
        }
        default: return fail("Nicht unterstützte Block-Kompression im Bundle.");
        }
    }
    const qint32 nodeCount = static_cast<qint32>(ir.u32be());
    if (nodeCount < 0 || nodeCount > 10000) return fail("Ungültige Knoten im Bundle.");
    for (int k = 0; k < nodeCount && ir.ok(); k++) {
        Node node;
        const qint64 offset = static_cast<qint64>(ir.u64be());
        const qint64 size = static_cast<qint64>(ir.u64be());
        node.flags = ir.u32be();
        node.path = QString::fromUtf8(ir.cstring());
        if (offset < 0 || size < 0 || offset + size > stream.size()) return fail("Ungültiger Knoten im Bundle.");
        node.data = stream.mid(static_cast<int>(offset), static_cast<int>(size));
        nodes.append(node);
    }
    if (!ir.ok()) return fail("Die Bundle-Informationen sind unvollständig.");
    return true;
}

SerializedFile* UnityBundle::serialized(QString* error) {
    if (file) return file.get();
    for (int k = 0; k < nodes.size(); k++) {
        if (nodes[k].flags & 4) {
            auto f = std::make_shared<SerializedFile>();
            if (!f->load(nodes[k].data, error)) return nullptr;
            file = f;
            fileNode = k;
            return file.get();
        }
    }
    if (error) *error = "Das Bundle enthält keine Unity-Daten.";
    return nullptr;
}

void UnityBundle::commit() {
    if (file && fileNode >= 0 && file->isChanged()) nodes[fileNode].data = file->save();
}

QByteArray UnityBundle::save() const {
    QByteArray stream;
    for (const Node& n : nodes) stream.append(n.data);

    // Bloecke zu je 128 KB, LZ4-komprimiert (wie im Spiel)
    constexpr int kBlock = 0x20000;
    QByteArray blockData;
    QByteArray info(16, '\0'); // Hash (wird nicht geprueft)
    const int blockCount = (stream.size() + kBlock - 1) / kBlock;
    putU32be(info, static_cast<quint32>(blockCount));
    for (int b = 0; b < blockCount; b++) {
        const QByteArray chunk = stream.mid(b * kBlock, kBlock);
        const QByteArray packed = lz4Compress(chunk);
        const bool compress = packed.size() < chunk.size();
        putU32be(info, static_cast<quint32>(chunk.size()));
        putU32be(info, static_cast<quint32>(compress ? packed.size() : chunk.size()));
        putU16be(info, compress ? 2 : 0);
        blockData.append(compress ? packed : chunk);
    }
    putU32be(info, static_cast<quint32>(nodes.size()));
    qint64 offset = 0;
    for (const Node& n : nodes) {
        putU64be(info, static_cast<quint64>(offset));
        putU64be(info, static_cast<quint64>(n.data.size()));
        putU32be(info, n.flags);
        info.append(n.path.toUtf8());
        info.append('\0');
        offset += n.data.size();
    }
    const QByteArray infoPacked = lz4Compress(info);
    const quint32 outFlags = (flags & ~0x3Fu & ~0x80u & ~0x200u) | 0x40u | 2u;

    QByteArray out;
    out.append(signature);
    out.append('\0');
    putU32be(out, formatVersion);
    out.append(unityVersion);
    out.append('\0');
    out.append(unityRevision);
    out.append('\0');
    const int sizePos = out.size();
    putU64be(out, 0);
    putU32be(out, static_cast<quint32>(infoPacked.size()));
    putU32be(out, static_cast<quint32>(info.size()));
    putU32be(out, outFlags);
    if (formatVersion >= 7) alignOut(out, 16);
    out.append(infoPacked);
    out.append(blockData);
    qToBigEndian(static_cast<quint64>(out.size()), out.data() + sizePos);
    return out;
}

} // namespace bdsp
