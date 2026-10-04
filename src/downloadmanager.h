#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

class QWebEngineDownloadRequest;
class QWebEngineProfile;

namespace spacepenguin {

struct DownloadItem {
    QString id;
    QString url;
    QString filePath;
    QString suggestedFileName;
    qint64 totalBytes = 0;
    qint64 receivedBytes = 0;
    enum State { Requested, InProgress, Completed, Cancelled, Interrupted };
    State state = Requested;
    QString interruptReason;
};

class DownloadManager : public QObject
{
    Q_OBJECT

public:
    explicit DownloadManager(QWebEngineProfile *profile, QObject *parent = nullptr);
    ~DownloadManager() override;

    QList<DownloadItem> downloads() const { return m_downloads; }

    void setDownloadPath(const QString &path);

signals:
    void downloadStarted(const DownloadItem &item);
    void downloadProgress(const DownloadItem &item);
    void downloadFinished(const DownloadItem &item);
    void downloadRemoved(const QString &id);

public slots:
    void cancelDownload(const QString &id);
    void removeDownload(const QString &id);
    void clearFinished();

private:
    void onDownloadRequested(QWebEngineDownloadRequest *download);

    QWebEngineProfile *m_profile = nullptr;
    QList<DownloadItem> m_downloads;
    QMap<QString, QWebEngineDownloadRequest *> m_activeDownloads;
    QString m_downloadPath;
};

}