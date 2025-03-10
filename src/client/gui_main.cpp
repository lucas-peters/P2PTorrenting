#include "mainwindow.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("P2P Torrenting Client");
    app.setOrganizationName("P2PTorrenting");
    
    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("P2P Torrenting Client GUI");
    parser.addHelpOption();
    
    QCommandLineOption portOption(QStringList() << "p" << "port", 
        "Specify port number (default: 6881)", "port", "6881");
    parser.addOption(portOption);
    
    QCommandLineOption stateOption(QStringList() << "s" << "state", 
        "Load DHT state from file", "file");
    parser.addOption(stateOption);
    
    parser.process(app);
    
    // Create and show main window
    torrent_p2p::MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}