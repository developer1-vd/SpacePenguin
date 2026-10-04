#include "datasavermanager.h"

#include <QCoreApplication>
#include <QSettings>

namespace spacepenguin {

DataSaverManager::DataSaverManager(QObject *parent)
    : QObject(parent)
{
    const QString key = QSettings().value("datasaver/mode", "off").toString();
    if (key == "standard") {
        m_mode = Mode::Standard;
    } else if (key == "aggressive") {
        m_mode = Mode::Aggressive;
    } else {
        m_mode = Mode::Off;
    }
}

DataSaverManager::~DataSaverManager() = default;

void DataSaverManager::setMode(Mode mode)
{
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;

    QSettings settings;
    switch (mode) {
    case Mode::Off:
        settings.setValue("datasaver/mode", "off");
        break;
    case Mode::Standard:
        settings.setValue("datasaver/mode", "standard");
        break;
    case Mode::Aggressive:
        settings.setValue("datasaver/mode", "aggressive");
        break;
    }

    emit modeChanged(m_mode);
}

QString DataSaverManager::modeToString(Mode mode) const
{
    switch (mode) {
    case Mode::Standard:
        return QCoreApplication::translate("DataSaver", "Standard");
    case Mode::Aggressive:
        return QCoreApplication::translate("DataSaver", "Aggressive");
    default:
        return QCoreApplication::translate("DataSaver", "Off");
    }
}

}