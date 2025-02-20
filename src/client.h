#ifndef CLIENT_H
#define CLIENT_H

#include <string>

class Client {
public:
    Client();
    ~Client();

    void addTorrent(const std::string& torrentFilePath, const std::string& savePath);
    void start();
    void stop();
    double getProgress(const std::string& torrentFilePath);
private:
};

#endif