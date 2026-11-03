#include "headers/common/edit_model.h"

namespace edit {

Field number(const QString& label, int min, int max, std::function<int()> get, std::function<void(int)> set,
             std::function<int()> original) {
    Field f;
    f.label = label;
    f.kind = Kind::Number;
    f.min = min;
    f.max = max;
    f.get = std::move(get);
    f.set = std::move(set);
    f.original = std::move(original);
    return f;
}

Field check(const QString& label, std::function<int()> get, std::function<void(int)> set, std::function<int()> original) {
    Field f;
    f.label = label;
    f.kind = Kind::Check;
    f.min = 0;
    f.max = 1;
    f.get = std::move(get);
    f.set = std::move(set);
    f.original = std::move(original);
    return f;
}

Field choice(const QString& label, const QStringList& choices, std::function<int()> get, std::function<void(int)> set,
             std::function<int()> original) {
    Field f;
    f.label = label;
    f.kind = Kind::Choice;
    f.choices = choices;
    f.min = 0;
    f.max = static_cast<int>(choices.size()) - 1;
    f.get = std::move(get);
    f.set = std::move(set);
    f.original = std::move(original);
    return f;
}

Field picker(const QString& label, Kind kind, std::function<int()> get, std::function<void(int)> set,
             std::function<int()> original) {
    Field f;
    f.label = label;
    f.kind = kind;
    f.get = std::move(get);
    f.set = std::move(set);
    f.original = std::move(original);
    return f;
}

Field text(const QString& label, std::function<QString()> textFn) {
    Field f;
    f.label = label;
    f.kind = Kind::Text;
    f.text = std::move(textFn);
    return f;
}

const QList<QPair<int, QString>>& Catalog::list(Kind kind) const {
    static const QList<QPair<int, QString>> empty;
    switch (kind) {
    case Kind::Pokemon: return pokemon;
    case Kind::Move: return moves;
    case Kind::Item: return items;
    case Kind::Ability: return abilities;
    case Kind::Type: return types;
    case Kind::Nature: return natures;
    default: return empty;
    }
}

QString Catalog::name(Kind kind, int value) const {
    const int key = static_cast<int>(kind);
    if (!cache.contains(key)) {
        QHash<int, QString>& map = cache[key];
        for (const auto& entry : list(kind)) map.insert(entry.first, entry.second);
    }
    const QHash<int, QString>& map = cache[key];
    auto it = map.constFind(value);
    if (it != map.constEnd()) return it.value();
    switch (kind) {
    case Kind::Pokemon:
        if (value == 0) return "(keins)";
        return QString("#%1 (Form %2)").arg(speciesOf(value)).arg(formOf(value));
    case Kind::Move: return value == 0 ? QString("(keine)") : QString("Attacke %1").arg(value);
    case Kind::Item: return value == 0 ? QString("(kein Item)") : QString("Item %1").arg(value);
    case Kind::Ability: return value == 0 ? QString("(keine)") : QString("Fähigkeit %1").arg(value);
    case Kind::Type: return QString("Typ %1").arg(value);
    case Kind::Nature: return QString("Wesen %1").arg(value);
    default: return QString::number(value);
    }
}

QStringList natureNames() {
    return {"Robust", "Solo", "Mutig", "Hart", "Frech", "Kühn", "Sanft", "Locker", "Pfiffig", "Lasch",
            "Scheu", "Hastig", "Ernst", "Froh", "Naiv", "Mäßig", "Mild", "Ruhig", "Zaghaft", "Hitzig",
            "Still", "Zart", "Forsch", "Sacht", "Kauzig"};
}

QString valueText(const Catalog& catalog, const Field& field, int value) {
    switch (field.kind) {
    case Kind::Number: return QString::number(value);
    case Kind::Check: return value ? QString("Ja") : QString("Nein");
    case Kind::Choice: return value >= 0 && value < field.choices.size() ? field.choices[value] : QString::number(value);
    case Kind::Text: return field.text ? field.text() : QString();
    default: return catalog.name(field.kind, value);
    }
}

} // namespace edit
