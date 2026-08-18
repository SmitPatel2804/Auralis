#pragma once

#include <QString>
#include <QStringView>

namespace auralis::bluetooth {

enum class AgentCapability {
    NoInputNoOutput,
    DisplayOnly,
    DisplayYesNo,
    KeyboardOnly,
    KeyboardDisplay
};

inline QStringView toBlueZCapability(AgentCapability capability)
{
    switch (capability) {
    case AgentCapability::NoInputNoOutput:
        return u"NoInputNoOutput";
    case AgentCapability::DisplayOnly:
        return u"DisplayOnly";
    case AgentCapability::DisplayYesNo:
        return u"DisplayYesNo";
    case AgentCapability::KeyboardOnly:
        return u"KeyboardOnly";
    case AgentCapability::KeyboardDisplay:
        return u"KeyboardDisplay";
    }
    return u"KeyboardDisplay";
}

inline AgentCapability parseAgentCapability(const QString& raw)
{
    const QString normalized = raw.trimmed();
    if (normalized.compare(QStringLiteral("NoInputNoOutput"), Qt::CaseInsensitive) == 0) {
        return AgentCapability::NoInputNoOutput;
    }
    if (normalized.compare(QStringLiteral("DisplayOnly"), Qt::CaseInsensitive) == 0) {
        return AgentCapability::DisplayOnly;
    }
    if (normalized.compare(QStringLiteral("DisplayYesNo"), Qt::CaseInsensitive) == 0) {
        return AgentCapability::DisplayYesNo;
    }
    if (normalized.compare(QStringLiteral("KeyboardOnly"), Qt::CaseInsensitive) == 0) {
        return AgentCapability::KeyboardOnly;
    }
    return AgentCapability::KeyboardDisplay;
}

} // namespace auralis::bluetooth
