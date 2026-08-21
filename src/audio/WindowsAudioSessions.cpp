#include "WindowsAudioSessions.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <QCryptographicHash>
#include <QDataStream>
#include <QFileInfo>
#include <QIODevice>
#include <QSet>

#include <algorithm>

namespace auralis::audio {
namespace {

using Microsoft::WRL::ComPtr;

QString hresultText(HRESULT result)
{
    wchar_t* message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(result),
        0,
        reinterpret_cast<wchar_t*>(&message),
        0,
        nullptr);
    const QString text = length > 0 ? QString::fromWCharArray(message, static_cast<int>(length)).trimmed() : QString();
    if (message != nullptr) LocalFree(message);
    return text.isEmpty()
        ? QStringLiteral("HRESULT 0x%1").arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'))
        : text;
}

QString processExecutableName(DWORD processId)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) return QStringLiteral("Process %1").arg(processId);
    wchar_t path[32768]{};
    DWORD size = static_cast<DWORD>(sizeof(path) / sizeof(path[0]));
    const bool ok = QueryFullProcessImageNameW(process, 0, path, &size) != FALSE;
    CloseHandle(process);
    if (!ok) return QStringLiteral("Process %1").arg(processId);
    const QString base = QFileInfo(QString::fromWCharArray(path, static_cast<int>(size))).completeBaseName();
    if (base.compare(QLatin1String("brave"), Qt::CaseInsensitive) == 0) return QStringLiteral("Brave");
    if (base.compare(QLatin1String("chrome"), Qt::CaseInsensitive) == 0) return QStringLiteral("Google Chrome");
    if (base.compare(QLatin1String("msedge"), Qt::CaseInsensitive) == 0) return QStringLiteral("Microsoft Edge");
    if (base.compare(QLatin1String("firefox"), Qt::CaseInsensitive) == 0) return QStringLiteral("Firefox");
    return base.isEmpty() ? QStringLiteral("Process %1").arg(processId) : base;
}

QString takeCoTaskString(LPWSTR value)
{
    const QString result = value != nullptr ? QString::fromWCharArray(value).trimmed() : QString();
    CoTaskMemFree(value);
    return result;
}

} // namespace

QVector<WindowsAudioSession> enumerateWindowsAudioSessions(QString* errorText)
{
    if (errorText != nullptr) errorText->clear();
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = init == S_OK || init == S_FALSE;
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) {
        if (errorText != nullptr) *errorText = hresultText(init);
        return {};
    }

    QVector<WindowsAudioSession> result;
    ComPtr<IMMDeviceEnumerator> deviceEnumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
    ComPtr<IMMDeviceCollection> endpoints;
    if (SUCCEEDED(hr)) hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &endpoints);
    UINT endpointCount = 0;
    if (SUCCEEDED(hr)) hr = endpoints->GetCount(&endpointCount);

    QSet<QString> seen;
    for (UINT endpointIndex = 0; SUCCEEDED(hr) && endpointIndex < endpointCount; ++endpointIndex) {
        ComPtr<IMMDevice> endpoint;
        if (FAILED(endpoints->Item(endpointIndex, &endpoint))) continue;
        ComPtr<IAudioSessionManager2> manager;
        if (FAILED(endpoint->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, &manager))) continue;
        ComPtr<IAudioSessionEnumerator> sessions;
        if (FAILED(manager->GetSessionEnumerator(&sessions))) continue;
        int count = 0;
        if (FAILED(sessions->GetCount(&count))) continue;
        for (int index = 0; index < count; ++index) {
            ComPtr<IAudioSessionControl> control;
            if (FAILED(sessions->GetSession(index, &control))) continue;
            ComPtr<IAudioSessionControl2> control2;
            if (FAILED(control.As(&control2)) || control2->IsSystemSoundsSession() == S_OK) continue;
            DWORD processId = 0;
            if (FAILED(control2->GetProcessId(&processId)) || processId == 0 || processId == GetCurrentProcessId()) continue;
            AudioSessionState state = AudioSessionStateExpired;
            if (FAILED(control->GetState(&state)) || state == AudioSessionStateExpired) continue;

            LPWSTR rawIdentifier = nullptr;
            control2->GetSessionIdentifier(&rawIdentifier);
            const QString identifier = takeCoTaskString(rawIdentifier);
            // Process loopback is endpoint-independent and captures the full
            // target process tree. The same app session can be projected by
            // multiple render endpoints, so one row per PID is the correct UI
            // and routing abstraction.
            const QString key = QString::number(processId);
            if (seen.contains(key)) continue;
            seen.insert(key);

            LPWSTR rawDisplay = nullptr;
            control->GetDisplayName(&rawDisplay);
            const QString sessionDisplay = takeCoTaskString(rawDisplay);
            WindowsAudioSession session;
            session.processId = processId;
            session.applicationName = processExecutableName(processId);
            session.displayName = sessionDisplay.startsWith(QLatin1Char('@')) || sessionDisplay.isEmpty()
                ? session.applicationName
                : sessionDisplay;
            session.sessionIdentifier = identifier.isEmpty() ? key : identifier;
            session.active = state == AudioSessionStateActive;
            result.push_back(std::move(session));
        }
    }

    if (FAILED(hr) && errorText != nullptr) *errorText = hresultText(hr);
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.active != right.active) return left.active;
        const int nameCompare = QString::compare(left.applicationName, right.applicationName, Qt::CaseInsensitive);
        if (nameCompare != 0) return nameCompare < 0;
        return left.processId < right.processId;
    });
    if (uninitialize) CoUninitialize();
    return result;
}

QByteArray windowsAudioSessionFingerprint(const QVector<WindowsAudioSession>& sessions)
{
    QByteArray serialized;
    QDataStream stream(&serialized, QIODevice::WriteOnly);
    for (const WindowsAudioSession& session : sessions) {
        stream << session.processId << session.applicationName << session.displayName
               << session.sessionIdentifier << session.active;
    }
    return QCryptographicHash::hash(serialized, QCryptographicHash::Sha256);
}

} // namespace auralis::audio
