#pragma once

#include <QDialog>
#include <QWebEnginePermission>

class QTreeWidget;
class QWebEngineProfile;

namespace spacepenguin {

namespace SitePermissions {

/** What the site asks to do, to finish "example.com wants to …". */
QString request(QWebEnginePermission::PermissionType type);
/** A short name for lists: "Camera", "Location". */
QString name(QWebEnginePermission::PermissionType type);
QString stateName(QWebEnginePermission::State state);
/** The site as people know it: its host, or its whole origin when that is not https. */
QString siteName(const QUrl &origin);

}

/** Settings › Site permissions: what each site was allowed or refused, with Reset. */
class SitePermissionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SitePermissionsDialog(QWebEngineProfile *profile, QWidget *parent = nullptr);

private:
    void rebuild();

    QWebEngineProfile *m_profile;
    QTreeWidget *m_list = nullptr;
};

}
