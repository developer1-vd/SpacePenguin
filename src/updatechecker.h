#pragma once

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;

namespace spacepenguin {

/**
 * "A new version is out", for those who ask for it (Settings › General; off by
 * default, as SpacePenguin otherwise contacts no one unprompted). At most once a
 * day it reads the newest release on GitHub; it never downloads or installs
 * anything itself.
 */
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    static UpdateChecker *instance();

    /** Checks if enabled and not checked in the last day. */
    void checkIfDue();

    /** Whether @p latest (a tag such as "v0.2.0") is newer than @p current. */
    static bool isNewer(const QString &latest, const QString &current);

signals:
    void updateAvailable(const QString &version, const QUrl &page);

private:
    explicit UpdateChecker(QObject *parent = nullptr);

    QNetworkAccessManager *m_network = nullptr;
};

}
