#include "headers/qtwindows_headers/mainwindow.h"
#include "headers/modern_ui/GameSelectPage.h"

#include <QApplication>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QSettings>
#include <QUrl>

namespace {
const QString kCurrentVersion = "0.2.0";
const QString kRepoOwner = "crafting-jerry";
const QString kRepoName = "Pokemon-Randomizer";
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    resize(1180, 840);
    setMinimumSize(900, 600);

    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    gameSelect = new GameSelectPage(stackedWidget);
    stackedWidget->addWidget(gameSelect);

    connect(gameSelect, &GameSelectPage::gameChosen, this, &MainWindow::openGame);
    connect(gameSelect, &GameSelectPage::updateCheckRequested, this, &MainWindow::checkForUpdates);
    connect(gameSelect, &GameSelectPage::alwaysOnTopChanged, this, [this](bool checked) {
        QSettings settings("Pokemon Randomizer", "Main");
        settings.setValue("AlwaysOnTop", checked);
        setWindowFlag(Qt::WindowStaysOnTopHint, checked);
        show(); // Fensterflags neu anwenden
    });
}

void MainWindow::alwaysOnTop(bool always) {
    setWindowFlag(Qt::WindowStaysOnTopHint, always);
    show();
    raise();
    activateWindow();
}

void MainWindow::openGame(int id) {
    if (!gameWindows.contains(id)) {
        // Erstes Oeffnen: Spieldaten laden (dauert einen Moment)
        QApplication::setOverrideCursor(Qt::WaitCursor);
        AlternateWindow *window = new AlternateWindow(id);
        connect(window, &AlternateWindow::backRequested, this, &MainWindow::showGameSelect);
        stackedWidget->addWidget(window);
        gameWindows[id] = window;
        QApplication::restoreOverrideCursor();
    }
    stackedWidget->setCurrentWidget(gameWindows[id]);
}

void MainWindow::showGameSelect() {
    stackedWidget->setCurrentWidget(gameSelect);
}

void MainWindow::checkForUpdates() {
    QString apiUrl = QString("https://api.github.com/repos/%1/%2/releases/latest").arg(kRepoOwner, kRepoName);
    auto *manager = new QNetworkAccessManager(this);

    connect(manager, &QNetworkAccessManager::finished, this, [this, manager](QNetworkReply *reply) {
        reply->deleteLater();
        manager->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404) {
            QMessageBox::information(this, "Updates", "Es wurde noch keine Version veröffentlicht.\n"
                                                      "Du nutzt Version " + kCurrentVersion + ".");
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, "Updates", "Die Suche nach Updates ist fehlgeschlagen:\n" + reply->errorString());
            return;
        }

        QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        QString latestTag = json.value("tag_name").toString();
        if (latestTag.isEmpty()) {
            QMessageBox::warning(this, "Updates", "Die Antwort von GitHub konnte nicht gelesen werden.");
            return;
        }

        QVersionNumber current = QVersionNumber::fromString(kCurrentVersion);
        QVersionNumber latest = QVersionNumber::fromString(latestTag.startsWith('v') ? latestTag.mid(1) : latestTag);
        if (latest > current) {
            QMessageBox::information(this, "Update verfügbar",
                                     "Version " + latestTag + " ist verfügbar (du nutzt " + kCurrentVersion + ").");
        } else {
            QMessageBox::information(this, "Updates", "Du nutzt die aktuelle Version (" + kCurrentVersion + ").");
        }
    });

    manager->get(QNetworkRequest(QUrl(apiUrl)));
}
