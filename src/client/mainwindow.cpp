#include "mainwindow.hpp"
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QStandardPaths>

namespace torrent_p2p {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Initialize client with default port
    client = std::make_unique<Client>(6881);
    
    // Setup UI
    setupUI();
    createActions();
    createMenus();
    
    // Initialize torrent model
    torrentModel = new TorrentListModel(client.get(), this);
    torrentTableView->setModel(torrentModel);
    
    // Set up refresh timer
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::refreshStatus);
    refreshTimer->start(1000); // Update every second
    
    // Update DHT stats every 10 seconds
    QTimer *dhtTimer = new QTimer(this);
    connect(dhtTimer, &QTimer::timeout, this, &MainWindow::updateDHTStats);
    dhtTimer->start(10000);
    
    // Initial update
    refreshStatus();
    updateDHTStats();
    
    // Set window title and size
    setWindowTitle("P2P Torrenting Client");
    resize(800, 600);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    // Create central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Create tab widget
    tabWidget = new QTabWidget(centralWidget);
    
    // Create torrents tab
    QWidget *torrentsTab = new QWidget();
    QVBoxLayout *torrentsLayout = new QVBoxLayout(torrentsTab);
    
    // Create torrent table
    torrentTableView = new QTableView(torrentsTab);
    torrentTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    torrentTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    torrentTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    torrentTableView->horizontalHeader()->setStretchLastSection(true);
    torrentTableView->verticalHeader()->setVisible(false);
    
    // Create button layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    // Add torrent button
    addTorrentButton = new QPushButton("Add Torrent", torrentsTab);
    connect(addTorrentButton, &QPushButton::clicked, this, &MainWindow::addTorrent);
    buttonLayout->addWidget(addTorrentButton);
    
    // Generate torrent button
    generateTorrentButton = new QPushButton("Generate Torrent", torrentsTab);
    connect(generateTorrentButton, &QPushButton::clicked, this, &MainWindow::generateTorrent);
    buttonLayout->addWidget(generateTorrentButton);
    
    // Search layout
    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLineEdit = new QLineEdit(torrentsTab);
    searchLineEdit->setPlaceholderText("Enter info hash to search");
    searchButton = new QPushButton("Search DHT", torrentsTab);
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::searchDHT);
    searchLayout->addWidget(searchLineEdit);
    searchLayout->addWidget(searchButton);
    
    // Add layouts to torrents tab
    torrentsLayout->addWidget(torrentTableView);
    torrentsLayout->addLayout(buttonLayout);
    torrentsLayout->addLayout(searchLayout);
    
    // Create DHT tab
    QWidget *dhtTab = new QWidget();
    QVBoxLayout *dhtLayout = new QVBoxLayout(dhtTab);
    
    // DHT stats label
    dhtStatsLabel = new QLabel(dhtTab);
    dhtStatsLabel->setTextFormat(Qt::RichText);
    dhtStatsLabel->setWordWrap(true);
    dhtLayout->addWidget(dhtStatsLabel);
    
    // DHT buttons layout
    QHBoxLayout *dhtButtonLayout = new QHBoxLayout();
    
    // Save DHT state button
    QPushButton *saveDHTButton = new QPushButton("Save DHT State", dhtTab);
    connect(saveDHTButton, &QPushButton::clicked, this, &MainWindow::saveDHTState);
    dhtButtonLayout->addWidget(saveDHTButton);
    
    // Load DHT state button
    QPushButton *loadDHTButton = new QPushButton("Load DHT State", dhtTab);
    connect(loadDHTButton, &QPushButton::clicked, this, &MainWindow::loadDHTState);
    dhtButtonLayout->addWidget(loadDHTButton);
    
    dhtLayout->addLayout(dhtButtonLayout);
    
    // Create log tab
    QWidget *logTab = new QWidget();
    QVBoxLayout *logLayout = new QVBoxLayout(logTab);
    
    // Log text edit
    logTextEdit = new QTextEdit(logTab);
    logTextEdit->setReadOnly(true);
    logLayout->addWidget(logTextEdit);
    
    // Add tabs to tab widget
    tabWidget->addTab(torrentsTab, "Torrents");
    tabWidget->addTab(dhtTab, "DHT");
    tabWidget->addTab(logTab, "Log");
    
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addWidget(tabWidget);
    
    // Status bar
    statusBar()->showMessage("Ready");
}

void MainWindow::createActions()
{
    // Add actions here if needed
}

void MainWindow::createMenus()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    
    // Add torrent action
    QAction *addTorrentAction = new QAction("&Add Torrent", this);
    connect(addTorrentAction, &QAction::triggered, this, &MainWindow::addTorrent);
    fileMenu->addAction(addTorrentAction);
    
    // Generate torrent action
    QAction *generateTorrentAction = new QAction("&Generate Torrent", this);
    connect(generateTorrentAction, &QAction::triggered, this, &MainWindow::generateTorrent);
    fileMenu->addAction(generateTorrentAction);
    
    fileMenu->addSeparator();
    
    // Exit action
    QAction *exitAction = new QAction("E&xit", this);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(exitAction);
    
    // DHT menu
    QMenu *dhtMenu = menuBar()->addMenu("&DHT");
    
    // Save DHT state action
    QAction *saveDHTAction = new QAction("&Save DHT State", this);
    connect(saveDHTAction, &QAction::triggered, this, &MainWindow::saveDHTState);
    dhtMenu->addAction(saveDHTAction);
    
    // Load DHT state action
    QAction *loadDHTAction = new QAction("&Load DHT State", this);
    connect(loadDHTAction, &QAction::triggered, this, &MainWindow::loadDHTState);
    dhtMenu->addAction(loadDHTAction);
}

void MainWindow::addTorrent()
{
    QString torrentFile = QFileDialog::getOpenFileName(this, 
        "Open Torrent File", QDir::homePath(), "Torrent Files (*.torrent)");
    
    if (torrentFile.isEmpty())
        return;
    
    QString savePath = QFileDialog::getExistingDirectory(this, 
        "Select Save Directory", QDir::homePath());
    
    if (savePath.isEmpty())
        return;
    
    try {
        client->addTorrent(torrentFile.toStdString());
        logTextEdit->append("Added torrent: " + torrentFile);
        refreshStatus();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", 
            QString("Failed to add torrent: %1").arg(e.what()));
        logTextEdit->append("Error adding torrent: " + QString(e.what()));
    }
}

void MainWindow::generateTorrent()
{
    QString filePath = QFileDialog::getOpenFileName(this, 
        "Select File or Directory", QDir::homePath());
    
    if (filePath.isEmpty())
        return;
    
    try {
        client->generateTorrentFile(filePath.toStdString());
        logTextEdit->append("Generated torrent for: " + filePath);
        QMessageBox::information(this, "Success", 
            "Torrent file generated successfully.");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", 
            QString("Failed to generate torrent: %1").arg(e.what()));
        logTextEdit->append("Error generating torrent: " + QString(e.what()));
    }
}

void MainWindow::searchDHT()
{
    QString infoHash = searchLineEdit->text().trimmed();
    
    if (infoHash.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please enter an info hash to search.");
        return;
    }
    
    try {
        client->searchDHT(infoHash.toStdString());
        logTextEdit->append("Searching DHT for info hash: " + infoHash);
        statusBar()->showMessage("DHT search initiated...");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", 
            QString("Failed to search DHT: %1").arg(e.what()));
        logTextEdit->append("Error searching DHT: " + QString(e.what()));
    }
}

void MainWindow::refreshStatus()
{
    // Update torrent model
    torrentModel->refresh();
}

void MainWindow::updateDHTStats()
{
    QString stats = QString::fromStdString(client->getDHTStats());
    dhtStatsLabel->setText("<pre>" + stats + "</pre>");
}

void MainWindow::saveDHTState()
{
    QString saveFile = QFileDialog::getSaveFileName(this, 
        "Save DHT State", QDir::homePath(), "DHT State Files (*.dat)");
    
    if (saveFile.isEmpty())
        return;
    
    try {
        if (client->saveDHTState()) {
            logTextEdit->append("DHT state saved to: " + saveFile);
            statusBar()->showMessage("DHT state saved successfully.");
        } else {
            QMessageBox::warning(this, "Warning", "Failed to save DHT state.");
            logTextEdit->append("Failed to save DHT state.");
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", 
            QString("Failed to save DHT state: %1").arg(e.what()));
        logTextEdit->append("Error saving DHT state: " + QString(e.what()));
    }
}

void MainWindow::loadDHTState()
{
    QString loadFile = QFileDialog::getOpenFileName(this, 
        "Load DHT State", QDir::homePath(), "DHT State Files (*.dat)");
    
    if (loadFile.isEmpty())
        return;
    
    try {
        if (client->loadDHTState(loadFile.toStdString())) {
            logTextEdit->append("DHT state loaded from: " + loadFile);
            statusBar()->showMessage("DHT state loaded successfully.");
            updateDHTStats();
        } else {
            QMessageBox::warning(this, "Warning", "Failed to load DHT state.");
            logTextEdit->append("Failed to load DHT state.");
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Error", 
            QString("Failed to load DHT state: %1").arg(e.what()));
        logTextEdit->append("Error loading DHT state: " + QString(e.what()));
    }
}

} // namespace torrent_p2p