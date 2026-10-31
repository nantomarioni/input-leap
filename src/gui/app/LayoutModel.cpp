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

#include "LayoutModel.h"

#include <QFile>
#include <QTextStream>

namespace {

const char* kModifierNames[] = {"shift", "ctrl", "alt", "meta", "super", "none"};
const char* kFixNames[] = {"halfDuplexCapsLock", "halfDuplexNumLock", "halfDuplexScrollLock",
                           "xtestIsXineramaUnaware", "preserveFocus"};
const char* kCornerNames[] = {"top-left", "top-right", "bottom-left", "bottom-right"};

struct Neighbour { int dc; int dr; const char* name; };
const Neighbour kNeighbours[] = {{1, 0, "right"}, {-1, 0, "left"}, {0, -1, "up"}, {0, 1, "down"}};

template <class T>
QVector<T> readArray(QSettings& s, const QString& name, int count, const T& fallback)
{
    QVector<T> out(count, fallback);
    const int entries = s.beginReadArray(name + QStringLiteral("Array"));
    for (int i = 0; i < entries && i < count; ++i) {
        s.setArrayIndex(i);
        out[i] = s.value(name, fallback).template value<T>();
    }
    s.endArray();
    return out;
}

template <class T>
void writeArray(QSettings& s, const QString& name, const QVector<T>& values)
{
    s.beginWriteArray(name + QStringLiteral("Array"));
    for (int i = 0; i < values.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(name, values[i]);
    }
    s.endArray();
}

} // namespace

const char* ScreenConfig::modifierName(int modifier) { return kModifierNames[qBound(0, modifier, int(None))]; }
const char* ScreenConfig::cornerName(int corner) { return kCornerNames[qBound(0, corner, int(CornerCount) - 1)]; }
const char* ScreenConfig::fixName(int fix) { return kFixNames[qBound(0, fix, int(FixCount) - 1)]; }

LayoutModel::LayoutModel(QSettings* store, QObject* parent) :
    QAbstractListModel(parent),
    m_store(store)
{
    load();
}

// ---- model -----------------------------------------------------------------

int LayoutModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_cells.size();
}

QVariant LayoutModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_cells.size()) {
        return QVariant();
    }
    const ScreenConfig& cell = m_cells[index.row()];
    switch (role) {
    case NameRole: return cell.name;
    case ColumnRole: return index.row() % m_columns;
    case RowRole: return index.row() / m_columns;
    case OccupiedRole: return !cell.isNull();
    case IsServerRole: return !cell.isNull() && cell.name == m_serverName;
    case StateRole: return m_liveStates.value(cell.name);
    case PositionRole: return positionText(index.row());
    default: return QVariant();
    }
}

QHash<int, QByteArray> LayoutModel::roleNames() const
{
    return {{NameRole, "name"}, {ColumnRole, "column"}, {RowRole, "row"}, {OccupiedRole, "occupied"},
            {IsServerRole, "isServer"}, {StateRole, "state"}, {PositionRole, "position"}};
}

void LayoutModel::setServerName(const QString& name)
{
    if (m_serverName == name) {
        return;
    }
    const int old = cellOf(m_serverName);
    m_serverName = name;
    if (old >= 0 && !name.isEmpty() && cellOf(name) < 0) {
        m_cells[old].name = name;   // the server was renamed: keep its cell and options
        save();
    }
    emit dataChanged(index(0), index(m_cells.size() - 1));
    emit layoutChanged();
}

QStringList LayoutModel::screenNames() const
{
    QStringList names;
    for (const ScreenConfig& cell : m_cells) {
        if (!cell.isNull()) {
            names << cell.name;
        }
    }
    return names;
}

int LayoutModel::screenCount() const
{
    return screenNames().size();
}

// ---- cells -----------------------------------------------------------------

int LayoutModel::cellOf(const QString& name) const
{
    if (name.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < m_cells.size(); ++i) {
        if (m_cells[i].name == name) {
            return i;
        }
    }
    return -1;
}

bool LayoutModel::addScreen(const QString& name, int cell)
{
    if (name.trimmed().isEmpty() || cell < 0 || cell >= m_cells.size() || !m_cells[cell].isNull() || hasScreen(name)) {
        return false;
    }
    m_cells[cell].clear();
    m_cells[cell].name = name.trimmed();
    save();
    emit dataChanged(index(cell), index(cell));
    emit layoutChanged();
    return true;
}

bool LayoutModel::addScreenAnywhere(const QString& name)
{
    for (int i = 0; i < m_cells.size(); ++i) {
        if (m_cells[i].isNull()) {
            return addScreen(name, i);
        }
    }
    return false;
}

bool LayoutModel::placeNextTo(const QString& name, const QString& anchor, int direction)
{
    int start = cellOf(anchor);
    if (start < 0 || hasScreen(name)) {
        return false;
    }
    const int dc = direction == Left ? -1 : direction == Right ? 1 : 0;
    const int dr = direction == Up ? -1 : direction == Down ? 1 : 0;
    int cell = adjacentCell(start, dc, dr);
    while (cell >= 0) {
        if (m_cells[cell].isNull()) {
            return addScreen(name, cell);
        }
        cell = adjacentCell(cell, dc, dr);
    }
    return false;
}

bool LayoutModel::moveScreen(int fromCell, int toCell)
{
    if (fromCell < 0 || toCell < 0 || fromCell >= m_cells.size() || toCell >= m_cells.size() ||
        fromCell == toCell || m_cells[fromCell].isNull()) {
        return false;
    }
    std::swap(m_cells[fromCell], m_cells[toCell]);
    save();
    emit dataChanged(index(qMin(fromCell, toCell)), index(qMax(fromCell, toCell)));
    emit layoutChanged();
    return true;
}

void LayoutModel::removeScreen(int cell)
{
    if (cell < 0 || cell >= m_cells.size() || m_cells[cell].isNull()) {
        return;
    }
    m_cells[cell].clear();
    save();
    emit dataChanged(index(cell), index(cell));
    emit layoutChanged();
}

bool LayoutModel::renameScreen(int cell, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (cell < 0 || cell >= m_cells.size() || trimmed.isEmpty() || m_cells[cell].isNull()) {
        return false;
    }
    const int other = cellOf(trimmed);
    if (other >= 0 && other != cell) {
        return false;
    }
    if (m_cells[cell].name == m_serverName) {
        m_serverName = trimmed;
    }
    m_cells[cell].name = trimmed;
    save();
    emit dataChanged(index(cell), index(cell));
    emit layoutChanged();
    return true;
}

QVariantMap LayoutModel::screenSettings(int cell) const
{
    QVariantMap map;
    if (cell < 0 || cell >= m_cells.size()) {
        return map;
    }
    const ScreenConfig& s = m_cells[cell];
    map.insert(QStringLiteral("name"), s.name);
    map.insert(QStringLiteral("aliases"), s.aliases);
    QVariantList modifiers, corners, fixes;
    for (int m : s.modifiers) modifiers << m;
    for (bool c : s.corners) corners << c;
    for (bool f : s.fixes) fixes << f;
    map.insert(QStringLiteral("modifiers"), modifiers);
    map.insert(QStringLiteral("corners"), corners);
    map.insert(QStringLiteral("cornerSize"), s.cornerSize);
    map.insert(QStringLiteral("fixes"), fixes);
    map.insert(QStringLiteral("isServer"), s.name == m_serverName);
    map.insert(QStringLiteral("position"), positionText(cell));
    return map;
}

bool LayoutModel::setScreenSettings(int cell, const QVariantMap& settings)
{
    if (cell < 0 || cell >= m_cells.size() || m_cells[cell].isNull()) {
        return false;
    }
    ScreenConfig& s = m_cells[cell];
    if (settings.contains(QStringLiteral("aliases"))) {
        QStringList aliases;
        for (const QVariant& a : settings.value(QStringLiteral("aliases")).toList()) {
            const QString alias = a.toString().trimmed();
            if (!alias.isEmpty() && alias != s.name && !aliases.contains(alias)) {
                aliases << alias;
            }
        }
        s.aliases = aliases;
    }
    if (settings.contains(QStringLiteral("modifiers"))) {
        const QVariantList list = settings.value(QStringLiteral("modifiers")).toList();
        for (int i = 0; i < list.size() && i < s.modifiers.size(); ++i) {
            s.modifiers[i] = qBound(0, list[i].toInt(), int(ScreenConfig::None));
        }
    }
    if (settings.contains(QStringLiteral("corners"))) {
        const QVariantList list = settings.value(QStringLiteral("corners")).toList();
        for (int i = 0; i < list.size() && i < s.corners.size(); ++i) {
            s.corners[i] = list[i].toBool();
        }
    }
    if (settings.contains(QStringLiteral("cornerSize"))) {
        s.cornerSize = qMax(0, settings.value(QStringLiteral("cornerSize")).toInt());
    }
    if (settings.contains(QStringLiteral("fixes"))) {
        const QVariantList list = settings.value(QStringLiteral("fixes")).toList();
        for (int i = 0; i < list.size() && i < s.fixes.size(); ++i) {
            s.fixes[i] = list[i].toBool();
        }
    }
    save();
    emit dataChanged(index(cell), index(cell));
    emit layoutChanged();
    return true;
}

QString LayoutModel::positionText(int cell) const
{
    if (cell < 0 || cell >= m_cells.size() || m_cells[cell].isNull()) {
        return QString();
    }
    if (m_cells[cell].name == m_serverName) {
        return tr("centre");
    }
    const int server = cellOf(m_serverName);
    if (server >= 0) {
        const int sc = server % m_columns, sr = server / m_columns;
        const int c = cell % m_columns, r = cell / m_columns;
        if (r == sr && c > sc) return tr("right of %1").arg(m_serverName);
        if (r == sr && c < sc) return tr("left of %1").arg(m_serverName);
        if (c == sc && r < sr) return tr("above %1").arg(m_serverName);
        if (c == sc && r > sr) return tr("below %1").arg(m_serverName);
    }
    return tr("column %1, row %2").arg(cell % m_columns + 1).arg(cell / m_columns + 1);
}

QString LayoutModel::neighbour(int cell, int direction) const
{
    const int dc = direction == Left ? -1 : direction == Right ? 1 : 0;
    const int dr = direction == Up ? -1 : direction == Down ? 1 : 0;
    const int n = adjacentCell(cell, dc, dr);
    return n >= 0 ? m_cells[n].name : QString();
}

void LayoutModel::setLiveState(const QString& name, const QString& state)
{
    if (m_liveStates.value(name) == state) {
        return;
    }
    m_liveStates.insert(name, state);
    const int cell = cellOf(name);
    if (cell >= 0) {
        emit dataChanged(index(cell), index(cell), {StateRole});
    }
}

void LayoutModel::clearLiveStates()
{
    m_liveStates.clear();
    emit dataChanged(index(0), index(m_cells.size() - 1), {StateRole});
}

void LayoutModel::ensureServerPlaced()
{
    if (m_serverName.isEmpty() || hasScreen(m_serverName)) {
        return;
    }
    if (m_cells[centreCell()].isNull()) {
        addScreen(m_serverName, centreCell());
    } else {
        addScreenAnywhere(m_serverName);
    }
}

int LayoutModel::adjacentCell(int cell, int dColumn, int dRow) const
{
    if (cell < 0 || cell >= m_cells.size()) {
        return -1;
    }
    const int c = cell % m_columns + dColumn;
    const int r = cell / m_columns + dRow;
    if (c < 0 || c >= m_columns || r < 0 || r >= m_rows) {
        return -1;
    }
    return r * m_columns + c;
}

// ---- options ---------------------------------------------------------------

template <class T>
void LayoutModel::setOption(T& member, const T& value)
{
    if (member == value) {
        return;
    }
    member = value;
    save();
    emit optionsChanged();
}

QVariantList LayoutModel::switchCornerList() const
{
    QVariantList list;
    for (bool c : m_switchCorners) list << c;
    return list;
}

void LayoutModel::setSwitchCorner(int corner, bool on)
{
    if (corner < 0 || corner >= m_switchCorners.size() || m_switchCorners[corner] == on) {
        return;
    }
    m_switchCorners[corner] = on;
    save();
    emit optionsChanged();
}

void LayoutModel::setHasHeartbeat(bool on) { setOption(m_hasHeartbeat, on); }
void LayoutModel::setHeartbeat(int ms) { setOption(m_heartbeat, qMax(1, ms)); }
void LayoutModel::setRelativeMouseMoves(bool on) { setOption(m_relativeMouseMoves, on); }
void LayoutModel::setScreenSaverSync(bool on) { setOption(m_screenSaverSync, on); }
void LayoutModel::setWin32KeepForeground(bool on) { setOption(m_win32KeepForeground, on); }
void LayoutModel::setHasSwitchDelay(bool on) { setOption(m_hasSwitchDelay, on); }
void LayoutModel::setSwitchDelay(int ms) { setOption(m_switchDelay, qMax(0, ms)); }
void LayoutModel::setHasSwitchDoubleTap(bool on) { setOption(m_hasSwitchDoubleTap, on); }
void LayoutModel::setSwitchDoubleTap(int ms) { setOption(m_switchDoubleTap, qMax(0, ms)); }
void LayoutModel::setSwitchCornerSize(int px) { setOption(m_switchCornerSize, qMax(0, px)); }
void LayoutModel::setIgnoreAutoConfigClient(bool on) { setOption(m_ignoreAutoConfigClient, on); }
void LayoutModel::setEnableDragAndDrop(bool on) { setOption(m_enableDragAndDrop, on); }
void LayoutModel::setClipboardSharing(bool on) { setOption(m_clipboardSharing, on); }
void LayoutModel::setClipboardSharingSizeMb(int mb) { setOption(m_clipboardSharingSize, qulonglong(qMax(1, mb)) * 1000 * 1000); }
void LayoutModel::setDimmingEnabled(bool on) { setOption(m_dimmingEnabled, on); }
void LayoutModel::setDimmingPercentage(int percent) { setOption(m_dimmingPercentage, qBound(0, percent, 100)); }

// ---- hotkeys ---------------------------------------------------------------

QVariantList LayoutModel::hotkeyList() const
{
    QVariantList list;
    for (const Hotkey& hotkey : m_hotkeys) {
        QVariantMap map;
        map.insert(QStringLiteral("text"), hotkey.text());
        map.insert(QStringLiteral("key"), hotkey.keySequence().toString());
        QVariantList actions;
        for (const Action& action : hotkey.actions()) {
            actions << action.text();
        }
        map.insert(QStringLiteral("actions"), actions);
        list << map;
    }
    return list;
}

void LayoutModel::removeHotkey(int index)
{
    if (index < 0 || index >= int(m_hotkeys.size())) {
        return;
    }
    m_hotkeys.erase(m_hotkeys.begin() + index);
    hotkeysEdited();
}

void LayoutModel::removeAction(int hotkeyIndex, int actionIndex)
{
    if (hotkeyIndex < 0 || hotkeyIndex >= int(m_hotkeys.size())) {
        return;
    }
    Hotkey& hotkey = m_hotkeys[hotkeyIndex];
    if (actionIndex < 0 || actionIndex >= int(hotkey.actions().size())) {
        return;
    }
    hotkey.removeAction(actionIndex);
    hotkeysEdited();
}

void LayoutModel::hotkeysEdited()
{
    save();
    emit hotkeysChanged();
}

// ---- persistence -----------------------------------------------------------

void LayoutModel::load()
{
    QSettings& s = *m_store;
    s.beginGroup(QStringLiteral("internalConfig"));

    m_columns = qMax(1, s.value(QStringLiteral("numColumns"), 5).toInt());
    m_rows = qMax(1, s.value(QStringLiteral("numRows"), 3).toInt());
    beginResetModel();
    m_cells = QVector<ScreenConfig>(m_columns * m_rows);

    m_hasHeartbeat = s.value(QStringLiteral("hasHeartbeat"), false).toBool();
    m_heartbeat = s.value(QStringLiteral("heartbeat"), 5000).toInt();
    m_relativeMouseMoves = s.value(QStringLiteral("relativeMouseMoves"), false).toBool();
    m_screenSaverSync = s.value(QStringLiteral("screenSaverSync"), true).toBool();
    m_win32KeepForeground = s.value(QStringLiteral("win32KeepForeground"), false).toBool();
    m_hasSwitchDelay = s.value(QStringLiteral("hasSwitchDelay"), false).toBool();
    m_switchDelay = s.value(QStringLiteral("switchDelay"), 250).toInt();
    m_hasSwitchDoubleTap = s.value(QStringLiteral("hasSwitchDoubleTap"), false).toBool();
    m_switchDoubleTap = s.value(QStringLiteral("switchDoubleTap"), 250).toInt();
    m_switchCornerSize = s.value(QStringLiteral("switchCornerSize"), 0).toInt();
    m_ignoreAutoConfigClient = s.value(QStringLiteral("ignoreAutoConfigClient"), false).toBool();
    m_enableDragAndDrop = s.value(QStringLiteral("enableDragAndDrop"), true).toBool();
    m_clipboardSharing = s.value(QStringLiteral("clipboardSharing"), true).toBool();
    m_clipboardSharingSize = s.value(QStringLiteral("clipboardSharingSize"), 100 * 1000 * 1000).toULongLong();
    m_dimmingEnabled = s.value(QStringLiteral("screenDimmingEnabled"), false).toBool();
    m_dimmingPercentage = qBound(0, s.value(QStringLiteral("screenDimmingPercentage"), 30).toInt(), 100);
    m_switchCorners = readArray<bool>(s, QStringLiteral("switchCorner"), ScreenConfig::CornerCount, false);

    const int screens = s.beginReadArray(QStringLiteral("screens"));
    for (int i = 0; i < screens && i < m_cells.size(); ++i) {
        s.setArrayIndex(i);
        loadScreen(m_cells[i]);
    }
    s.endArray();

    m_hotkeys.clear();
    const int hotkeys = s.beginReadArray(QStringLiteral("hotkeys"));
    for (int i = 0; i < hotkeys; ++i) {
        s.setArrayIndex(i);
        Hotkey hotkey;
        hotkey.loadSettings(s);
        m_hotkeys.push_back(hotkey);
    }
    s.endArray();

    s.endGroup();
    endResetModel();
    emit layoutChanged();
    emit optionsChanged();
    emit hotkeysChanged();
}

void LayoutModel::loadScreen(ScreenConfig& screen)
{
    QSettings& s = *m_store;
    screen.clear();
    screen.name = s.value(QStringLiteral("name")).toString();
    if (screen.isNull()) {
        return;
    }
    screen.cornerSize = s.value(QStringLiteral("switchCornerSize"), 0).toInt();
    screen.aliases.clear();
    const int aliases = s.beginReadArray(QStringLiteral("aliasArray"));
    for (int i = 0; i < aliases; ++i) {
        s.setArrayIndex(i);
        const QString alias = s.value(QStringLiteral("alias")).toString();
        if (!alias.isEmpty()) {
            screen.aliases << alias;
        }
    }
    s.endArray();
    const QVector<int> modifiers = readArray<int>(s, QStringLiteral("modifier"), ScreenConfig::ModifierCount - 1, -1);
    for (int i = 0; i < modifiers.size() && i < screen.modifiers.size(); ++i) {
        screen.modifiers[i] = modifiers[i] < 0 ? i : qBound(0, modifiers[i], int(ScreenConfig::None));
    }
    screen.corners = readArray<bool>(s, QStringLiteral("switchCorner"), ScreenConfig::CornerCount, false);
    screen.fixes = readArray<bool>(s, QStringLiteral("fix"), ScreenConfig::FixCount, false);
}

void LayoutModel::saveScreen(const ScreenConfig& screen)
{
    QSettings& s = *m_store;
    s.setValue(QStringLiteral("name"), screen.name);
    if (screen.isNull()) {
        return;
    }
    s.setValue(QStringLiteral("switchCornerSize"), screen.cornerSize);
    writeArray<QString>(s, QStringLiteral("alias"), QVector<QString>(screen.aliases.begin(), screen.aliases.end()));
    // the legacy store keeps "default" (-1) for an unmapped modifier
    QVector<int> modifiers;
    for (int i = 0; i < screen.modifiers.size(); ++i) {
        modifiers << (screen.modifiers[i] == i ? -1 : screen.modifiers[i]);
    }
    writeArray<int>(s, QStringLiteral("modifier"), modifiers);
    writeArray<bool>(s, QStringLiteral("switchCorner"), screen.corners);
    writeArray<bool>(s, QStringLiteral("fix"), screen.fixes);
}

void LayoutModel::save()
{
    QSettings& s = *m_store;
    s.beginGroup(QStringLiteral("internalConfig"));
    s.remove(QString());

    s.setValue(QStringLiteral("numColumns"), m_columns);
    s.setValue(QStringLiteral("numRows"), m_rows);
    s.setValue(QStringLiteral("hasHeartbeat"), m_hasHeartbeat);
    s.setValue(QStringLiteral("heartbeat"), m_heartbeat);
    s.setValue(QStringLiteral("relativeMouseMoves"), m_relativeMouseMoves);
    s.setValue(QStringLiteral("screenSaverSync"), m_screenSaverSync);
    s.setValue(QStringLiteral("win32KeepForeground"), m_win32KeepForeground);
    s.setValue(QStringLiteral("hasSwitchDelay"), m_hasSwitchDelay);
    s.setValue(QStringLiteral("switchDelay"), m_switchDelay);
    s.setValue(QStringLiteral("hasSwitchDoubleTap"), m_hasSwitchDoubleTap);
    s.setValue(QStringLiteral("switchDoubleTap"), m_switchDoubleTap);
    s.setValue(QStringLiteral("switchCornerSize"), m_switchCornerSize);
    s.setValue(QStringLiteral("ignoreAutoConfigClient"), m_ignoreAutoConfigClient);
    s.setValue(QStringLiteral("enableDragAndDrop"), m_enableDragAndDrop);
    s.setValue(QStringLiteral("clipboardSharing"), m_clipboardSharing);
    s.setValue(QStringLiteral("clipboardSharingSize"), int(m_clipboardSharingSize));
    s.setValue(QStringLiteral("screenDimmingEnabled"), m_dimmingEnabled);
    s.setValue(QStringLiteral("screenDimmingPercentage"), m_dimmingPercentage);
    writeArray<bool>(s, QStringLiteral("switchCorner"), m_switchCorners);

    s.beginWriteArray(QStringLiteral("screens"));
    for (int i = 0; i < m_cells.size(); ++i) {
        s.setArrayIndex(i);
        saveScreen(m_cells[i]);
    }
    s.endArray();

    s.beginWriteArray(QStringLiteral("hotkeys"));
    for (int i = 0; i < int(m_hotkeys.size()); ++i) {
        s.setArrayIndex(i);
        m_hotkeys[i].saveSettings(s);
    }
    s.endArray();

    s.endGroup();
}

// ---- the .conf -------------------------------------------------------------

QString LayoutModel::configText() const
{
    QString text;
    QTextStream out(&text);

    out << "section: screens\n";
    for (const ScreenConfig& s : m_cells) {
        if (s.isNull()) continue;
        out << "\t" << s.name << ":\n";
        for (int i = 0; i < s.modifiers.size(); ++i) {
            if (s.modifiers[i] != i) {
                out << "\t\t" << ScreenConfig::modifierName(i) << " = " << ScreenConfig::modifierName(s.modifiers[i]) << "\n";
            }
        }
        for (int i = 0; i < s.fixes.size(); ++i) {
            out << "\t\t" << ScreenConfig::fixName(i) << " = " << (s.fixes[i] ? "true" : "false") << "\n";
        }
        out << "\t\tswitchCorners = none ";
        for (int i = 0; i < s.corners.size(); ++i) {
            if (s.corners[i]) out << "+" << ScreenConfig::cornerName(i) << " ";
        }
        out << "\n";
        out << "\t\tswitchCornerSize = " << s.cornerSize << "\n";
    }
    out << "end\n\n";

    out << "section: aliases\n";
    for (const ScreenConfig& s : m_cells) {
        if (s.isNull() || s.aliases.isEmpty()) continue;
        out << "\t" << s.name << ":\n";
        for (const QString& alias : s.aliases) {
            out << "\t\t" << alias << "\n";
        }
    }
    out << "end\n\n";

    out << "section: links\n";
    for (int i = 0; i < m_cells.size(); ++i) {
        if (m_cells[i].isNull()) continue;
        out << "\t" << m_cells[i].name << ":\n";
        for (const Neighbour& n : kNeighbours) {
            const int cell = adjacentCell(i, n.dc, n.dr);
            if (cell >= 0 && !m_cells[cell].isNull()) {
                out << "\t\t" << n.name << " = " << m_cells[cell].name << "\n";
            }
        }
    }
    out << "end\n\n";

    out << "section: options\n";
    if (m_hasHeartbeat) out << "\theartbeat = " << m_heartbeat << "\n";
    out << "\trelativeMouseMoves = " << (m_relativeMouseMoves ? "true" : "false") << "\n";
    out << "\tscreenSaverSync = " << (m_screenSaverSync ? "true" : "false") << "\n";
    out << "\twin32KeepForeground = " << (m_win32KeepForeground ? "true" : "false") << "\n";
    out << "\tclipboardSharing = " << (m_clipboardSharing ? "true" : "false") << "\n";
    out << "\tclipboardSharingSize = " << m_clipboardSharingSize << "\n";
    if (m_hasSwitchDelay) out << "\tswitchDelay = " << m_switchDelay << "\n";
    if (m_hasSwitchDoubleTap) out << "\tswitchDoubleTap = " << m_switchDoubleTap << "\n";
    out << "\tswitchCorners = none ";
    for (int i = 0; i < m_switchCorners.size(); ++i) {
        if (m_switchCorners[i]) out << "+" << ScreenConfig::cornerName(i) << " ";
    }
    out << "\n";
    out << "\tswitchCornerSize = " << m_switchCornerSize << "\n";
    out << "\tscreenDimmingEnabled = " << (m_dimmingEnabled ? "true" : "false") << "\n";
    out << "\tscreenDimmingPercentage = " << m_dimmingPercentage << "\n";
    for (const Hotkey& hotkey : m_hotkeys) {
        out << hotkey;
    }
    out << "end\n\n";

    out.flush();
    return text;
}

bool LayoutModel::writeConfig(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray bytes = configText().toUtf8();
    return file.write(bytes) == bytes.size();
}
