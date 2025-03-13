#include <drogon/drogon.h>
#include "client/client.hpp"

using namespace drogon;
using namespace torrent_p2p;

// Create a globally accessible Client instance (thread-safe)
std::unique_ptr<Client> client;

int main() {
    client = std::make_unique<Client>(6882, "shanaya", "None");

    // Add a torrent file
    app().registerHandler("/add-torrent",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr &)> &&callback) {
            auto json = req->getJsonObject();
            if (!json || !json->isMember("file")) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(HttpStatusCode::k400BadRequest);
                resp->setBody("Missing 'file' parameter");
                callback(resp);
                return;
            }

            std::string file_path = (*json)["file"].asString();
            client->addTorrent(file_path);

            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->setBody("Torrent added successfully");
            callback(resp);
        },
        {Post}
    );

    // Generate a torrent file from a file/directory
    app().registerHandler("/generate-torrent",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr &)> &&callback) {
            auto json = req->getJsonObject();
            if (!json || !json->isMember("path")) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(HttpStatusCode::k400BadRequest);
                resp->setBody("Missing 'path' parameter");
                callback(resp);
                return;
            }

            std::string path = (*json)["path"].asString();
            client->generateTorrentFile(path);

            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->setBody("Torrent file generated");
            callback(resp);
        },
        {Post}
    );

    // Get the status of all torrents
    app().registerHandler("/status",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr &)> &&callback) {
            client->printStatus();
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->setBody("Status printed to console");
            callback(resp);
        },
        {Get}
    );

    // Search the DHT network for a specific info hash
    app().registerHandler("/search/{info_hash}",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr &)> &&callback, std::string info_hash) {
            client->searchDHT(info_hash);
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->setBody("DHT search initiated");
            callback(resp);
        },
        {Get}
    );

    // Get DHT statistics
    app().registerHandler("/dht-stats",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr &)> &&callback) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(HttpStatusCode::k200OK);
            resp->setBody(client->getDHTStats());
            callback(resp);
        },
        {Get}
    );

    // Start the server on port 8080
    app().addListener("0.0.0.0", 8080).run();
}
