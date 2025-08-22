/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2024 InputLeap Developers
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MultiMonitorScreenSetupModel.h"
#include "ServerConfig.h"

#include <QGuiApplication>
#include <QScreen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MultiMonitorScreenSetupModel::MultiMonitorScreenSetupModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    // Connect to screen change events
    connect(QGuiApplication::primaryScreen(), &QScreen::geometryChanged,
            this, [this]() { detectLocalMonitors(); });
}

MultiMonitorScreenSetupModel::~MultiMonitorScreenSetupModel() = default;

bool MultiMonitorScreenSetupModel::addScreen(const QString& name, const QList<MonitorInfo>& monitors)
{
    if (name.isEmpty() || m_ScreenLayouts.contains(name)) {
        return false;
    }
    
    ScreenMonitorLayout layout(name);
    layout.monitors = monitors;
    updateCombinedGeometry(name);
    
    if (!monitors.isEmpty()) {
        // Set the first monitor as primary by default
        layout.primaryMonitorId = monitors.first().deviceId;
    }
    
    m_ScreenLayouts[name] = layout;
    
    emit screenAdded(name);
    return true;
}

bool MultiMonitorScreenSetupModel::removeScreen(const QString& name)
{
    if (!m_ScreenLayouts.contains(name)) {
        return false;
    }
    
    const auto& layout = m_ScreenLayouts[name];
    for (const auto& monitor : layout.monitors) {
        emit monitorRemoved(name, monitor.deviceId);
    }
    
    m_ScreenLayouts.remove(name);
    
    emit screenRemoved(name);
    return true;
}

void MultiMonitorScreenSetupModel::clearScreens()
{
    for (auto it = m_ScreenLayouts.begin(); it != m_ScreenLayouts.end(); ++it) {
        const auto& layout = it.value();
        for (const auto& monitor : layout.monitors) {
            emit monitorRemoved(it.key(), monitor.deviceId);
        }
    }
    
    m_ScreenLayouts.clear();
    emit screenLayoutChanged();
}

bool MultiMonitorScreenSetupModel::addMonitorToScreen(const QString& screenName, const MonitorInfo& monitor)
{
    if (!m_ScreenLayouts.contains(screenName)) {
        return false;
    }
    
    auto& layout = m_ScreenLayouts[screenName];
    
    // Check if monitor already exists
    for (const auto& existingMonitor : layout.monitors) {
        if (existingMonitor.deviceId == monitor.deviceId) {
            return false;
        }
    }
    
    layout.monitors.append(monitor);
    updateCombinedGeometry(screenName);
    
    // Set as primary if it's the first monitor
    if (layout.monitors.size() == 1) {
        layout.primaryMonitorId = monitor.deviceId;
    }
    
    emit monitorAdded(screenName, monitor.deviceId);
    return true;
}

bool MultiMonitorScreenSetupModel::removeMonitorFromScreen(const QString& screenName, const QString& monitorId)
{
    if (!m_ScreenLayouts.contains(screenName)) {
        return false;
    }
    
    auto& layout = m_ScreenLayouts[screenName];
    
    for (int i = 0; i < layout.monitors.size(); ++i) {
        if (layout.monitors[i].deviceId == monitorId) {
            layout.monitors.removeAt(i);
            updateCombinedGeometry(screenName);
            
            // If removed monitor was primary, set new primary
            if (layout.primaryMonitorId == monitorId && !layout.monitors.isEmpty()) {
                layout.primaryMonitorId = layout.monitors.first().deviceId;
                emit primaryMonitorChanged(screenName, layout.primaryMonitorId);
            }
            
            emit monitorRemoved(screenName, monitorId);
            return true;
        }
    }
    
    return false;
}

bool MultiMonitorScreenSetupModel::updateMonitorInScreen(const QString& screenName, const QString& monitorId, const MonitorInfo& monitor)
{
    if (!m_ScreenLayouts.contains(screenName)) {
        return false;
    }
    
    auto& layout = m_ScreenLayouts[screenName];
    
    for (auto& existingMonitor : layout.monitors) {
        if (existingMonitor.deviceId == monitorId) {
            existingMonitor = monitor;
            updateCombinedGeometry(screenName);
            emit monitorUpdated(screenName, monitorId);
            return true;
        }
    }
    
    return false;
}

QList<MonitorInfo> MultiMonitorScreenSetupModel::getMonitorsForScreen(const QString& screenName) const
{
    if (m_ScreenLayouts.contains(screenName)) {
        return m_ScreenLayouts[screenName].monitors;
    }
    return {};
}

MonitorInfo MultiMonitorScreenSetupModel::getMonitor(const QString& screenName, const QString& monitorId) const
{
    if (m_ScreenLayouts.contains(screenName)) {
        const auto& layout = m_ScreenLayouts[screenName];
        for (const auto& monitor : layout.monitors) {
            if (monitor.deviceId == monitorId) {
                return monitor;
            }
        }
    }
    return {};
}

QString MultiMonitorScreenSetupModel::getPrimaryMonitorForScreen(const QString& screenName) const
{
    if (m_ScreenLayouts.contains(screenName)) {
        return m_ScreenLayouts[screenName].primaryMonitorId;
    }
    return {};
}

bool MultiMonitorScreenSetupModel::setPrimaryMonitorForScreen(const QString& screenName, const QString& monitorId)
{
    if (!m_ScreenLayouts.contains(screenName)) {
        return false;
    }
    
    auto& layout = m_ScreenLayouts[screenName];
    
    // Check if monitor exists
    bool found = false;
    for (const auto& monitor : layout.monitors) {
        if (monitor.deviceId == monitorId) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        return false;
    }
    
    layout.primaryMonitorId = monitorId;
    emit primaryMonitorChanged(screenName, monitorId);
    return true;
}

QRect MultiMonitorScreenSetupModel::getCombinedGeometryForScreen(const QString& screenName) const
{
    if (m_ScreenLayouts.contains(screenName)) {
        return m_ScreenLayouts[screenName].combinedGeometry;
    }
    return {};
}

QRect MultiMonitorScreenSetupModel::getEffectiveScreenGeometry(const QString& screenName) const
{
    return getCombinedGeometryForScreen(screenName);
}

QPoint MultiMonitorScreenSetupModel::getScreenCenterPoint(const QString& screenName) const
{
    QRect geometry = getCombinedGeometryForScreen(screenName);
    return geometry.center();
}

bool MultiMonitorScreenSetupModel::validateMonitorLayout(const QString& screenName) const
{
    if (!m_ScreenLayouts.contains(screenName)) {
        return false;
    }
    
    const auto& layout = m_ScreenLayouts[screenName];
    return validateMonitorConfiguration(layout) && 
           hasValidPrimaryMonitor(screenName) && 
           !hasOverlappingMonitors(screenName);
}

bool MultiMonitorScreenSetupModel::hasOverlappingMonitors(const QString& screenName) const
{
    if (!m_ScreenLayouts.contains(screenName)) {
        return false;
    }
    
    const auto& layout = m_ScreenLayouts[screenName];
    const auto& monitors = layout.monitors;
    
    for (int i = 0; i < monitors.size(); ++i) {
        for (int j = i + 1; j < monitors.size(); ++j) {
            if (checkMonitorOverlap(monitors[i], monitors[j])) {
                return true;
            }
        }
    }
    
    return false;
}

bool MultiMonitorScreenSetupModel::hasValidPrimaryMonitor(const QString& screenName) const
{
    if (!m_ScreenLayouts.contains(screenName)) {
        return false;
    }
    
    const auto& layout = m_ScreenLayouts[screenName];
    const QString& primaryId = layout.primaryMonitorId;
    
    if (primaryId.isEmpty()) {
        return layout.monitors.isEmpty();
    }
    
    for (const auto& monitor : layout.monitors) {
        if (monitor.deviceId == primaryId) {
            return true;
        }
    }
    
    return false;
}

bool MultiMonitorScreenSetupModel::detectLocalMonitors()
{
    m_DetectedMonitors.clear();
    
#ifdef Q_OS_WIN
    m_DetectedMonitors = detectMonitorsWindows();
#elif defined(Q_OS_LINUX)
    m_DetectedMonitors = detectMonitorsLinux();
#elif defined(Q_OS_MACOS)
    m_DetectedMonitors = detectMonitorsMacOS();
#else
    // Fallback to Qt screens
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        const auto screen = screens[i];
        MonitorInfo monitor;
        monitor.name = screen->name();
        monitor.geometry = screen->geometry();
        monitor.isPrimary = (screen == QGuiApplication::primaryScreen());
        monitor.refreshRate = static_cast<int>(screen->refreshRate());
        monitor.physicalSize = screen->physicalSize().toSize();
        monitor.deviceId = QString("qt_screen_%1").arg(i);
        monitor.isEnabled = true;
        m_DetectedMonitors.append(monitor);
    }
#endif
    
    emit monitorDetectionCompleted();
    return !m_DetectedMonitors.isEmpty();
}

QList<MonitorInfo> MultiMonitorScreenSetupModel::getDetectedMonitors() const
{
    return m_DetectedMonitors;
}

bool MultiMonitorScreenSetupModel::applyDetectedMonitorsToScreen(const QString& screenName)
{
    if (!m_ScreenLayouts.contains(screenName) || m_DetectedMonitors.isEmpty()) {
        return false;
    }
    
    auto& layout = m_ScreenLayouts[screenName];
    layout.monitors = m_DetectedMonitors;
    updateCombinedGeometry(screenName);
    
    // Set primary monitor
    for (const auto& monitor : m_DetectedMonitors) {
        if (monitor.isPrimary) {
            layout.primaryMonitorId = monitor.deviceId;
            break;
        }
    }
    
    if (layout.primaryMonitorId.isEmpty() && !m_DetectedMonitors.isEmpty()) {
        layout.primaryMonitorId = m_DetectedMonitors.first().deviceId;
    }
    
    for (const auto& monitor : m_DetectedMonitors) {
        emit monitorAdded(screenName, monitor.deviceId);
    }
    
    return true;
}

void MultiMonitorScreenSetupModel::loadFromServerConfig(ServerConfig& config)
{
    // TODO: Load multi-monitor specific configuration from ServerConfig
    // This would require extending ServerConfig to support monitor information
    Q_UNUSED(config);
}

void MultiMonitorScreenSetupModel::saveToServerConfig(ServerConfig& config) const
{
    // TODO: Save multi-monitor specific configuration to ServerConfig
    // This would require extending ServerConfig to support monitor information
    Q_UNUSED(config);
}

QString MultiMonitorScreenSetupModel::exportLayoutToJson() const
{
    QJsonObject root;
    QJsonArray screensArray;
    
    for (auto it = m_ScreenLayouts.begin(); it != m_ScreenLayouts.end(); ++it) {
        const auto& layout = it.value();
        QJsonObject screenObj;
        screenObj["name"] = layout.screenName;
        screenObj["primaryMonitor"] = layout.primaryMonitorId;
        
        QJsonArray monitorsArray;
        for (const auto& monitor : layout.monitors) {
            QJsonObject monitorObj;
            monitorObj["name"] = monitor.name;
            monitorObj["deviceId"] = monitor.deviceId;
            monitorObj["x"] = monitor.geometry.x();
            monitorObj["y"] = monitor.geometry.y();
            monitorObj["width"] = monitor.geometry.width();
            monitorObj["height"] = monitor.geometry.height();
            monitorObj["isPrimary"] = monitor.isPrimary;
            monitorObj["isEnabled"] = monitor.isEnabled;
            monitorObj["refreshRate"] = monitor.refreshRate;
            monitorObj["physicalWidth"] = monitor.physicalSize.width();
            monitorObj["physicalHeight"] = monitor.physicalSize.height();
            monitorsArray.append(monitorObj);
        }
        
        screenObj["monitors"] = monitorsArray;
        screensArray.append(screenObj);
    }
    
    root["screens"] = screensArray;
    root["version"] = "1.0";
    
    QJsonDocument doc(root);
    return doc.toJson();
}

bool MultiMonitorScreenSetupModel::importLayoutFromJson(const QString& jsonData)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        return false;
    }
    
    QJsonObject root = doc.object();
    QJsonArray screensArray = root["screens"].toArray();
    
    clearScreens();
    
    for (const auto& screenValue : screensArray) {
        QJsonObject screenObj = screenValue.toObject();
        QString screenName = screenObj["name"].toString();
        QString primaryMonitorId = screenObj["primaryMonitor"].toString();
        
        QJsonArray monitorsArray = screenObj["monitors"].toArray();
        QList<MonitorInfo> monitors;
        
        for (const auto& monitorValue : monitorsArray) {
            QJsonObject monitorObj = monitorValue.toObject();
            MonitorInfo monitor;
            monitor.name = monitorObj["name"].toString();
            monitor.deviceId = monitorObj["deviceId"].toString();
            monitor.geometry = QRect(
                monitorObj["x"].toInt(),
                monitorObj["y"].toInt(),
                monitorObj["width"].toInt(),
                monitorObj["height"].toInt()
            );
            monitor.isPrimary = monitorObj["isPrimary"].toBool();
            monitor.isEnabled = monitorObj["isEnabled"].toBool();
            monitor.refreshRate = monitorObj["refreshRate"].toInt();
            monitor.physicalSize = QSize(
                monitorObj["physicalWidth"].toInt(),
                monitorObj["physicalHeight"].toInt()
            );
            monitors.append(monitor);
        }
        
        addScreen(screenName, monitors);
        setPrimaryMonitorForScreen(screenName, primaryMonitorId);
    }
    
    return true;
}

bool MultiMonitorScreenSetupModel::exportLayoutToFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    QString jsonData = exportLayoutToJson();
    file.write(jsonData.toUtf8());
    return true;
}

bool MultiMonitorScreenSetupModel::importLayoutFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QString jsonData = file.readAll();
    return importLayoutFromJson(jsonData);
}

// Private helper methods
void MultiMonitorScreenSetupModel::updateCombinedGeometry(const QString& screenName)
{
    if (!m_ScreenLayouts.contains(screenName)) {
        return;
    }
    
    auto& layout = m_ScreenLayouts[screenName];
    if (layout.monitors.isEmpty()) {
        layout.combinedGeometry = QRect();
        return;
    }
    
    layout.combinedGeometry = calculateBoundingRect(layout.monitors);
}

bool MultiMonitorScreenSetupModel::validateMonitorConfiguration(const ScreenMonitorLayout& layout) const
{
    // Check for duplicate device IDs
    QSet<QString> deviceIds;
    for (const auto& monitor : layout.monitors) {
        if (deviceIds.contains(monitor.deviceId)) {
            return false;
        }
        deviceIds.insert(monitor.deviceId);
    }
    
    return true;
}

QRect MultiMonitorScreenSetupModel::calculateBoundingRect(const QList<MonitorInfo>& monitors) const
{
    if (monitors.isEmpty()) {
        return QRect();
    }
    
    QRect boundingRect = monitors.first().geometry;
    for (int i = 1; i < monitors.size(); ++i) {
        boundingRect = boundingRect.united(monitors[i].geometry);
    }
    
    return boundingRect;
}

bool MultiMonitorScreenSetupModel::checkMonitorOverlap(const MonitorInfo& monitor1, const MonitorInfo& monitor2) const
{
    return monitor1.geometry.intersects(monitor2.geometry);
}

// Platform-specific monitor detection
#ifdef Q_OS_WIN
QList<MonitorInfo> MultiMonitorScreenSetupModel::detectMonitorsWindows() const
{
    QList<MonitorInfo> monitors;
    
    // Use Qt's screen detection as fallback
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        const auto screen = screens[i];
        MonitorInfo monitor;
        monitor.name = screen->name();
        monitor.geometry = screen->geometry();
        monitor.isPrimary = (screen == QGuiApplication::primaryScreen());
        monitor.refreshRate = static_cast<int>(screen->refreshRate());
        monitor.physicalSize = screen->physicalSize().toSize();
        monitor.deviceId = QString("windows_screen_%1").arg(i);
        monitor.isEnabled = true;
        monitors.append(monitor);
    }
    
    return monitors;
}
#endif

#ifdef Q_OS_LINUX
QList<MonitorInfo> MultiMonitorScreenSetupModel::detectMonitorsLinux() const
{
    QList<MonitorInfo> monitors;
    
    // Use Qt's screen detection
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        const auto screen = screens[i];
        MonitorInfo monitor;
        monitor.name = screen->name();
        monitor.geometry = screen->geometry();
        monitor.isPrimary = (screen == QGuiApplication::primaryScreen());
        monitor.refreshRate = static_cast<int>(screen->refreshRate());
        monitor.physicalSize = screen->physicalSize().toSize();
        monitor.deviceId = QString("linux_screen_%1").arg(i);
        monitor.isEnabled = true;
        monitors.append(monitor);
    }
    
    return monitors;
}
#endif

#ifdef Q_OS_MACOS
QList<MonitorInfo> MultiMonitorScreenSetupModel::detectMonitorsMacOS() const
{
    QList<MonitorInfo> monitors;
    
    // Use Qt's screen detection
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        const auto screen = screens[i];
        MonitorInfo monitor;
        monitor.name = screen->name();
        monitor.geometry = screen->geometry();
        monitor.isPrimary = (screen == QGuiApplication::primaryScreen());
        monitor.refreshRate = static_cast<int>(screen->refreshRate());
        monitor.physicalSize = screen->physicalSize().toSize();
        monitor.deviceId = QString("macos_screen_%1").arg(i);
        monitor.isEnabled = true;
        monitors.append(monitor);
    }
    
    return monitors;
}
#endif

// Slot implementations
void MultiMonitorScreenSetupModel::onScreenChanged(const QString& name)
{
    updateCombinedGeometry(name);
    emit screenLayoutChanged(name);
}

void MultiMonitorScreenSetupModel::onMonitorGeometryChanged(const QString& screenName, const QString& monitorId)
{
    updateCombinedGeometry(screenName);
    emit monitorUpdated(screenName, monitorId);
}
