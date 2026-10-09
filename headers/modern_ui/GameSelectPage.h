#ifndef GAMESELECTPAGE_H
#define GAMESELECTPAGE_H

// ---------------------------------------------------------------------------
// GameSelectPage
// Startbildschirm: Liste aller Spiele mit kleinem Vorschaubild. Verfuegbare
// Spiele lassen sich oeffnen, kommende sind als "Bald verfuegbar" markiert.
// Neue Spiele werden in GameSelectPage::games() eingetragen.
// ---------------------------------------------------------------------------

#include <QWidget>
#include <QCheckBox>
#include <QList>
#include <QString>

class GameSelectPage : public QWidget {
    Q_OBJECT
public:
    struct GameInfo {
        int id;              // wird an gameChosen() uebergeben
        QString title;
        QString subtitle;
        QString features;    // was randomisiert werden kann
        QString cover;       // Pfad unter assets/
        bool available;
        bool preview = false; // oeffnebar, aber noch im Aufbau
    };

    explicit GameSelectPage(QWidget* parent = nullptr);
    static QList<GameInfo> games();

signals:
    void gameChosen(int id);
    void alwaysOnTopChanged(bool on);
    void updateCheckRequested();

private:
    QWidget* buildGameCard(const GameInfo& game);
};

#endif // GAMESELECTPAGE_H
