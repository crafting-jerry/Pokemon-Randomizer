#include "headers/qtwindows_headers/AlternateWindow.h"
#include "headers/modern_ui/ModernRandomizerWindow.h"
#include "headers/swsh/SwShRandomizerWindow.h"
#include "headers/bdsp/BdspRandomizerWindow.h"
#include <QApplication>
#include <QScreen>
#include <QStackedWidget>
#include <QScrollArea>

AlternateWindow::AlternateWindow(int id, QWidget *parent) : QWidget(parent), windowId(id) {
    QVBoxLayout *layout = new QVBoxLayout(this); // Main layout for AlternateWindow
    stackedWidget = new QStackedWidget(this);
    stackedWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    switch(id){
        case 0:
        {
            ModernRandomizerWindow* modern = new ModernRandomizerWindow();
            stackedWidget->addWidget(modern);
            connect(modern, &ModernRandomizerWindow::backRequested, this, &AlternateWindow::backRequested);
            layout->setContentsMargins(0, 0, 0, 0);
            break;
        }
        case 1:
        {
            // Schwert/Schild: Daten kommen aus dem Dump, Pfade auf der Start-Seite
            SwShRandomizerWindow* swshWindow = new SwShRandomizerWindow();
            stackedWidget->addWidget(swshWindow);
            connect(swshWindow, &SwShRandomizerWindow::backRequested, this, &AlternateWindow::backRequested);
            layout->setContentsMargins(0, 0, 0, 0);
            break;
        }
        case 2:
        {
            // Strahlender Diamant/Leuchtende Perle: Daten kommen aus dem Dump, Pfade auf der Start-Seite
            BdspRandomizerWindow* bdspWindow = new BdspRandomizerWindow();
            stackedWidget->addWidget(bdspWindow);
            connect(bdspWindow, &BdspRandomizerWindow::backRequested, this, &AlternateWindow::backRequested);
            layout->setContentsMargins(0, 0, 0, 0);
            break;
        }
        default:
            break;
    }

    layout->addWidget(stackedWidget);
}
