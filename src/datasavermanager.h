#pragma once

#include <QObject>
#include <QString>

namespace spacepenguin {

class DataSaverManager : public QObject
{
    Q_OBJECT

public:
    enum class Mode { Off, Standard, Aggressive };

    explicit DataSaverManager(QObject *parent = nullptr);
    ~DataSaverManager() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);

    bool isDataSavingEnabled() const { return m_mode != Mode::Off; }

    QString modeToString(Mode mode) const;

signals:
    void modeChanged(Mode mode);

private:
    Mode m_mode = Mode::Off;
};

}