#ifndef TORRENTLISTMODEL_HPP
#define TORRENTLISTMODEL_HPP

#include <QAbstractTableModel>
#include <vector>
#include <string>
#include "client.hpp"

namespace torrent_p2p {

struct TorrentInfo {
    std::string name;
    std::string hash;
    std::string status;
    double progress;
    int64_t total_size;
    int64_t downloaded;
    int download_rate;
    int upload_rate;
    int num_peers;
    int num_seeds;
};

class TorrentListModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit TorrentListModel(Client* client, QObject *parent = nullptr);
    
    // QAbstractTableModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    // Refresh the model data
    void refresh();

private:
    Client* client_;
    std::vector<TorrentInfo> torrents_;
    
    // Column indices
    enum Column {
        Name,
        Status,
        Progress,
        Size,
        Downloaded,
        DownloadRate,
        UploadRate,
        Peers,
        Seeds,
        ColumnCount
    };
    
    // Helper functions
    QString formatSize(int64_t bytes) const;
    QString formatSpeed(int bytes_per_second) const;
};

} // namespace torrent_p2p

#endif // TORRENTLISTMODEL_HPP