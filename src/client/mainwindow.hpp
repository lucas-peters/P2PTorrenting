#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QTableView>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QTabWidget>
#include <QTextEdit>

#include "client.hpp"
#include "torrentlistmodel.hpp"

namespace torrent_p2p {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void addTorrent();
    void generateTorrent();
    void searchDHT();
    void refreshStatus();
    void updateDHTStats();
    void saveDHTState();
    void loadDHTState();

private:
    void setupUI();
    void createActions();
    void createMenus();

    // UI components
    QTableView *torrentTableView;
    QPushButton *addTorrentButton;
    QPushButton *generateTorrentButton;
    QPushButton *searchButton;
    QLineEdit *searchLineEdit;
    QLabel *dhtStatsLabel;
    QTextEdit *logTextEdit;
    QTabWidget *tabWidget;
    
    // Data model
    TorrentListModel *torrentModel;
    
    // Client
    std::unique_ptr<Client> client;
    
    // Timer for refreshing status
    QTimer *refreshTimer;
};

} // namespace torrent_p2p

#endif // MAINWINDOW_HPPs