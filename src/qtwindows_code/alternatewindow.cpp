#include "headers/qtwindows_headers/AlternateWindow.h"
#include "headers/sv_randomizer_headers/SVRandomizerWindow.h"
#include "headers/modern_ui/ModernRandomizerWindow.h"
#include "headers/swsh/SwShRandomizerWindow.h"
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
            svrandomizer = new SVRandomizerWindow();
            svrandomizer->createLayout();

            // Neue Oberflaeche. Die klassische Ansicht bleibt unsichtbar als Randomizer-Kern erhalten.
            svrandomizer->setParent(this);
            svrandomizer->hide();
            ModernRandomizerWindow* modern = new ModernRandomizerWindow(svrandomizer);
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
        default:
            label = new QLabel(QString("This is alternate window %1").arg(id + 1), this);
            layout->addWidget(label);
    }

        layout->addWidget(stackedWidget); // Ensure stackedWidget is added to the layout
        setLayout(layout); // Set the layout for AlternateWindow
}
