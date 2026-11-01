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
#include <QJsonArray>
#include <QJsonObject>
#include <QLine>
#include <QRect>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqmlregistration.h>

/* One display of a computer: its size and where it sits inside the
 * computer's desktop, in the pixels its core sees, and how big it really is
 * (0 when unknown). Reported by that computer's window over the settings
 * channel (or its zeroconf record), or typed by hand (docs/ui-redesign.md
 * §4.4).
 */
struct DisplayConfig
{
    int x = 0;
    int y = 0;
    int width = 1920;
    int height = 1080;
    int mmWidth = 0;
    int mmHeight = 0;

    QRect rect() const { return QRect(x, y, width, height); }
    bool operator==(const DisplayConfig& other) const
    {
        return x == other.x && y == other.y && width == other.width && height == other.height &&
               mmWidth == other.mmWidth && mmHeight == other.mmHeight;
    }
    bool operator!=(const DisplayConfig& other) const { return !(*this == other); }
};

/* One screen (computer) in the server's layout: where its desktop sits in
 * the shared space, its displays, how big its pixels are on the desk, and
 * the options the server holds for it. The shared space counts 96-dpi
 * pixels: a computer's own pixels are multiplied by its scale, so desktops
 * are drawn, and linked, at their real sizes. The options keep the legacy settings keys and the legacy `.conf`
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
    bool displaysReported = false;           // the displays came from the computer itself
    bool displaysTyped = false;              // typed by hand: reports no longer replace them
    int scale = 1000;                        // shared-space units per 1000 of its own pixels
    bool scaleTyped = false;                 // typed by hand: measurements no longer replace it
    QStringList aliases;
    QVector<int> modifiers = {Shift, Ctrl, Alt, Meta, Super};   // index = modifier, value = mapped to
    QVector<bool> corners = QVector<bool>(CornerCount, false);
    int cornerSize = 0;
    QVector<bool> fixes = QVector<bool>(FixCount, false);

    bool isNull() const { return name.isEmpty(); }
    void clear() { *this = ScreenConfig(); }

    // The desktop: the bounding box of the displays, scaled and placed at
    // (x, y) in the shared space.
    QRect desktop() const;
    // One display in the shared space; edges shared inside the desktop stay
    // shared once scaled.
    QRect displayRect(int index) const;
    // The desktop in its own pixels, from (0, 0).
    QSize nativeSize() const;
    int toShared(int pixels) const;
    // Shifts the displays so their bounding box starts at (0, 0), moving the
    // origin the opposite way so nothing moves on the desk; clamps the sizes.
    void normaliseDisplays();

    static const char* modifierName(int modifier);
    static const char* cornerName(int corner);
    static const char* fixName(int fix);
};

/* A link between two desktops that touch: `side` of `from` leads to `to`.
 * The ranges are percentages of each edge, (0, 100) when the whole edge;
 * `line` is the shared part in the shared space. A side may carry several:
 * only where a display of each desktop reaches the touching edges.
 */
struct ScreenLink
{
    int from = -1;
    int to = -1;
    int side = 0;          // LayoutModel::Direction
    double fromStart = 0;
    double fromEnd = 100;
    double toStart = 0;
    double toEnd = 100;
    QLine line;
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
    // Nothing may be changed here: a configuration file, or a client's copy
    // of the server's layout without the right to change it.
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY readOnlyChanged)
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
    // The scale's bounds, in thousandths: 25 dpi to 480 dpi pixels.
    static constexpr int kMinScale = 200;
    static constexpr int kMaxScale = 4000;

    // store = nullptr: a mirror, filled with loadJson() and never saved
    // (the server's layout as a client sees it, docs/ui-redesign.md §4.7).
    explicit LayoutModel(QSettings* store, QObject* parent = nullptr);
    bool isMirror() const { return m_store == nullptr; }
    bool readOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);

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
    // Where the cursor crosses, once per pair: [{x1, y1, x2, y2}] in the shared space.
    Q_INVOKABLE QVariantList crossings() const;

    // Live state shown on the tiles, set by the controller (never persisted).
    Q_INVOKABLE void setLiveState(const QString& name, const QString& state);
    Q_INVOKABLE void clearLiveStates();
    Q_INVOKABLE QString liveState(const QString& name) const { return m_liveStates.value(name); }

    // Makes sure the server has a desktop so links can be computed.
    void ensureServerPlaced();

    // ---- displays reported by the computers themselves --------------------
    // Remembers what `name` reports (placed or not; a computer placed later
    // starts with them) and applies it unless its displays were typed by
    // hand. A desktop that grows or shrinks pushes the desktops right of and
    // below it along so they keep touching. True when the layout changed.
    bool reportDisplays(const QString& name, const QVector<DisplayConfig>& displays);
    QVector<DisplayConfig> reportedDisplays(const QString& name) const { return m_reported.value(name); }
    // Replaces hand-typed displays with the reported ones; false when there
    // are none or they do not fit.
    Q_INVOKABLE bool useReportedDisplays(int index);
    // A typed scale (percent of a 96-dpi pixel), or back to the measured one.
    Q_INVOKABLE bool setScale(int index, double percent);
    Q_INVOKABLE bool useMeasuredScale(int index);
    // The scale the displays' real sizes give, in thousandths; 0 when they
    // say nothing usable.
    static int measuredScale(const QVector<DisplayConfig>& displays);

    // The wire forms of a display list: JSON for the settings channel, a
    // short text ("0,0,3024x1964;3024,0,2560x1440") for a zeroconf TXT
    // entry, "@597x336" after a display giving its millimetres. Reading
    // clamps sizes and offsets, drops what does not parse,
    // keeps at most kMaxReportedDisplays and moves the bounding box to (0, 0).
    static constexpr int kMaxReportedDisplays = 8;
    static QVector<DisplayConfig> cleanDisplays(const QVector<DisplayConfig>& displays);
    static QJsonArray displaysToJson(const QVector<DisplayConfig>& displays);
    static QVector<DisplayConfig> displaysFromJson(const QJsonArray& json);
    static QString displaysToText(const QVector<DisplayConfig>& displays, bool withSizes = true);
    static QVector<DisplayConfig> displaysFromText(const QString& text);

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
    // The server takes a whole configuration a client edited: screens,
    // options and hotkeys, checked and saved; its own name stays. False (and
    // nothing changed) when it is not a configuration.
    bool replaceWith(const QJsonObject& json, QString* error = nullptr);
    // A configuration file in the core's format (section: screens / links /
    // aliases / options): screens, options and hotkeys as written, desktops
    // laid out from the links (sizes from their ranges). For a mirror.
    void loadConfigText(const QString& text, const QString& serverName);
    Q_INVOKABLE QString configText() const;
    Q_INVOKABLE bool writeConfig(const QString& path) const;

signals:
    // A change made here (by the window, not loaded from elsewhere): what the
    // server applies and a client sends to the server.
    void edited();
    void readOnlyChanged();
    void layoutChanged();
    void optionsChanged();
    void hotkeysChanged();

private:
    bool overlapsAnother(int index, const QRect& rect) const;
    bool applyDisplays(int index, const QVector<DisplayConfig>& displays);
    bool applyGeometry(int index, const ScreenConfig& probe);
    int measuredScaleOf(int index) const;
    ScreenConfig newScreen(const QString& name) const;
    bool fitsAt(const QRect& rect, int except) const { return !overlapsAnother(except, rect); }
    void changed(int first, int last);
    void loadScreen(ScreenConfig& screen);
    void loadLegacyGrid(QSettings& s);
    void saveScreen(const ScreenConfig& screen);
    template <class T> void setOption(T& member, const T& value);

    QSettings* m_store;
    bool m_readOnly = false;
    QString m_serverName;
    QVector<ScreenConfig> m_screens;
    QHash<QString, QString> m_liveStates;
    QHash<QString, QVector<DisplayConfig>> m_reported;   // never persisted: re-reported every session

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
