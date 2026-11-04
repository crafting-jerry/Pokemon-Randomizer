#ifndef SV_SESSION_H
#define SV_SESSION_H

// ---------------------------------------------------------------------------
// Pokemon Karmesin/Purpur: Bearbeiten-Modus.
//
// Eine EditSession haelt alle bearbeitbaren Tabellen (Trainer, Starter und
// Geschenke, statische Begegnungen, wilde Pokemon, Tera-Raids, Items,
// Pokemon-Daten) als JSON im Speicher:
//   original = Spieldaten (die *_clean.json im Ordner SV_FLATBUFFERS)
//   start    = Stand beim Oeffnen (Randomizer-Ergebnis, Mod oder Original)
//   work     = wird bearbeitet
// Die Dateien des Randomizer-Ergebnisses bzw. der geoeffneten Mod (baseFiles)
// werden beim Export unveraendert uebernommen; nur Tabellen, die sich seit dem
// Oeffnen geaendert haben, werden neu geschrieben (flatc). Danach wird wie
// beim Randomisieren die Dateiliste (data.trpfd) gepatcht.
// ---------------------------------------------------------------------------

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <vector>

#include "../common/edit_model.h"
#include "sv_shared_class.h"

class svTrainers;

namespace svedit {

struct DataTable {
    QString key;        // z. B. "trainers", "raid:su1_raid_enemy_03"
    QString title;      // fuer Meldungen
    QString jsonPath;   // Arbeitsdatei relativ zu SV_FLATBUFFERS (wird nach flatc geloescht)
    QString cleanPath;  // Spieldaten relativ zu SV_FLATBUFFERS (leer = keine)
    QString schema;     // relativ zu SV_FLATBUFFERS
    QString outDir;     // Zielordner in romfs (mit / am Ende)
    QString binName;    // Dateiname in outDir
    json original, start, work;
    bool loaded = false;      // Tabelle ist vorhanden
    bool hasOriginal = false; // original stammt aus den Spieldaten (sonst = start)

    QString binPath() const { return outDir + binName; }
};

struct EditSession {
    bool randomized = false;   // false = Original oder geoeffnete Mod
    bool reopened = false;     // schon exportierte Mod wieder geoeffnet
    QMap<QString, QByteArray> baseFiles;   // Pfad in romfs -> Inhalt (ohne arc/data.trpfd)
    std::vector<DataTable> tables;

    // Namen und Zuordnungen (aus SV_FLATBUFFERS)
    json mapping;                          // pokemon_mapping.json
    json names;                            // german_names.json
    json moveList;                         // sorted_move_list.json
    json wazaTable;                        // waza_array_clean.json
    QHash<QString, int> natdexByDev;       // DEV_... -> Nationaldex
    QHash<int, QString> devByNatdex;
    QHash<int, int> natdexByDevid;         // interne Nummer -> Nationaldex
    QHash<int, int> devidByNatdex;
    QHash<QString, int> moveIdByDev;       // WAZA_... -> Nummer
    QHash<int, QString> moveDevById;
    QStringList itemDevs;                  // Index -> ITEMID_...
    QHash<QString, int> itemIndex;

    // Trainer-Beschriftung (Kategorie, Name) wie im Spoiler-Log
    QHash<int, QPair<QString, QString>> trainerLabels;
    QStringList categoryOrder;

    DataTable* table(const QString& key);
    const DataTable* table(const QString& key) const;

    // Umrechnungen
    int natdexOfDev(const QString& dev) const;
    QString devOfNatdex(int natdex) const;
    int natdexOfDevid(int devid) const;
    int devidOfNatdex(int natdex) const;
    int moveId(const QString& dev) const;
    QString moveDev(int id) const;
    int itemId(const QString& dev);            // unbekannte Items werden angehaengt
    int itemIdConst(const QString& dev) const;
    QString itemDev(int index) const;

    QString pokemonName(int natdex, int form = 0) const;
    QString moveName(int id) const;
    QString itemName(const QString& dev) const;
    QString abilityName(int id) const;
    QString typeName(int type) const;

    // Pokemon-Daten (personal) einer Art/Form in work (nullptr = keine)
    QHash<qint64, int> personalIndex;      // (interne Nummer << 16 | Form) -> Index in "entry"
    const json* personal(int natdex, int form) const;
};

// Liest alle Tabellen. baseRomfs: Ordner "romfs" des Randomizer-Ergebnisses bzw. der Mod
// (leer = Original). trainers liefert die Beschriftung der Trainer.
bool openSession(EditSession& session, const QString& baseRomfs, const svTrainers& trainers, QString* error);

QList<edit::Section> buildSections(EditSession& session);
edit::Catalog buildCatalog(const EditSession& session);

bool sessionHasChanges(const EditSession& session);   // seit dem Oeffnen
int changedTrainerCount(const EditSession& session);  // gegenueber dem Original

// Schreibt die Mod nach outFolder (romfs + ggf. arc/data.trpfd) und das Spoiler-Log
// (wenn spoilerPath nicht leer). tools: fuer flatc, Szenen und data.trpfd.
bool exportSession(EditSession& session, SVShared& tools, const QString& outFolder, bool autoPatch,
                   const QString& spoilerPath, const QString& seedText, QString* error);

} // namespace svedit

#endif // SV_SESSION_H
