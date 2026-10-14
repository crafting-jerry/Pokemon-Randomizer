#ifndef BDSP_ITEMS_H
#define BDSP_ITEMS_H

// ---------------------------------------------------------------------------
// Pokemon Strahlender Diamant / Leuchtende Perle: Items und Laeden
//
//  - Items auf dem Boden: Skripte ev_fld_item_N (Befehl _FLD_ITEM_EVENT)
//  - versteckte Items: Skripte ev_hide_item_N (_LDVAL in Arbeitswert 234/235)
//  - Laeden: ShopTable (masterdatas), Pokemon-Center-Laeden und feste Laeden
//  - Trainer-Items: Trainer-Pokemon mit Item bekommen ein zufaelliges Kampf-Item
// TMs und Basis-Items bleiben immer erhalten.
// ---------------------------------------------------------------------------

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "bdsp_files.h"
#include "bdsp_trainers.h"

namespace bdsp {

struct ItemSettings {
    bool fieldItems = false;   // Items auf dem Boden (Pokeball-Symbole)
    bool hiddenItems = false;  // versteckte Items (Itemradar/Glitzern)
    int mode = 0;              // 0 Mischen (Original-Items neu verteilen), 1 komplett zufaellig
    bool shops = false;
    bool trainerItems = false; // Trainer-Pokemon mit Item bekommen ein zufaelliges Kampf-Item
    bool anyEnabled() const { return fieldItems || hiddenItems || shops || trainerItems; }
};
QJsonObject itemSettingsToJson(const ItemSettings& s);
void itemSettingsFromJson(const QJsonObject& json, ItemSettings& s);

struct ItemResult {
    int fieldItems = 0, hiddenItems = 0, shopItems = 0, trainerItems = 0;
    QHash<int, int> placedItems; // Item -> Anzahl (Boden + versteckt), fuers Spoiler-Log
    struct ShopLog {
        QString name;
        QList<int> items;        // neues Sortiment
    };
    QList<ShopLog> shops;
};

// Veraendert scripts (Boden/versteckt), files.masterBundle (ShopTable) und die Trainer
bool randomizeItems(const QString& romfs, GameFiles& files, ScriptData& scripts, const ItemSettings& settings,
                    quint64 seed, ItemResult& result, QString* error);

QList<SpoilerSection> itemSpoiler(const ItemResult& result, const GameTexts& texts);

} // namespace bdsp

#endif // BDSP_ITEMS_H
