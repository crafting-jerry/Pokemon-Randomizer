#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QMap>
#include "AlternateWindow.h"

class GameSelectPage;

// ---------------------------------------------------------------------------
// Hauptfenster: zeigt zuerst die Spielauswahl, danach den Randomizer des
// gewaehlten Spiels. "Spielauswahl" in der Seitenleiste fuehrt zurueck.
// ---------------------------------------------------------------------------
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    void alwaysOnTop(bool always);

private slots:
    void openGame(int id);
    void showGameSelect();
    void checkForUpdates();

private:
    QStackedWidget *stackedWidget;
    GameSelectPage *gameSelect;
    QMap<int, AlternateWindow *> gameWindows; // werden beim ersten Oeffnen erstellt
};

#endif // MAINWINDOW_H
