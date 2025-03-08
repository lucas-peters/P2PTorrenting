#include "torrentlistmodel.hpp"
#include <QColor>
#include <QBrush>
#include <QFont>
#include <QProgressBar>

namespace torrent_p2p {

TorrentListModel::TorrentListModel(Client* client, QObject *parent)
    : QAbstractTableModel(parent), client_(client)
{
}

int TorrentListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    
    return torrents_.size();
}

int TorrentListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    
    return Column::ColumnCount;
}

QVariant TorrentListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= torrents_.size())
        return QVariant();
    
    const TorrentInfo &torrent = torrents_[index.row()];
    
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case Column::Name:
                return QString::fromStdString(torrent.name);
            case Column::Status:
                return QString::fromStdString(torrent.status);
            case Column::Progress:
                return QString("%1%").arg(torrent.progress * 100.0, 0, 'f', 1);
            case Column::Size:
                return formatSize(torrent.total_size);
            case Column::Downloaded:
                return formatSize(torrent.downloaded);
            case Column::DownloadRate:
                return formatSpeed(torrent.download_rate);
            case Column::UploadRate:
                return formatSpeed(torrent.upload_rate);
            case Column::Peers:
                return torrent.num_peers;
            case Column::Seeds:
                return torrent.num_seeds;
            default:
                return QVariant();
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == Column::Name)
            return Qt::AlignLeft + Qt::AlignVCenter;
        else
            return Qt::AlignRight + Qt::AlignVCenter;
    } else if (role == Qt::BackgroundRole) {
        if (torrent.status == "Downloading")
            return QBrush(QColor(230, 250, 230));
        else if (torrent.status == "Seeding")
            return QBrush(QColor(230, 230, 250));
        else if (torrent.status == "Error")
            return QBrush(QColor(250, 230, 230));
    }
    
    return QVariant();
}

QVariant TorrentListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case Column::Name:
                return "Name";
            case Column::Status:
                return "Status";
            case Column::Progress:
                return "Progress";
            case Column::Size:
                return "Size";
            case Column::Downloaded:
                return "Downloaded";
            case Column::DownloadRate:
                return "Down Speed";
            case Column::UploadRate:
                return "Up Speed";
            case Column::Peers:
                return "Peers";
            case Column::Seeds:
                return "Seeds";
            default:
                return QVariant();
        }
    }
    
    return QVariant();
}

void TorrentListModel::refresh()
{
    beginResetModel();
    
    // Clear current data
    torrents_.clear();
    
    // Get status from client
    // Note: This is a placeholder - you'll need to implement a method in Client
    // to get the status of all torrents in a format that can be used here
    
    // For now, we'll just simulate some data
    // In a real implementation, you would get this from the client
    
    endResetModel();
}

QString TorrentListModel::formatSize(int64_t bytes) const
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unit]);
}

QString TorrentListModel::formatSpeed(int bytes_per_second) const
{
    if (bytes_per_second < 0)
        return "N/A";
    
    return formatSize(bytes_per_second) + "/s";
}

} // namespace torrent_p2ps