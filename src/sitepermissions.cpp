#include "sitepermissions.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWebEngineProfile>

namespace spacepenguin {

using Type = QWebEnginePermission::PermissionType;

QString SitePermissions::request(Type type)
{
    switch (type) {
    case Type::MediaAudioCapture: return QObject::tr("use your microphone");
    case Type::MediaVideoCapture: return QObject::tr("use your camera");
    case Type::MediaAudioVideoCapture: return QObject::tr("use your camera and microphone");
    case Type::DesktopVideoCapture: return QObject::tr("see your screen");
    case Type::DesktopAudioVideoCapture: return QObject::tr("see your screen and hear its sound");
    case Type::MouseLock: return QObject::tr("hide and lock your mouse pointer");
    case Type::Notifications: return QObject::tr("show notifications");
    case Type::Geolocation: return QObject::tr("know your location");
    case Type::ClipboardReadWrite: return QObject::tr("see and change what you copy");
    case Type::LocalFontsAccess: return QObject::tr("see the fonts on your computer");
    case Type::Unsupported: break;
    }
    return QObject::tr("use a feature of your computer");
}

QString SitePermissions::name(Type type)
{
    switch (type) {
    case Type::MediaAudioCapture: return QObject::tr("Microphone");
    case Type::MediaVideoCapture: return QObject::tr("Camera");
    case Type::MediaAudioVideoCapture: return QObject::tr("Camera and microphone");
    case Type::DesktopVideoCapture: return QObject::tr("Screen");
    case Type::DesktopAudioVideoCapture: return QObject::tr("Screen and sound");
    case Type::MouseLock: return QObject::tr("Mouse pointer");
    case Type::Notifications: return QObject::tr("Notifications");
    case Type::Geolocation: return QObject::tr("Location");
    case Type::ClipboardReadWrite: return QObject::tr("Clipboard");
    case Type::LocalFontsAccess: return QObject::tr("Fonts");
    case Type::Unsupported: break;
    }
    return QObject::tr("Other");
}

QString SitePermissions::stateName(QWebEnginePermission::State state)
{
    switch (state) {
    case QWebEnginePermission::State::Granted: return QObject::tr("Allowed");
    case QWebEnginePermission::State::Denied: return QObject::tr("Blocked");
    case QWebEnginePermission::State::Ask: return QObject::tr("Ask");
    case QWebEnginePermission::State::Invalid: break;
    }
    return QString();
}

QString SitePermissions::siteName(const QUrl &origin)
{
    if (origin.host().isEmpty())
        return origin.toString();
    return origin.port() == -1 ? origin.host()
                               : QStringLiteral("%1:%2").arg(origin.host()).arg(origin.port());
}

SitePermissionsDialog::SitePermissionsDialog(QWebEngineProfile *profile, QWidget *parent)
    : QDialog(parent)
    , m_profile(profile)
{
    setWindowTitle(tr("Site Permissions"));
    resize(560, 420);

    m_list = new QTreeWidget;
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({tr("Site"), tr("Permission"), tr("Setting")});
    m_list->setRootIsDecorated(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    auto *reset = new QPushButton(tr("Reset"));
    reset->setToolTip(tr("The site asks again next time"));
    reset->setEnabled(false);
    auto *resetAll = new QPushButton(tr("Reset all"));
    connect(m_list, &QTreeWidget::itemSelectionChanged, this,
            [this, reset] { reset->setEnabled(!m_list->selectedItems().isEmpty()); });
    connect(reset, &QPushButton::clicked, this, [this] {
        for (QTreeWidgetItem *item : m_list->selectedItems()) {
            m_profile->queryPermission(item->data(0, Qt::UserRole).toUrl(),
                                       Type(item->data(1, Qt::UserRole).toInt()))
                .reset();
        }
        rebuild();
    });
    connect(resetAll, &QPushButton::clicked, this, [this] {
        for (const QWebEnginePermission &permission : m_profile->listAllPermissions())
            permission.reset();
        rebuild();
    });

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(reset);
    buttons->addWidget(resetAll);
    buttons->addStretch(1);

    auto *note = new QLabel(tr("Sites ask before using your camera, microphone, location or "
                               "notifications. Your answers are listed here."));
    note->setWordWrap(true);
    note->setProperty("muted", true);

    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(note);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);
    layout->addWidget(close);
    rebuild();
}

void SitePermissionsDialog::rebuild()
{
    m_list->clear();
    for (const QWebEnginePermission &permission : m_profile->listAllPermissions()) {
        if (permission.state() == QWebEnginePermission::State::Ask)
            continue;
        auto *item = new QTreeWidgetItem(m_list, {SitePermissions::siteName(permission.origin()),
                                                  SitePermissions::name(permission.permissionType()),
                                                  SitePermissions::stateName(permission.state())});
        item->setData(0, Qt::UserRole, permission.origin());
        item->setData(1, Qt::UserRole, int(permission.permissionType()));
    }
    m_list->sortItems(0, Qt::AscendingOrder);
    if (m_list->topLevelItemCount() == 0) {
        auto *empty = new QTreeWidgetItem(m_list, {tr("No site has asked for anything yet.")});
        empty->setFlags(Qt::NoItemFlags);
        empty->setFirstColumnSpanned(true);
    }
}

}
