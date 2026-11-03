#ifndef COMMON_EDIT_MODEL_H
#define COMMON_EDIT_MODEL_H

// ---------------------------------------------------------------------------
// Spielneutrales Modell fuer den Bearbeiten-Modus.
//
// Nach dem Randomisieren (oder mit "Original bearbeiten") beschreibt jedes Spiel
// seine Daten als Bereiche (Section = Reiter in der Seitenleiste) mit Eintraegen
// (Entry = z. B. ein Trainer oder ein Gebiet). Ein Eintrag besteht aus Feldern
// (Field) und Tabellen (Table). Felder lesen und schreiben ueber Funktionen direkt
// in den Spieldaten im Speicher; die Oberflaeche (modern_ui/edit_pages) kennt
// dadurch kein einziges Spiel. Exportiert wird erst, wenn der Nutzer es will.
// ---------------------------------------------------------------------------

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <functional>

namespace edit {

enum class Kind {
    Number,   // Spinbox (min/max)
    Check,    // Haken (0/1)
    Choice,   // Auswahl aus choices (Wert = Index)
    Pokemon,  // Art + Form (Wert = pokemonValue)
    Move,
    Item,
    Ability,
    Type,
    Nature,
    Text      // nur Anzeige (text)
};

// Art und Form in einem Wert
inline int pokemonValue(int species, int form) { return (species & 0xFFFF) | (form << 16); }
inline int speciesOf(int value) { return value & 0xFFFF; }
inline int formOf(int value) { return (value >> 16) & 0xFFFF; }

struct Field {
    QString label;
    Kind kind = Kind::Number;
    std::function<int()> get;
    std::function<void(int)> set;      // leer = nur Anzeige
    std::function<int()> original;     // leer = kein Vorher-Wert
    int min = 0;
    int max = 65535;
    QStringList choices;               // Kind::Choice
    QString info;                      // Erklaerung (Tooltip)
    std::function<QString()> text;     // Kind::Text
    int span = 1;                      // Breite im Raster (Spalten)
    bool rebuild = false;              // nach dem Aendern den Eintrag neu aufbauen (z. B. Art -> Faehigkeiten)

    bool changed() const { return original && get && original() != get(); }
};

// Bequeme Erzeuger
Field number(const QString& label, int min, int max, std::function<int()> get, std::function<void(int)> set,
             std::function<int()> original = {});
Field check(const QString& label, std::function<int()> get, std::function<void(int)> set,
            std::function<int()> original = {});
Field choice(const QString& label, const QStringList& choices, std::function<int()> get, std::function<void(int)> set,
             std::function<int()> original = {});
Field picker(const QString& label, Kind kind, std::function<int()> get, std::function<void(int)> set,
             std::function<int()> original = {});
Field text(const QString& label, std::function<QString()> text);

// Tabelle: jede Zeile hat dieselben Spalten. cards = jede Zeile als eigene Karte mit Raster
// (fuer Zeilen mit vielen Feldern wie Trainer-Pokemon).
struct Table {
    QString title;
    QStringList columns;
    std::function<int()> rowCount;
    std::function<QList<Field>(int)> row;
    std::function<QString(int)> rowTitle;   // nur bei cards (z. B. "Pokémon 1")
    std::function<bool()> canAdd;           // leer = keine Zeilen hinzufuegen
    std::function<void()> add;
    std::function<bool()> canRemove;
    std::function<void(int)> remove;
    bool cards = false;
    int columnsPerLine = 4;                 // nur bei cards
};

struct Content {
    QString hint;            // grauer Hinweis oben
    QList<Field> fields;     // Kopf des Eintrags
    int fieldsPerLine = 3;
    QList<Table> tables;
};

struct Entry {
    QString group;                       // Ueberschrift in der Liste ("Arenaleiter", "Route 1" ...)
    QString title;
    std::function<QString()> search;     // weiterer Suchtext (aktuelle Pokemon ...)
    std::function<bool()> changed;       // weicht vom Original ab
    std::function<Content()> build;
};

struct Section {
    QString title;           // Reiter in der Seitenleiste
    QString subtitle;
    QList<Entry> entries;
};

// Namen fuer die Auswahllisten. Reihenfolge = Anzeige.
struct Catalog {
    QList<QPair<int, QString>> pokemon;
    QList<QPair<int, QString>> moves;
    QList<QPair<int, QString>> items;
    QList<QPair<int, QString>> abilities;
    QList<QPair<int, QString>> types;
    QList<QPair<int, QString>> natures;

    const QList<QPair<int, QString>>& list(Kind kind) const;
    QString name(Kind kind, int value) const;   // auch fuer Werte, die nicht in der Liste stehen

private:
    mutable QHash<int, QHash<int, QString>> cache;
};

// Wesen auf Deutsch (Index 0-24 wie im Spiel)
QStringList natureNames();

// Text eines Feldes (fuer Suche, Vorher-Anzeige und Log)
QString valueText(const Catalog& catalog, const Field& field, int value);

} // namespace edit

#endif // COMMON_EDIT_MODEL_H
