/*  InputLeap -- mouse and keyboard sharing utility
    Copyright (C) - Nicolas Antomarioni (nantomarioni@gmail.com)

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "Hotkey.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqmlregistration.h>

/* One screen (computer) in the server's layout, with the options the server
 * holds for it. Same settings keys and the same `.conf` text as the legacy
 * ServerConfig/Screen, so the store round-trips between the two GUIs.
 */
struct ScreenConfig
{
    enum Modifier { Shift, Ctrl, Alt, Meta, Super, None, ModifierCount };
    enum Corner { TopLeft, TopRight, BottomLeft, BottomRight, CornerCount };
    enum Fix { CapsLock, NumLock, ScrollLock, XTest, PreserveFocus, FixCount };

    QString name;
    QStringList aliases;
    QVector<int> modifiers = {Shift, Ctrl, Alt, Meta, Super};   // index = modifier, value = mapped to
    QVector<bool> corners = QVector<bool>(CornerCount, false);
    int cornerSize = 0;
    QVector<bool> fixes = QVector<bool>(FixCount, false);

    bool isNull() const { return name.isEmpty(); }
    void clear() { *this = ScreenConfig(); }

    static const char* modifierName(int modifier);
    static const char* cornerName(int corner);
    static const char* fixName(int fix);
};

/* The server's layout and options: a grid of cells (5×3 like the legacy
 * GUI, phase A of docs/ui-redesign.md §4.4), the per-screen options, the
 * switching/behaviour options and the hotkeys. Writes itself to QSettings
 * (`internalConfig` group, legacy keys) and renders the `.conf` the core
 * reads.
 *
 * As a list model it exposes the cells to the Layout page: one row per cell,
 * empty cells included (name is empty).
 */
class LayoutModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reach it through App.layout")

    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(QString serverName READ serverName WRITE setServerName NOTIFY layoutChanged)
    Q_PROPERTY(QStringList screenNames READ screenNames NOTIFY layoutChanged)
    Q_PROPERTY(int screenCount READ screenCount NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList hotkeys READ hotkeyList NOTIFY hotkeysChanged)

    // options (section: options)
    Q_PROPERTY(bool hasHeartbeat READ hasHeartbeat WRITE setHasHeartbeat NOTIFY optionsChanged)
    Q_PROPERTY(int heartbeat READ heartbeat WRITE setHeartbeat NOTIFY optionsChanged)
    Q_PROPERTY(bool relativeMouseMoves READ relativeMouseMoves WRITE setRelativeMouseMoves NOTIFY optionsChanged)
    Q_PROPERTY(bool screenSaverSync READ screenSaverSync WRITE setScreenSaverSync NOTIFY optionsChanged)
    Q_PROPERTY(bool win32KeepForeground READ win32KeepForeground WRITE setWin32KeepForeground NOTIFY optionsChanged)
    Q_PROPERTY(bool hasSwitchDelay READ hasSwitchDelay WRITE setHasSwitchDelay NOTIFY optionsChanged)
    Q_PROPERTY(int switchDelay READ switchDelay WRITE setSwitchDelay NOTIFY optionsChanged)
    Q_PROPERTY(bool hasSwitchDoubleTap READ hasSwitchDoubleTap WRITE setHasSwitchDoubleTap NOTIFY optionsChanged)
    Q_PROPERTY(int switchDoubleTap READ switchDoubleTap WRITE setSwitchDoubleTap NOTIFY optionsChanged)
    Q_PROPERTY(int switchCornerSize READ switchCornerSize WRITE setSwitchCornerSize NOTIFY optionsChanged)
    Q_PROPERTY(QVariantList switchCorners READ switchCornerList NOTIFY optionsChanged)
    Q_PROPERTY(bool ignoreAutoConfigClient READ ignoreAutoConfigClient WRITE setIgnoreAutoConfigClient NOTIFY optionsChanged)
    Q_PROPERTY(bool enableDragAndDrop READ enableDragAndDrop WRITE setEnableDragAndDrop NOTIFY optionsChanged)
    Q_PROPERTY(bool clipboardSharing READ clipboardSharing WRITE setClipboardSharing NOTIFY optionsChanged)
    Q_PROPERTY(int clipboardSharingSizeMb READ clipboardSharingSizeMb WRITE setClipboardSharingSizeMb NOTIFY optionsChanged)
    Q_PROPERTY(bool dimmingEnabled READ dimmingEnabled WRITE setDimmingEnabled NOTIFY optionsChanged)
    Q_PROPERTY(int dimmingPercentage READ dimmingPercentage WRITE setDimmingPercentage NOTIFY optionsChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        ColumnRole,
        RowRole,
        OccupiedRole,
        IsServerRole,
        StateRole,
        PositionRole
    };
    Q_ENUM(Roles)

    enum Direction { Left, Right, Up, Down };
    Q_ENUM(Direction)

    explicit LayoutModel(QSettings* store, QObject* parent = nullptr);

    // QAbstractListModel
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int columns() const { return m_columns; }
    int rows() const { return m_rows; }
    QString serverName() const { return m_serverName; }
    void setServerName(const QString& name);

    QStringList screenNames() const;
    int screenCount() const;
    const QVector<ScreenConfig>& cells() const { return m_cells; }

    // ---- cells -----------------------------------------------------------
    Q_INVOKABLE int cellOf(const QString& name) const;
    Q_INVOKABLE bool hasScreen(const QString& name) const { return cellOf(name) >= 0; }
    Q_INVOKABLE bool addScreen(const QString& name, int cell);
    Q_INVOKABLE bool addScreenAnywhere(const QString& name);
    Q_INVOKABLE bool placeNextTo(const QString& name, const QString& anchor, int direction);
    Q_INVOKABLE bool moveScreen(int fromCell, int toCell);
    Q_INVOKABLE void removeScreen(int cell);
    Q_INVOKABLE bool renameScreen(int cell, const QString& name);
    Q_INVOKABLE QVariantMap screenSettings(int cell) const;
    Q_INVOKABLE bool setScreenSettings(int cell, const QVariantMap& settings);
    Q_INVOKABLE QString positionText(int cell) const;   // "left of desk-win", "centre"
    Q_INVOKABLE QString neighbour(int cell, int direction) const;

    // Live state shown on the tiles, set by the controller (never persisted).
    Q_INVOKABLE void setLiveState(const QString& name, const QString& state);
    Q_INVOKABLE void clearLiveStates();
    Q_INVOKABLE QString liveState(const QString& name) const { return m_liveStates.value(name); }

    // Makes sure the server has a cell (centre) so links can be computed.
    void ensureServerPlaced();

    // ---- options -----------------------------------------------------------
    bool hasHeartbeat() const { return m_hasHeartbeat; }
    int heartbeat() const { return m_heartbeat; }
    bool relativeMouseMoves() const { return m_relativeMouseMoves; }
    bool screenSaverSync() const { return m_screenSaverSync; }
    bool win32KeepForeground() const { return m_win32KeepForeground; }
    bool hasSwitchDelay() const { return m_hasSwitchDelay; }
    int switchDelay() const { return m_switchDelay; }
    bool hasSwitchDoubleTap() const { return m_hasSwitchDoubleTap; }
    int switchDoubleTap() const { return m_switchDoubleTap; }
    int switchCornerSize() const { return m_switchCornerSize; }
    QVariantList switchCornerList() const;
    Q_INVOKABLE bool switchCorner(int corner) const { return m_switchCorners.value(corner, false); }
    Q_INVOKABLE void setSwitchCorner(int corner, bool on);
    bool ignoreAutoConfigClient() const { return m_ignoreAutoConfigClient; }
    bool enableDragAndDrop() const { return m_enableDragAndDrop; }
    bool clipboardSharing() const { return m_clipboardSharing; }
    int clipboardSharingSizeMb() const { return int(m_clipboardSharingSize / (1000 * 1000)); }
    bool dimmingEnabled() const { return m_dimmingEnabled; }
    int dimmingPercentage() const { return m_dimmingPercentage; }

    void setHasHeartbeat(bool on);
    void setHeartbeat(int ms);
    void setRelativeMouseMoves(bool on);
    void setScreenSaverSync(bool on);
    void setWin32KeepForeground(bool on);
    void setHasSwitchDelay(bool on);
    void setSwitchDelay(int ms);
    void setHasSwitchDoubleTap(bool on);
    void setSwitchDoubleTap(int ms);
    void setSwitchCornerSize(int px);
    void setIgnoreAutoConfigClient(bool on);
    void setEnableDragAndDrop(bool on);
    void setClipboardSharing(bool on);
    void setClipboardSharingSizeMb(int mb);
    void setDimmingEnabled(bool on);
    void setDimmingPercentage(int percent);

    // ---- hotkeys -----------------------------------------------------------
    const std::vector<Hotkey>& hotkeys() const { return m_hotkeys; }
    std::vector<Hotkey>& hotkeys() { return m_hotkeys; }
    QVariantList hotkeyList() const;
    Q_INVOKABLE void removeHotkey(int index);
    Q_INVOKABLE void removeAction(int hotkeyIndex, int actionIndex);
    void hotkeysEdited();   // after direct edits through hotkeys()

    // ---- persistence & output ---------------------------------------------
    void load();
    void save();
    Q_INVOKABLE QString configText() const;
    Q_INVOKABLE bool writeConfig(const QString& path) const;

signals:
    void layoutChanged();
    void optionsChanged();
    void hotkeysChanged();

private:
    int adjacentCell(int cell, int dColumn, int dRow) const;
    void loadScreen(ScreenConfig& screen);
    void saveScreen(const ScreenConfig& screen);
    template <class T> void setOption(T& member, const T& value);
    int centreCell() const { return (m_rows / 2) * m_columns + m_columns / 2; }

    QSettings* m_store;
    int m_columns = 5;
    int m_rows = 3;
    QString m_serverName;
    QVector<ScreenConfig> m_cells;
    QHash<QString, QString> m_liveStates;

    bool m_hasHeartbeat = false;
    int m_heartbeat = 5000;
    bool m_relativeMouseMoves = false;
    bool m_screenSaverSync = true;
    bool m_win32KeepForeground = false;
    bool m_hasSwitchDelay = false;
    int m_switchDelay = 250;
    bool m_hasSwitchDoubleTap = false;
    int m_switchDoubleTap = 250;
    int m_switchCornerSize = 0;
    QVector<bool> m_switchCorners = QVector<bool>(ScreenConfig::CornerCount, false);
    bool m_ignoreAutoConfigClient = false;
    bool m_enableDragAndDrop = true;
    bool m_clipboardSharing = true;
    qulonglong m_clipboardSharingSize = 100ull * 1000 * 1000;
    bool m_dimmingEnabled = false;
    int m_dimmingPercentage = 30;

    std::vector<Hotkey> m_hotkeys;
};
