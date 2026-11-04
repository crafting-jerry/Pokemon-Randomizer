#ifndef ALTERNATEWINDOW_H
#define ALTERNATEWINDOW_H

#include <QWidget>
#include <QStackedWidget>

// Rahmen um das Fenster eines Spiels (0 = Karmesin/Purpur, 1 = Schwert/Schild, 2 = Diamant/Perle)
class AlternateWindow : public QWidget {
    Q_OBJECT

public:
    AlternateWindow(int id, QWidget *parent = nullptr);
signals:
    void backRequested();
private:
    int windowId;
    QStackedWidget *stackedWidget;
};

#endif // ALTERNATEWINDOW_H
