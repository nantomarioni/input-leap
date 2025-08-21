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

#pragma once

#include "ScreenSetupModel.h"
#include <QRect>
#include <QList>
#include <QMap>
#include <QSize>

class ServerConfig;

struct MonitorInfo {
    QString name;
    QRect geometry;
    bool isPrimary;
    int refreshRate;
    QSize physicalSize;
    QString deviceId;
    bool isEnabled;
    
    MonitorInfo() : isPrimary(false), refreshRate(60), isEnabled(true) {}
};

struct ScreenMonitorLayout {
    QString screenName;
    QList<MonitorInfo> monitors;
    QRect combinedGeometry;
    QString primaryMonitorId;
    
    ScreenMonitorLayout() = default;
    ScreenMonitorLayout(const QString& name) : screenName(name) {}
};

class MultiMonitorScreenSetupModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MultiMonitorScreenSetupModel(QObject* parent = nullptr);
    ~MultiMonitorScreenSetupModel() override;

    // Enhanced screen management with multi-monitor support
    bool addScreen(const QString& name, const QList<MonitorInfo>& monitors);
    bool removeScreen(const QString& name);
    void clearScreens();
    
    // Monitor management for screens
    bool addMonitorToScreen(const QString& screenName, const MonitorInfo& monitor);
    bool removeMonitorFromScreen(const QString& screenName, const QString& monitorId);
    bool updateMonitorInScreen(const QString& screenName, const QString& monitorId, const MonitorInfo& monitor);
    
    // Monitor queries
    QList<MonitorInfo> getMonitorsForScreen(const QString& screenName) const;
    MonitorInfo getMonitor(const QString& screenName, const QString& monitorId) const;
    QString getPrimaryMonitorForScreen(const QString& screenName) const;
    bool setPrimaryMonitorForScreen(const QString& screenName, const QString& monitorId);
    
    // Layout calculations
    QRect getCombinedGeometryForScreen(const QString& screenName) const;
    QRect getEffectiveScreenGeometry(const QString& screenName) const;
    QPoint getScreenCenterPoint(const QString& screenName) const;
    
    // Multi-monitor layout validation
    bool validateMonitorLayout(const QString& screenName) const;
    bool hasOverlappingMonitors(const QString& screenName) const;
    bool hasValidPrimaryMonitor(const QString& screenName) const;
    
    // Auto-detection and discovery
    bool detectLocalMonitors();
    QList<MonitorInfo> getDetectedMonitors() const;
    bool applyDetectedMonitorsToScreen(const QString& screenName);
    
    // Configuration management
    void loadFromServerConfig(ServerConfig& config);
    void saveToServerConfig(ServerConfig& config) const;
    
    // Layout templates and presets
    bool saveLayoutAsTemplate(const QString& templateName, const QString& screenName);
    bool loadLayoutFromTemplate(const QString& templateName, const QString& screenName);
    QStringList getAvailableTemplates() const;
    bool deleteLayoutTemplate(const QString& templateName);
    
    // Enhanced positioning with monitor awareness
    bool moveScreenToPosition(const QString& name, int x, int y);
    bool canMoveScreenToPosition(const QString& name, int x, int y) const;
    QList<QString> getAdjacentScreens(const QString& name) const;
    
    // Monitor-aware edge detection
    bool isScreenAtLeftEdge(const QString& screenName, const QString& monitorId) const;
    bool isScreenAtRightEdge(const QString& screenName, const QString& monitorId) const;
    bool isScreenAtTopEdge(const QString& screenName, const QString& monitorId) const;
    bool isScreenAtBottomEdge(const QString& screenName, const QString& monitorId) const;
    
    // Advanced layout operations
    bool arrangeScreensHorizontally(const QStringList& screenNames);
    bool arrangeScreensVertically(const QStringList& screenNames);
    bool arrangeScreensInGrid(const QStringList& screenNames, int columns);
    bool optimizeScreenLayout();
    
    // Monitor resolution and scaling
    bool setMonitorResolution(const QString& screenName, const QString& monitorId, const QSize& resolution);
    bool setMonitorScaling(const QString& screenName, const QString& monitorId, double scaleFactor);
    double getMonitorScaling(const QString& screenName, const QString& monitorId) const;
    
    // Export/Import functionality
    bool exportLayoutToFile(const QString& filePath) const;
    bool importLayoutFromFile(const QString& filePath);
    QString exportLayoutToJson() const;
    bool importLayoutFromJson(const QString& jsonData);

signals:
    void monitorAdded(const QString& screenName, const QString& monitorId);
    void monitorRemoved(const QString& screenName, const QString& monitorId);
    void monitorUpdated(const QString& screenName, const QString& monitorId);
    void primaryMonitorChanged(const QString& screenName, const QString& monitorId);
    void screenLayoutOptimized();
    void layoutTemplateChanged();
    void monitorDetectionCompleted();

private slots:
    void onScreenChanged(const QString& name);
    void onMonitorGeometryChanged(const QString& screenName, const QString& monitorId);

private:
    // Internal data structures
    QMap<QString, ScreenMonitorLayout> m_ScreenLayouts;
    QList<MonitorInfo> m_DetectedMonitors;
    QMap<QString, QString> m_LayoutTemplates;  // template name -> JSON data
    
    // Helper methods
    void updateCombinedGeometry(const QString& screenName);
    bool validateMonitorConfiguration(const ScreenMonitorLayout& layout) const;
    void calculateOptimalPositions(QList<ScreenMonitorLayout>& layouts);
    QRect calculateBoundingRect(const QList<MonitorInfo>& monitors) const;
    
    // Platform-specific monitor detection
    QList<MonitorInfo> detectMonitorsWindows() const;
    QList<MonitorInfo> detectMonitorsLinux() const;
    QList<MonitorInfo> detectMonitorsMacOS() const;
    
    // Layout algorithm helpers
    void arrangeMonitorsInScreen(ScreenMonitorLayout& layout);
    bool checkMonitorOverlap(const MonitorInfo& monitor1, const MonitorInfo& monitor2) const;
    QPoint findOptimalMonitorPosition(const ScreenMonitorLayout& layout, const MonitorInfo& newMonitor) const;
};
