#include "downloadmanager.h"

#include <QDir>
#include <QStandardPaths>
#include <QUuid>
#include <QWebEngineDownloadRequest>
#include <QWebEngineProfile>

namespace spacepenguin {

DownloadManager::DownloadManager(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent)
    , m_profile(profile)
{
    if (!m_profile) {
        return;
    }

    const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    setDownloadPath(defaultPath);

    connect(m_profile, &QWebEngineProfile::downloadRequested,
            this, &DownloadManager::onDownloadRequested);
}

DownloadManager::~DownloadManager() = default;

void DownloadManager::setDownloadPath(const QString &path)
{
    m_downloadPath = path;
    QDir().mkpath(m_downloadPath);
}

void DownloadManager::onDownloadRequested(QWebEngineDownloadRequest *download)
{
    if (!download) {
        return;
    }

    // Accept first: QtWebEngine settles the final, de-duplicated file name only once
    // the directory is set, so the recorded path matches what lands on disk.
    download->setDownloadDirectory(m_downloadPath);
    download->accept();

    DownloadItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.url = download->url().toString();
    item.suggestedFileName = download->downloadFileName();
    item.filePath = QDir(download->downloadDirectory()).filePath(download->downloadFileName());
    item.totalBytes = download->totalBytes();
    item.state = DownloadItem::InProgress;

    m_downloads.prepend(item);
    m_activeDownloads.insert(item.id, download);
    emit downloadStarted(item);

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged,
            this, [this, id = item.id, download] {
        for (auto &item : m_downloads) {
            if (item.id == id) {
                item.receivedBytes = download->receivedBytes();
                emit downloadProgress(item);
                break;
            }
        }
    });

    connect(download, &QWebEngineDownloadRequest::totalBytesChanged,
            this, [this, id = item.id, download] {
        for (auto &item : m_downloads) {
            if (item.id == id) {
                item.totalBytes = download->totalBytes();
                emit downloadProgress(item);
                break;
            }
        }
    });

    connect(download, &QWebEngineDownloadRequest::stateChanged,
            this, [this, id = item.id, download](QWebEngineDownloadRequest::DownloadState state) {
        for (auto &item : m_downloads) {
            if (item.id == id) {
                switch (state) {
                case QWebEngineDownloadRequest::DownloadCompleted:
                    item.state = DownloadItem::Completed;
                    emit downloadFinished(item);
                    break;
                case QWebEngineDownloadRequest::DownloadCancelled:
                    item.state = DownloadItem::Cancelled;
                    emit downloadFinished(item);
                    break;
                case QWebEngineDownloadRequest::DownloadInterrupted:
                    item.state = DownloadItem::Interrupted;
                    item.interruptReason = download->interruptReasonString();
                    emit downloadFinished(item);
                    break;
                default:
                    // Still running: keep it cancellable.
                    return;
                }
                break;
            }
        }
        m_activeDownloads.remove(id);
    });
}

void DownloadManager::cancelDownload(const QString &id)
{
    // Take it out first: cancel() can emit stateChanged synchronously, and that
    // handler also edits m_activeDownloads.
    if (QWebEngineDownloadRequest *download = m_activeDownloads.take(id))
        download->cancel();

    for (auto &item : m_downloads) {
        if (item.id == id && item.state == DownloadItem::InProgress) {
            item.state = DownloadItem::Cancelled;
            emit downloadFinished(item);
            break;
        }
    }
}

void DownloadManager::removeDownload(const QString &id)
{
    cancelDownload(id);

    for (int i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i].id == id) {
            m_downloads.removeAt(i);
            emit downloadRemoved(id);
            break;
        }
    }
}

void DownloadManager::clearFinished()
{
    for (int i = m_downloads.size() - 1; i >= 0; --i) {
        if (m_downloads[i].state == DownloadItem::Completed ||
            m_downloads[i].state == DownloadItem::Cancelled ||
            m_downloads[i].state == DownloadItem::Interrupted) {
            const QString id = m_downloads[i].id;
            m_downloads.removeAt(i);
            emit downloadRemoved(id);
        }
    }
}

}