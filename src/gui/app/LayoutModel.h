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
#include <QJsonObject>
#include <QRect>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqmlregistration.h>

/* One display of a computer: its size and where it sits inside the
 * computer's desktop, in pixels. Typed by hand until the core reports it
 * (docs/ui-redesign.md §4.4).
 */
struct DisplayConfig
{
    int x = 0;
    int y = 0;
    int width = 1920;
    int height = 1080;

    QRect rect() const { return QRect(x, y, width, height); }
    bool operator==(const DisplayConfig& other) const
    {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
};

/* One screen (computer) in the server's layout: where its desktop sits in
 * the shared pixel space, its displays, and the options the server holds
 * for it. The options keep the legacy settings keys and the legacy `.conf`
 * text, so a store written by the Widgets GUI still loads.
 */
struct ScreenConfig
{
    enum Modifier { Shift, Ctrl, Alt, Meta, Super, None, ModifierCount };
    enum Corner { TopLeft, TopRight, BottomLeft, BottomRight, CornerCount };
    enum Fix { CapsLock, NumLock, ScrollLock, XTest, PreserveFocus, FixCount };

    QString name;
    int x = 0;                               // desktop origin in the shared space
    int y = 0;
    QVector<DisplayConfig> displays = {DisplayConfig()};
    QStringList aliases;
    QVector<int> modifiers = {Shift, Ctrl, Alt, Meta, Super};   // index = modifier, value = mapped to
    QVector<bool> corners = QVector<bool>(CornerCount, false);
    int cornerSize = 0;
    QVector<bool> fixes = QVector<bool>(FixCount, false);

    bool isNull() const { return name.isEmpty(); }
    void clear() { *this = ScreenConfig(); }

    // The desktop: the bounding box of the displays, placed at (x, y).
    QRect desktop() const;
    // Shifts the displays so their bounding box starts at (0, 0), moving the
    // origin the opposite way so nothing moves on the desk; clamps the sizes.
    void normaliseDisplays();

    static const char* modifierName(int modifier);
    static const char* cornerName(int corner);
    static const char* fixName(int fix);
};

/* A link between two desktops that touch: `side` of `from` leads to `to`.
 * The ranges are percentages of each edge, (0, 100) when the whole edge.
 */
struct ScreenLink
{
    int from = -1;
    int to = -1;
    int side = 0;          // LayoutModel::Direction
    int fromStart = 0;
    int fromEnd = 100;
    int toStart = 0;
    int toEnd = 100;
};

/* The server's layout and options (docs/ui-redesign.md §4.4, phase B): the
 * computers placed freely in a shared pixel space, each one the bounding
 * box of its displays; two desktops whose edges touch are linked, with
 * ranges when the touching part is not the whole edge. Plus the per-screen
 * options, the switching/behaviour options and the hotkeys. Writes itself
 * to QSettings (`internalConfig` group) and renders the `.conf` the core
 * reads. A store from the grid era (phase A, the Widgets GUI) migrates on
 * load: every cell becomes one desktop of the default size.
 *
 * As a list model it exposes one row per computer to the Layout page.
 */
class LayoutModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reach it through App.layout")

    Q_PROPERTY(QString serverName READ serverName WRITE setServerName NOTIFY layoutChanged)
    Q_PROPERTY(QStringList screenNames READ screenNames NOTIFY layoutChanged)
    Q_PROPERTY(int screenCount READ screenCount NOTIFY layoutChanged)
    Q_PROPERTY(QRect bounds READ bounds NOTIFY layoutChanged)
    Q_PROPERTY(bool isMirror READ isMirror CONSTANT)
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
        XRole,
        YRole,
        WidthRole,
        HeightRole,
        DisplaysRole,
        IsServerRole,
        LinkedRole,
        StateRole,
        PositionRole
    };
    Q_ENUM(Roles)

    enum Direction { Left, Right, Up, Down };
    Q_ENUM(Direction)

    // The desktop a computer gets until its displays are typed in.
    static constexpr int kDefaultWidth = 1920;
    static constexpr int kDefaultHeight = 1080;

    // store = nullptr: a mirror, filled with loadJson() and never saved
    // (the server's layout as a client sees it, docs/ui-redesign.md §4.7).
    explicit LayoutModel(QSettings* store, QObject* parent = nullptr);
    bool isMirror() const { return m_store == nullptr; }

    // QAbstractListModel
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString serverName() const { return m_serverName; }
    void setServerName(const QString& name);

    QStringList screenNames() const;
    int screenCount() const { return m_screens.size(); }
    const QVector<ScreenConfig>& screens() const { return m_screens; }
    QRect bounds() const;   // the union of the desktops; empty when none

    // ---- computers ---------------------------------------------------------
    Q_INVOKABLE int indexOf(const QString& name) const;
    Q_INVOKABLE bool hasScreen(const QString& name) const { return indexOf(name) >= 0; }
    Q_INVOKABLE bool addScreen(const QString& name, int x, int y);
    Q_INVOKABLE bool addScreenAnywhere(const QString& name);
    Q_INVOKABLE bool placeNextTo(const QString& name, const QString& anchor, int direction);
    Q_INVOKABLE bool moveTo(int index, int x, int y);          // false when it would overlap
    Q_INVOKABLE QVariantMap snapPosition(int index, int x, int y, int threshold) const;
    Q_INVOKABLE void removeScreen(int index);
    Q_INVOKABLE bool renameScreen(int index, const QString& name);
    Q_INVOKABLE QVariantMap screenSettings(int index) const;
    Q_INVOKABLE bool setScreenSettings(int index, const QVariantMap& settings);
    Q_INVOKABLE bool addDisplay(int index);                     // to the right of the last one
    Q_INVOKABLE bool removeDisplay(int index, int display);
    Q_INVOKABLE QString positionText(int index) const;         // "left of desk-win", "centre", "not linked"
    Q_INVOKABLE QVariantList linksOf(int index) const;         // [{name, side, fromStart, fromEnd, toStart, toEnd}]
    QVector<ScreenLink> links() const;

    // Live state shown on the tiles, set by the controller (never persisted).
    Q_INVOKABLE void setLiveState(const QString& name, const QString& state);
    Q_INVOKABLE void clearLiveStates();
    Q_INVOKABLE QString liveState(const QString& name) const { return m_liveStates.value(name); }

    // Makes sure the server has a desktop so links can be computed.
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
    int addHotkey(const KeySequence& keys);                 // returns the new index, -1 when invalid/duplicate
    bool addAction(int hotkeyIndex, const Action& action);
    Q_INVOKABLE void removeHotkey(int index);
    Q_INVOKABLE void removeAction(int hotkeyIndex, int actionIndex);
    void hotkeysEdited();   // after direct edits through hotkeys()

    // ---- persistence & output ---------------------------------------------
    void load();
    void save();
    QJsonObject toJson() const;                 // the whole server configuration
    void loadJson(const QJsonObject& json);     // replaces everything; a mirror's only input
    Q_INVOKABLE QString configText() const;
    Q_INVOKABLE bool writeConfig(const QString& path) const;

signals:
    void layoutChanged();
    void optionsChanged();
    void hotkeysChanged();

private:
    bool overlapsAnother(int index, const QRect& rect) const;
    bool fitsAt(const QRect& rect, int except) const { return !overlapsAnother(except, rect); }
    void changed(int first, int last);
    void loadScreen(ScreenConfig& screen);
    void loadLegacyGrid(QSettings& s);
    void saveScreen(const ScreenConfig& screen);
    template <class T> void setOption(T& member, const T& value);

    QSettings* m_store;
    QString m_serverName;
    QVector<ScreenConfig> m_screens;
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
    QVariantList m_mirrorHotkeys;   // a mirror shows the server's hotkeys as text
};
