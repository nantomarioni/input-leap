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
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace {

const char* kModifierNames[] = {"shift", "ctrl", "alt", "meta", "super", "none"};
const char* kFixNames[] = {"halfDuplexCapsLock", "halfDuplexNumLock", "halfDuplexScrollLock",
                           "xtestIsXineramaUnaware", "preserveFocus"};
const char* kCornerNames[] = {"top-left", "top-right", "bottom-left", "bottom-right"};
const char* kSideNames[] = {"left", "right", "up", "down"};

// The settings layout: 1 = the 5x3 grid of cells (phase A and the Widgets
// GUI), 2 = free placement with displays.
const int kLayoutVersion = 2;
const int kMaxDimension = 100000;

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

// A share of an edge in percent, to the hundredth the core reads.
double percent(double value, int whole)
{
    return whole <= 0 ? 0 : qBound(0.0, std::round(10000.0 * value / whole) / 100.0, 100.0);
}

QString percentText(double value)
{
    QString text = QString::number(value, 'f', 2);
    while (text.endsWith(QLatin1Char('0'))) text.chop(1);
    if (text.endsWith(QLatin1Char('.'))) text.chop(1);
    return text;
}

// The exclusive right/bottom edge, unlike QRect::right()/bottom().
int rightOf(const QRect& r) { return r.x() + r.width(); }
int bottomOf(const QRect& r) { return r.y() + r.height(); }

QVariantMap displayMap(const DisplayConfig& d)
{
    QVariantMap map;
    map.insert(QStringLiteral("x"), d.x);
    map.insert(QStringLiteral("y"), d.y);
    map.insert(QStringLiteral("width"), d.width);
    map.insert(QStringLiteral("height"), d.height);
    map.insert(QStringLiteral("mmWidth"), d.mmWidth);
    map.insert(QStringLiteral("mmHeight"), d.mmHeight);
    return map;
}

QVariantList displayList(const QVector<DisplayConfig>& displays)
{
    QVariantList list;
    for (const DisplayConfig& d : displays) {
        list << displayMap(d);
    }
    return list;
}

int opposite(int side)
{
    switch (side) {
    case LayoutModel::Left: return LayoutModel::Right;
    case LayoutModel::Right: return LayoutModel::Left;
    case LayoutModel::Up: return LayoutModel::Down;
    default: return LayoutModel::Up;
    }
}

// The parts of one side of a desktop its displays reach, merged and in
// order, in the shared space. Elsewhere that side is a wall: the core only
// notices the cursor at the desktop's own edges, and a cursor brought in
// where no display is would jump to the nearest one.
// Displays of different scales stay apart: a link maps its two ranges
// linearly, which is only true within one scale.
QVector<QPair<int, int>> edgeSpans(const ScreenConfig& screen, int side)
{
    const QVector<QRect> rects = screen.displayRects();
    QRect box;
    for (const QRect& r : rects) box = box.isNull() ? r : box.united(r);
    struct Span { int from; int to; int scale; };
    QVector<Span> spans;
    for (int i = 0; i < rects.size(); ++i) {
        const QRect& d = rects[i];
        const int scale = screen.displayScale(i);
        switch (side) {
        case LayoutModel::Left: if (d.x() == box.x()) spans << Span{d.y(), bottomOf(d), scale}; break;
        case LayoutModel::Right: if (rightOf(d) == rightOf(box)) spans << Span{d.y(), bottomOf(d), scale}; break;
        case LayoutModel::Up: if (d.y() == box.y()) spans << Span{d.x(), rightOf(d), scale}; break;
        case LayoutModel::Down: if (bottomOf(d) == bottomOf(box)) spans << Span{d.x(), rightOf(d), scale}; break;
        default: break;
        }
    }
    std::sort(spans.begin(), spans.end(), [](const Span& a, const Span& b) { return a.from < b.from; });
    QVector<QPair<int, int>> merged;
    int lastScale = 0;
    for (const Span& span : std::as_const(spans)) {
        if (!merged.isEmpty() && span.from <= merged.last().second && span.scale == lastScale) {
            merged.last().second = qMax(merged.last().second, span.to);
        } else {
            merged << qMakePair(span.from, span.to);
            lastScale = span.scale;
        }
    }
    return merged;
}

// Where a stretch of one side, in the shared space, lies on that side in the
// computer's own pixels (what the core's ranges count, over its whole
// desktop), in percent: each end through the display that reaches the side
// there, at that display's scale.
QPair<double, double> ownRange(const ScreenConfig& screen, int side, int from, int to)
{
    const QVector<QRect> rects = screen.displayRects();
    QRect box;
    for (const QRect& r : rects) box = box.isNull() ? r : box.united(r);
    const QSize native = screen.nativeSize();
    const bool upright = side == LayoutModel::Left || side == LayoutModel::Right;
    const int whole = upright ? native.height() : native.width();
    auto reaches = [&](const QRect& r) {
        switch (side) {
        case LayoutModel::Left: return r.x() == box.x();
        case LayoutModel::Right: return rightOf(r) == rightOf(box);
        case LayoutModel::Up: return r.y() == box.y();
        default: return bottomOf(r) == bottomOf(box);
        }
    };
    auto own = [&](int at, bool end) {
        // the display holding this end: an end is exclusive, a start inclusive
        int pick = -1;
        for (int i = 0; i < rects.size(); ++i) {
            const QRect& r = rects[i];
            const int lo = upright ? r.y() : r.x(), hi = upright ? bottomOf(r) : rightOf(r);
            if (reaches(r) && (end ? (at > lo && at <= hi) : (at >= lo && at < hi))) {
                pick = i;
                break;
            }
        }
        if (pick < 0) {
            // not on a display (the uniform reading of the desktop box)
            const int lo = upright ? box.y() : box.x(), length = upright ? box.height() : box.width();
            return length <= 0 ? 0.0 : double(at - lo) * whole / length;
        }
        const DisplayConfig& d = screen.displays[pick];
        const QRect& r = rects[pick];
        return upright ? d.y + double(at - r.y()) * d.height / r.height()
                       : d.x + double(at - r.x()) * d.width / r.width();
    };
    return qMakePair(percent(own(from, false), whole), percent(own(to, true), whole));
}

} // namespace

// ---- ScreenConfig --------------------------------------------------------------

const char* ScreenConfig::modifierName(int modifier) { return kModifierNames[qBound(0, modifier, int(None))]; }
const char* ScreenConfig::cornerName(int corner) { return kCornerNames[qBound(0, corner, int(CornerCount) - 1)]; }
const char* ScreenConfig::fixName(int fix) { return kFixNames[qBound(0, fix, int(FixCount) - 1)]; }

int ScreenConfig::toShared(int pixels) const
{
    return int(std::lround(double(pixels) * scale / 1000.0));
}

QSize ScreenConfig::nativeSize() const
{
    QRect box;
    for (const DisplayConfig& d : displays) {
        box = box.isNull() ? d.rect() : box.united(d.rect());
    }
    return box.isNull() ? QSize(LayoutModel::kDefaultWidth, LayoutModel::kDefaultHeight) : box.size();
}

QRect ScreenConfig::desktop() const
{
    QRect box;
    const QVector<QRect> rects = displayRects();
    for (const QRect& r : rects) box = box.isNull() ? r : box.united(r);
    return box.isNull() ? QRect(x, y, 1, 1) : box;
}

QRect ScreenConfig::displayRect(int index) const
{
    return displayRects().value(index);
}

int ScreenConfig::displayScale(int index) const
{
    if (scaleTyped || index < 0 || index >= displays.size()) {
        return scale;
    }
    const int own = LayoutModel::measuredScale({displays[index]});
    return own > 0 ? own : scale;
}

QVector<QRect> ScreenConfig::displayRects() const
{
    QVector<QRect> rects(displays.size());
    if (displays.isEmpty()) {
        return rects;
    }
    int main = 0;
    for (int i = 1; i < displays.size(); ++i) {
        if (qint64(displays[i].width) * displays[i].height > qint64(displays[main].width) * displays[main].height) main = i;
    }
    auto shared = [](int pixels, int scale) { return int(std::lround(double(pixels) * scale / 1000.0)); };
    const DisplayConfig& m = displays[main];
    const int sm = displayScale(main);
    const QRect M(shared(m.x, sm), shared(m.y, sm), qMax(1, shared(m.width, sm)), qMax(1, shared(m.height, sm)));
    rects[main] = M;
    for (int i = 0; i < displays.size(); ++i) {
        if (i == main) continue;
        const DisplayConfig& d = displays[i];
        const int si = displayScale(i);
        const QSize size(qMax(1, shared(d.width, si)), qMax(1, shared(d.height, si)));
        // along the biggest one, offsets at its scale; a display meeting it
        // keeps meeting it, whatever its own size
        int left = M.x() + shared(d.x - m.x, sm), top = M.y() + shared(d.y - m.y, sm);
        if (d.x == m.x + m.width) left = rightOf(M);
        else if (d.x + d.width == m.x) left = M.x() - size.width();
        if (d.y == m.y + m.height) top = bottomOf(M);
        else if (d.y + d.height == m.y) top = M.y() - size.height();
        rects[i] = QRect(QPoint(left, top), size);
    }
    QRect box;
    for (const QRect& r : std::as_const(rects)) box = box.isNull() ? r : box.united(r);
    const QPoint shift = QPoint(x, y) - box.topLeft();
    for (QRect& r : rects) r.translate(shift);
    return rects;
}

void ScreenConfig::normaliseDisplays()
{
    for (DisplayConfig& d : displays) {
        d.width = qBound(1, d.width, kMaxDimension);
        d.height = qBound(1, d.height, kMaxDimension);
        d.x = qBound(-kMaxDimension, d.x, kMaxDimension);
        d.y = qBound(-kMaxDimension, d.y, kMaxDimension);
    }
    if (displays.isEmpty()) {
        displays << DisplayConfig();
    }
    int minX = displays[0].x, minY = displays[0].y;
    for (const DisplayConfig& d : displays) {
        minX = qMin(minX, d.x);
        minY = qMin(minY, d.y);
    }
    // the displays that were there stay where they were on the desk: the
    // desktop's origin moves instead
    x += toShared(minX);
    y += toShared(minY);
    for (DisplayConfig& d : displays) {
        d.x -= minX;
        d.y -= minY;
    }
}

// ---- model ---------------------------------------------------------------------

LayoutModel::LayoutModel(QSettings* store, QObject* parent) :
    QAbstractListModel(parent),
    m_store(store)
{
    if (m_store != nullptr) {
        load();
    }
}

int LayoutModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_screens.size();
}

QVariant LayoutModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_screens.size()) {
        return QVariant();
    }
    const ScreenConfig& s = m_screens[index.row()];
    const QRect desktop = s.desktop();
    switch (role) {
    case NameRole: return s.name;
    case XRole: return desktop.x();
    case YRole: return desktop.y();
    case WidthRole: return desktop.width();
    case HeightRole: return desktop.height();
    case DisplaysRole: {
        // in the shared space, from the desktop's corner: what the canvas draws
        QVariantList list;
        for (int i = 0; i < s.displays.size(); ++i) {
            const QRect r = s.displayRect(i).translated(-desktop.topLeft());
            list << QVariantMap{{QStringLiteral("x"), r.x()}, {QStringLiteral("y"), r.y()},
                                {QStringLiteral("width"), r.width()}, {QStringLiteral("height"), r.height()}};
        }
        return list;
    }
    case IsServerRole: return s.name == m_serverName;
    case LinkedRole: return s.name == m_serverName || !linksOf(index.row()).isEmpty();
    case StateRole: return m_liveStates.value(s.name);
    case PositionRole: return positionText(index.row());
    default: return QVariant();
    }
}

QHash<int, QByteArray> LayoutModel::roleNames() const
{
    // not x/y/width/height: those would clash with the delegate Item's own
    return {{NameRole, "name"}, {XRole, "desktopX"}, {YRole, "desktopY"}, {WidthRole, "desktopWidth"},
            {HeightRole, "desktopHeight"}, {DisplaysRole, "displays"}, {IsServerRole, "isServer"},
            {LinkedRole, "linked"}, {StateRole, "state"}, {PositionRole, "position"}};
}

void LayoutModel::changed(int first, int last)
{
    if (!m_screens.isEmpty()) {
        // links and positions depend on every other desktop: refresh all rows
        emit dataChanged(index(0), index(m_screens.size() - 1));
    }
    Q_UNUSED(first);
    Q_UNUSED(last);
    emit layoutChanged();
}

void LayoutModel::setServerName(const QString& name)
{
    if (m_serverName == name) {
        return;
    }
    const int old = indexOf(m_serverName);
    m_serverName = name;
    if (old >= 0 && !name.isEmpty() && indexOf(name) < 0) {
        m_screens[old].name = name;   // the server was renamed: keep its desktop and options
        save();
    }
    changed(0, m_screens.size() - 1);
}

QStringList LayoutModel::screenNames() const
{
    QStringList names;
    for (const ScreenConfig& s : m_screens) {
        names << s.name;
    }
    return names;
}

QRect LayoutModel::bounds() const
{
    QRect box;
    for (const ScreenConfig& s : m_screens) {
        box = box.isNull() ? s.desktop() : box.united(s.desktop());
    }
    return box;
}

// ---- computers -----------------------------------------------------------------

int LayoutModel::indexOf(const QString& name) const
{
    if (name.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < m_screens.size(); ++i) {
        if (m_screens[i].name.compare(name, Qt::CaseInsensitive) == 0) {
            return i;   // the core matches names without regard to case
        }
    }
    return -1;
}

bool LayoutModel::overlapsAnother(int index, const QRect& rect) const
{
    for (int i = 0; i < m_screens.size(); ++i) {
        if (i != index && m_screens[i].desktop().intersects(rect)) {
            return true;
        }
    }
    return false;
}

// What a computer reported, matched without regard to case like the core
// does (a Mac may report "MacBooKPro-P" for the file's "MacBookPro-P").
QVector<DisplayConfig> LayoutModel::reportedFor(const QString& name) const
{
    for (auto it = m_reported.cbegin(); it != m_reported.cend(); ++it) {
        if (it.key().compare(name, Qt::CaseInsensitive) == 0) {
            return it.value();
        }
    }
    return {};
}

// A computer about to be added: with the displays it reported, if any.
ScreenConfig LayoutModel::newScreen(const QString& name) const
{
    ScreenConfig screen;
    screen.name = name;
    const QVector<DisplayConfig> reported = reportedFor(name);
    if (!reported.isEmpty()) {
        screen.displays = reported;
        screen.normaliseDisplays();
        const int measured = measuredScale(screen.displays);
        if (measured > 0) {
            screen.scale = measured;
        }
        screen.displaysReported = true;
    }
    return screen;
}

bool LayoutModel::addScreen(const QString& name, int x, int y)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || hasScreen(trimmed)) {
        return false;
    }
    ScreenConfig screen = newScreen(trimmed);
    screen.x = x;
    screen.y = y;
    if (overlapsAnother(-1, screen.desktop())) {
        return false;
    }
    beginInsertRows(QModelIndex(), m_screens.size(), m_screens.size());
    m_screens << screen;
    endInsertRows();
    save();
    changed(0, m_screens.size() - 1);
    return true;
}

int LayoutModel::adoptScreens(const LayoutModel& other)
{
    // where `other` puts its desktops, moved so its server sits on ours
    QPoint shift;
    const int ours = indexOf(m_serverName), theirs = other.indexOf(m_serverName);
    if (ours >= 0 && theirs >= 0) {
        shift = QPoint(m_screens[ours].x - other.m_screens[theirs].x, m_screens[ours].y - other.m_screens[theirs].y);
    }
    int adopted = 0;
    for (const ScreenConfig& s : other.m_screens) {
        if (hasScreen(s.name)) {
            continue;
        }
        ScreenConfig screen = s;   // with its options
        const QVector<DisplayConfig> reported = reportedFor(s.name);
        if (!reported.isEmpty()) {
            screen.displays = reported;
            screen.displaysReported = true;
        }
        screen.x += shift.x();
        screen.y += shift.y();
        if (overlapsAnother(-1, screen.desktop())) {
            const QRect box = bounds();
            screen.x = rightOf(box);
            screen.y = box.y();
        }
        beginInsertRows(QModelIndex(), m_screens.size(), m_screens.size());
        m_screens << screen;
        endInsertRows();
        ++adopted;
    }
    if (adopted > 0) {
        save();
        changed(0, m_screens.size() - 1);
    }
    return adopted;
}

bool LayoutModel::addScreenAnywhere(const QString& name)
{
    if (m_screens.isEmpty()) {
        return addScreen(name, 0, 0);
    }
    // to the right of everything, level with the server (or the top)
    const QRect box = bounds();
    const int server = indexOf(m_serverName);
    const int y = server >= 0 ? m_screens[server].desktop().y() : box.y();
    return addScreen(name, rightOf(box), y);
}

bool LayoutModel::placeNextTo(const QString& name, const QString& anchor, int direction)
{
    const int start = indexOf(anchor);
    if (start < 0 || hasScreen(name.trimmed()) || name.trimmed().isEmpty()) {
        return false;
    }
    const ScreenConfig probe = newScreen(name.trimmed());
    const QSize size = probe.desktop().size();
    QRect beside = m_screens[start].desktop();
    // Walk along the direction past whatever is in the way.
    for (int attempt = 0; attempt < 64; ++attempt) {
        QRect candidate;
        switch (direction) {
        case Left: candidate = QRect(beside.x() - size.width(), beside.y(), size.width(), size.height()); break;
        case Right: candidate = QRect(rightOf(beside), beside.y(), size.width(), size.height()); break;
        case Up: candidate = QRect(beside.x(), beside.y() - size.height(), size.width(), size.height()); break;
        case Down: candidate = QRect(beside.x(), bottomOf(beside), size.width(), size.height()); break;
        default: return false;
        }
        int blocker = -1;
        for (int i = 0; i < m_screens.size(); ++i) {
            if (m_screens[i].desktop().intersects(candidate)) {
                blocker = i;
                break;
            }
        }
        if (blocker < 0) {
            return addScreen(probe.name, candidate.x(), candidate.y());
        }
        beside = m_screens[blocker].desktop();
        // keep the row/column of the anchor so the chain stays straight
        if (direction == Left || direction == Right) {
            beside.moveTop(m_screens[start].desktop().y());
        } else {
            beside.moveLeft(m_screens[start].desktop().x());
        }
    }
    return false;
}

bool LayoutModel::moveTo(int index, int x, int y)
{
    if (index < 0 || index >= m_screens.size()) {
        return false;
    }
    ScreenConfig& s = m_screens[index];
    x = qBound(-kMaxDimension, x, kMaxDimension);
    y = qBound(-kMaxDimension, y, kMaxDimension);
    if (s.x == x && s.y == y) {
        return true;
    }
    QRect target = s.desktop();
    target.moveTo(x, y);
    if (overlapsAnother(index, target)) {
        return false;
    }
    s.x = x;
    s.y = y;
    save();
    changed(0, m_screens.size() - 1);
    return true;
}

QVariantMap LayoutModel::snapPosition(int index, int x, int y, int threshold) const
{
    QVariantMap out;
    out.insert(QStringLiteral("x"), x);
    out.insert(QStringLiteral("y"), y);
    out.insert(QStringLiteral("snapped"), false);
    if (index < 0 || index >= m_screens.size()) {
        return out;
    }
    const QRect me = m_screens[index].desktop();
    const int w = me.width(), h = me.height();
    threshold = qMax(0, threshold);

    // Nearest edge-to-edge contact or alignment on each axis, considering
    // only the desktops the moving one would be level with.
    int bestX = x, bestY = y;
    int bestDx = threshold + 1, bestDy = threshold + 1;
    for (int i = 0; i < m_screens.size(); ++i) {
        if (i == index) continue;
        const QRect other = m_screens[i].desktop();
        const bool levelY = y < bottomOf(other) + threshold && y + h > other.y() - threshold;
        const bool levelX = x < rightOf(other) + threshold && x + w > other.x() - threshold;
        // contact with the desktop, or lined up with one of its displays
        // (an edge, or centred on it)
        QVector<int> xs{rightOf(other), other.x() - w, other.x(), rightOf(other) - w};
        QVector<int> ys{bottomOf(other), other.y() - h, other.y(), bottomOf(other) - h};
        for (int k = 0; k < m_screens[i].displays.size(); ++k) {
            const QRect r = m_screens[i].displayRect(k);
            xs << r.x() << rightOf(r) - w << r.x() + (r.width() - w) / 2;
            ys << r.y() << bottomOf(r) - h << r.y() + (r.height() - h) / 2;
        }
        if (levelY) {
            for (int c : std::as_const(xs)) {
                const int d = std::abs(c - x);
                if (d < bestDx) { bestDx = d; bestX = c; }
            }
        }
        if (levelX) {
            for (int c : std::as_const(ys)) {
                const int d = std::abs(c - y);
                if (d < bestDy) { bestDy = d; bestY = c; }
            }
        }
    }
    out.insert(QStringLiteral("x"), bestX);
    out.insert(QStringLiteral("y"), bestY);
    out.insert(QStringLiteral("snapped"), bestX != x || bestY != y);
    return out;
}

void LayoutModel::removeScreen(int index)
{
    if (index < 0 || index >= m_screens.size()) {
        return;
    }
    const QString gone = m_screens[index].name;
    m_gapLinks.erase(std::remove_if(m_gapLinks.begin(), m_gapLinks.end(), [&gone](const GapLink& g) {
        return g.from.compare(gone, Qt::CaseInsensitive) == 0 || g.to.compare(gone, Qt::CaseInsensitive) == 0;
    }), m_gapLinks.end());
    beginRemoveRows(QModelIndex(), index, index);
    m_screens.removeAt(index);
    endRemoveRows();
    save();
    changed(0, m_screens.size() - 1);
}

bool LayoutModel::renameScreen(int index, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (index < 0 || index >= m_screens.size() || trimmed.isEmpty()) {
        return false;
    }
    const int other = indexOf(trimmed);
    if (other >= 0 && other != index) {
        return false;
    }
    if (m_screens[index].name == m_serverName) {
        m_serverName = trimmed;
    }
    for (GapLink& g : m_gapLinks) {
        if (g.from.compare(m_screens[index].name, Qt::CaseInsensitive) == 0) g.from = trimmed;
        if (g.to.compare(m_screens[index].name, Qt::CaseInsensitive) == 0) g.to = trimmed;
    }
    m_screens[index].name = trimmed;
    save();
    changed(0, m_screens.size() - 1);
    return true;
}

QVariantMap LayoutModel::screenSettings(int index) const
{
    QVariantMap map;
    if (index < 0 || index >= m_screens.size()) {
        return map;
    }
    const ScreenConfig& s = m_screens[index];
    const QRect desktop = s.desktop();
    map.insert(QStringLiteral("name"), s.name);
    map.insert(QStringLiteral("x"), desktop.x());
    map.insert(QStringLiteral("y"), desktop.y());
    map.insert(QStringLiteral("width"), s.nativeSize().width());     // its own pixels, like the displays
    map.insert(QStringLiteral("height"), s.nativeSize().height());
    map.insert(QStringLiteral("displays"), displayList(s.displays));
    const QVector<DisplayConfig> reported = m_reported.value(s.name);
    map.insert(QStringLiteral("displaysReported"), s.displaysReported);
    map.insert(QStringLiteral("displaysTyped"), s.displaysTyped);
    map.insert(QStringLiteral("scale"), s.scale / 10.0);
    map.insert(QStringLiteral("scaleTyped"), s.scaleTyped);
    map.insert(QStringLiteral("measuredScale"), measuredScaleOf(index) / 10.0);
    map.insert(QStringLiteral("reportedDisplays"), displayList(reported));
    map.insert(QStringLiteral("reportDiffers"), !reported.isEmpty() && reported != s.displays);
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
    map.insert(QStringLiteral("position"), positionText(index));
    map.insert(QStringLiteral("linked"), s.name == m_serverName || !linksOf(index).isEmpty());
    return map;
}

bool LayoutModel::setScreenSettings(int index, const QVariantMap& settings)
{
    if (index < 0 || index >= m_screens.size()) {
        return false;
    }
    ScreenConfig& s = m_screens[index];
    if (settings.contains(QStringLiteral("displays"))) {
        QVector<DisplayConfig> displays;
        for (const QVariant& v : settings.value(QStringLiteral("displays")).toList()) {
            const QVariantMap m = v.toMap();
            DisplayConfig d;
            d.x = m.value(QStringLiteral("x"), 0).toInt();
            d.y = m.value(QStringLiteral("y"), 0).toInt();
            d.width = m.value(QStringLiteral("width"), kDefaultWidth).toInt();
            d.height = m.value(QStringLiteral("height"), kDefaultHeight).toInt();
            displays << d;
        }
        ScreenConfig probe = s;
        probe.displays = displays;
        probe.normaliseDisplays();
        if (overlapsAnother(index, probe.desktop())) {
            return false;   // a bigger desktop would run into a neighbour
        }
        s.displays = probe.displays;
        s.x = probe.x;      // normalising may have moved the origin
        s.y = probe.y;
        s.displaysReported = false;
        s.displaysTyped = true;       // reports no longer replace them
    }
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
    changed(0, m_screens.size() - 1);
    return true;
}

bool LayoutModel::addDisplay(int index)
{
    if (index < 0 || index >= m_screens.size()) {
        return false;
    }
    QVariantMap settings;
    QVector<DisplayConfig> displays = m_screens[index].displays;
    DisplayConfig next = displays.isEmpty() ? DisplayConfig() : displays.last();
    next.x = displays.isEmpty() ? 0 : next.x + next.width;
    displays << next;
    settings.insert(QStringLiteral("displays"), displayList(displays));
    return setScreenSettings(index, settings);
}

bool LayoutModel::removeDisplay(int index, int display)
{
    if (index < 0 || index >= m_screens.size() || display < 0 || display >= m_screens[index].displays.size() ||
        m_screens[index].displays.size() == 1) {
        return false;
    }
    QVector<DisplayConfig> displays = m_screens[index].displays;
    displays.removeAt(display);
    QVariantMap settings;
    settings.insert(QStringLiteral("displays"), displayList(displays));
    return setScreenSettings(index, settings);
}

// ---- displays reported by the computers -----------------------------------------

// Reported displays for one desktop, and the scale their real sizes give
// unless one was typed.
bool LayoutModel::applyDisplays(int index, const QVector<DisplayConfig>& displays)
{
    ScreenConfig probe = m_screens[index];
    probe.displays = displays;
    probe.normaliseDisplays();
    const int measured = probe.scaleTyped ? 0 : measuredScale(probe.displays);
    if (measured > 0) {
        probe.scale = measured;
    }
    probe.displaysReported = true;
    probe.displaysTyped = false;
    return applyGeometry(index, probe);
}

// A desktop's new displays or scale, its origin kept. The desktops wholly
// right of its old right edge move by the change in width, those wholly
// below its old bottom edge by the change in height, so neighbours keep
// touching; when that still overlaps something, the new size alone is tried.
bool LayoutModel::applyGeometry(int index, const ScreenConfig& probe)
{
    const QRect before = m_screens[index].desktop();
    const QRect after = probe.desktop();
    const int dw = after.width() - before.width();
    const int dh = after.height() - before.height();

    auto fits = [&](const QVector<QPoint>& origins) {
        for (int i = 0; i < m_screens.size(); ++i) {
            const QRect a = i == index ? after : m_screens[i].desktop().translated(origins[i] - QPoint(m_screens[i].x, m_screens[i].y));
            for (int j = i + 1; j < m_screens.size(); ++j) {
                const QRect b = j == index ? after : m_screens[j].desktop().translated(origins[j] - QPoint(m_screens[j].x, m_screens[j].y));
                if (a.intersects(b)) {
                    return false;
                }
            }
        }
        return true;
    };

    QVector<QPoint> unchanged, pushed;
    for (int i = 0; i < m_screens.size(); ++i) {
        const QPoint origin(m_screens[i].x, m_screens[i].y);
        unchanged << origin;
        const QRect d = m_screens[i].desktop();
        QPoint moved = origin;
        if (i != index && dw != 0 && d.x() >= rightOf(before)) moved.rx() += dw;
        if (i != index && dh != 0 && d.y() >= bottomOf(before)) moved.ry() += dh;
        pushed << moved;
    }
    QVector<QPoint> origins;
    if (fits(pushed)) {
        origins = pushed;
    } else if (fits(unchanged)) {
        origins = unchanged;
    } else {
        return false;
    }
    for (int i = 0; i < m_screens.size(); ++i) {
        if (i != index) {
            m_screens[i].x = origins[i].x();
            m_screens[i].y = origins[i].y();
        }
    }
    m_screens[index] = probe;
    save();
    changed(0, m_screens.size() - 1);
    return true;
}

bool LayoutModel::reportDisplays(const QString& name, const QVector<DisplayConfig>& displays)
{
    const QVector<DisplayConfig> clean = cleanDisplays(displays);
    if (name.isEmpty() || clean.isEmpty()) {
        return false;
    }
    const bool news = m_reported.value(name) != clean;
    m_reported.insert(name, clean);
    const int index = indexOf(name);
    if (index < 0) {
        return false;
    }
    const ScreenConfig& s = m_screens[index];
    if (s.displaysTyped) {
        if (news) {
            changed(index, index);   // the panel offers the reported ones
        }
        return false;
    }
    const int measured = s.scaleTyped ? 0 : measuredScale(clean);
    if (s.displays == clean && s.displaysReported && (measured == 0 || measured == s.scale)) {
        return false;
    }
    // other monitors than before (not a first report): another setup
    const bool otherMonitors = s.displaysReported && s.displays != clean && m_store != nullptr;
    if (otherMonitors && restoreSetup(index, clean)) {
        return true;
    }
    const int previous = m_activeSetup;
    if (otherMonitors) {
        m_activeSetup = -1;
    }
    if (!applyDisplays(index, clean)) {
        m_activeSetup = previous;
        changed(index, index);       // does not fit: the panel says so
        return false;
    }
    return true;
}

bool LayoutModel::useReportedDisplays(int index)
{
    if (index < 0 || index >= m_screens.size()) {
        return false;
    }
    const QVector<DisplayConfig> reported = m_reported.value(m_screens[index].name);
    if (reported.isEmpty()) {
        return false;
    }
    return applyDisplays(index, reported);
}

bool LayoutModel::setScale(int index, double percent)
{
    if (index < 0 || index >= m_screens.size() || !std::isfinite(percent)) {
        return false;
    }
    ScreenConfig probe = m_screens[index];
    probe.scale = qBound(kMinScale, int(std::lround(percent * 10)), kMaxScale);
    probe.scaleTyped = true;
    return applyGeometry(index, probe);
}

bool LayoutModel::useMeasuredScale(int index)
{
    if (index < 0 || index >= m_screens.size()) {
        return false;
    }
    const int measured = measuredScaleOf(index);
    if (measured == 0) {
        return false;
    }
    ScreenConfig probe = m_screens[index];
    probe.scale = measured;
    probe.scaleTyped = false;
    return applyGeometry(index, probe);
}

int LayoutModel::measuredScale(const QVector<DisplayConfig>& displays)
{
    // the biggest display that knows its size speaks for the desktop
    const DisplayConfig* best = nullptr;
    for (const DisplayConfig& d : displays) {
        if (d.mmWidth <= 0 || d.mmHeight <= 0 || d.width <= 0 || d.height <= 0) continue;
        if (best == nullptr || qint64(d.width) * d.height > qint64(best->width) * best->height) {
            best = &d;
        }
    }
    if (best == nullptr) {
        return 0;
    }
    // millimetres per pixel over both axes, against a 96-dpi pixel
    const double mmPerPixel = (double(best->mmWidth) / best->width + double(best->mmHeight) / best->height) / 2;
    const int scale = int(std::lround(mmPerPixel / (25.4 / 96) * 1000));
    return scale >= kMinScale && scale <= kMaxScale ? scale : 0;   // else an aspect ratio posing as a size
}

int LayoutModel::measuredScaleOf(int index) const
{
    const int live = measuredScale(m_reported.value(m_screens[index].name));
    return live > 0 ? live : measuredScale(m_screens[index].displays);
}

// What a computer reports, made safe: sizes and offsets clamped, empty
// displays dropped, a few at most, the bounding box moved to (0, 0).
QVector<DisplayConfig> LayoutModel::cleanDisplays(const QVector<DisplayConfig>& reported)
{
    QVector<DisplayConfig> displays;
    for (const DisplayConfig& d : reported) {
        if (displays.size() == kMaxReportedDisplays) {
            break;
        }
        if (d.width <= 0 || d.height <= 0) {
            continue;
        }
        DisplayConfig clean;
        clean.width = qMin(d.width, kMaxDimension);
        clean.height = qMin(d.height, kMaxDimension);
        clean.x = qBound(-kMaxDimension, d.x, kMaxDimension);
        clean.y = qBound(-kMaxDimension, d.y, kMaxDimension);
        if (d.mmWidth > 0 && d.mmHeight > 0) {
            clean.mmWidth = qMin(d.mmWidth, 10000);
            clean.mmHeight = qMin(d.mmHeight, 10000);
        }
        displays << clean;
    }
    if (displays.isEmpty()) {
        return displays;
    }
    int minX = displays[0].x, minY = displays[0].y;
    for (const DisplayConfig& d : displays) {
        minX = qMin(minX, d.x);
        minY = qMin(minY, d.y);
    }
    for (DisplayConfig& d : displays) {
        d.x -= minX;
        d.y -= minY;
    }
    return displays;
}

QJsonArray LayoutModel::displaysToJson(const QVector<DisplayConfig>& displays)
{
    QJsonArray json;
    for (const DisplayConfig& d : displays) {
        json << QJsonObject::fromVariantMap(displayMap(d));
    }
    return json;
}

QVector<DisplayConfig> LayoutModel::displaysFromJson(const QJsonArray& json)
{
    QVector<DisplayConfig> displays;
    for (const QJsonValue& value : json) {
        const QJsonObject o = value.toObject();
        DisplayConfig d;
        d.x = o.value(QStringLiteral("x")).toInt();
        d.y = o.value(QStringLiteral("y")).toInt();
        d.width = o.value(QStringLiteral("width")).toInt();
        d.height = o.value(QStringLiteral("height")).toInt();
        d.mmWidth = o.value(QStringLiteral("mmWidth")).toInt();
        d.mmHeight = o.value(QStringLiteral("mmHeight")).toInt();
        displays << d;
        if (displays.size() > kMaxReportedDisplays * 4) {
            break;   // cleanReport keeps the first few that are usable
        }
    }
    return cleanDisplays(displays);
}

QString LayoutModel::displaysToText(const QVector<DisplayConfig>& displays, bool withSizes)
{
    QStringList parts;
    for (const DisplayConfig& d : displays) {
        QString part = QStringLiteral("%1,%2,%3x%4").arg(d.x).arg(d.y).arg(d.width).arg(d.height);
        if (withSizes && d.mmWidth > 0 && d.mmHeight > 0) {
            part += QStringLiteral("@%1x%2").arg(d.mmWidth).arg(d.mmHeight);
        }
        parts << part;
    }
    return parts.join(QLatin1Char(';'));
}

QVector<DisplayConfig> LayoutModel::displaysFromText(const QString& text)
{
    static const QRegularExpression pattern(QStringLiteral("^(-?\\d{1,6}),(-?\\d{1,6}),(\\d{1,6})x(\\d{1,6})(?:@(\\d{1,5})x(\\d{1,5}))?$"));
    QVector<DisplayConfig> displays;
    const QStringList parts = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QRegularExpressionMatch m = pattern.match(part.trimmed());
        if (!m.hasMatch()) {
            continue;
        }
        DisplayConfig d;
        d.x = m.captured(1).toInt();
        d.y = m.captured(2).toInt();
        d.width = m.captured(3).toInt();
        d.height = m.captured(4).toInt();
        d.mmWidth = m.captured(5).toInt();    // 0 when absent
        d.mmHeight = m.captured(6).toInt();
        displays << d;
        if (displays.size() > kMaxReportedDisplays * 4) {
            break;
        }
    }
    return cleanDisplays(displays);
}

// Two desktops whose edges touch are linked where a display of each reaches
// the touching edges; the rest of the edge is a wall. The core maps a
// crossing along each link proportionally, which is the real geometry once
// both desktops are drawn to scale.
QVector<ScreenLink> LayoutModel::links() const
{
    const QVector<ScreenLink> touching = touchingLinks();
    QVector<ScreenLink> links = touching;
    for (const GapLink& g : m_gapLinks) {
        const int a = indexOf(g.from), b = indexOf(g.to);
        if (a < 0 || b < 0 || a == b) continue;
        const int back = opposite(g.side);
        const QVector<QPair<int, int>> mine = freeSpans(a, g.side, touching);
        const QVector<QPair<int, int>> theirs = freeSpans(b, back, touching);
        if (mine.isEmpty() || theirs.isEmpty()) continue;
        // the free stretch of each edge, end to end
        const QPair<double, double> ra = ownRange(m_screens[a], g.side, mine.first().first, mine.last().second);
        const QPair<double, double> rb = ownRange(m_screens[b], back, theirs.first().first, theirs.last().second);
        ScreenLink there;
        there.from = a;
        there.to = b;
        there.side = g.side;
        there.fromStart = ra.first;
        there.fromEnd = ra.second;
        there.toStart = rb.first;
        there.toEnd = rb.second;
        there.gap = true;
        if (there.fromStart >= there.fromEnd || there.toStart >= there.toEnd) continue;
        ScreenLink returning = there;
        returning.from = b;
        returning.to = a;
        returning.side = back;
        std::swap(returning.fromStart, returning.toStart);
        std::swap(returning.fromEnd, returning.toEnd);
        links << there << returning;
    }
    return links;
}

// The parts of an edge where a display reaches it and no touching neighbour
// is linked: what a link across a gap may use.
QVector<QPair<int, int>> LayoutModel::freeSpans(int index, int side, const QVector<ScreenLink>& touching) const
{
    QVector<QPair<int, int>> open = edgeSpans(m_screens[index], side);
    const bool upright = side == Left || side == Right;
    for (const ScreenLink& link : touching) {
        if (link.from != index || link.side != side) continue;
        const int from = upright ? qMin(link.line.y1(), link.line.y2()) : qMin(link.line.x1(), link.line.x2());
        const int to = upright ? qMax(link.line.y1(), link.line.y2()) : qMax(link.line.x1(), link.line.x2());
        QVector<QPair<int, int>> rest;
        for (const auto& span : std::as_const(open)) {
            if (to <= span.first || from >= span.second) {
                rest << span;
                continue;
            }
            if (span.first < from) rest << qMakePair(span.first, from);
            if (to < span.second) rest << qMakePair(to, span.second);
        }
        open = rest;
    }
    return open;
}

bool LayoutModel::addGapLink(int index, int side, int other)
{
    if (index < 0 || other < 0 || index >= m_screens.size() || other >= m_screens.size() || index == other
        || side < Left || side > Down) {
        return false;
    }
    const QString from = m_screens[index].name, to = m_screens[other].name;
    for (const GapLink& g : std::as_const(m_gapLinks)) {
        const bool same = g.from.compare(from, Qt::CaseInsensitive) == 0 && g.side == side && g.to.compare(to, Qt::CaseInsensitive) == 0;
        const bool reverse = g.from.compare(to, Qt::CaseInsensitive) == 0 && g.side == opposite(side) && g.to.compare(from, Qt::CaseInsensitive) == 0;
        if (same || reverse) {
            return false;
        }
    }
    m_gapLinks << GapLink{from, side, to};
    save();
    changed(0, m_screens.size() - 1);
    return true;
}

void LayoutModel::removeGapLink(int index, int side, int other)
{
    if (index < 0 || other < 0 || index >= m_screens.size() || other >= m_screens.size()) {
        return;
    }
    const QString from = m_screens[index].name, to = m_screens[other].name;
    const int before = m_gapLinks.size();
    m_gapLinks.erase(std::remove_if(m_gapLinks.begin(), m_gapLinks.end(), [&](const GapLink& g) {
        const bool same = g.from.compare(from, Qt::CaseInsensitive) == 0 && g.side == side && g.to.compare(to, Qt::CaseInsensitive) == 0;
        const bool reverse = g.from.compare(to, Qt::CaseInsensitive) == 0 && g.side == opposite(side) && g.to.compare(from, Qt::CaseInsensitive) == 0;
        return same || reverse;
    }), m_gapLinks.end());
    if (m_gapLinks.size() != before) {
        save();
        changed(0, m_screens.size() - 1);
    }
}

QVariantList LayoutModel::gapLinksOf(int index) const
{
    QVariantList list;
    if (index < 0 || index >= m_screens.size()) {
        return list;
    }
    const QString name = m_screens[index].name;
    for (const GapLink& g : m_gapLinks) {
        int side = -1;
        QString other;
        if (g.from.compare(name, Qt::CaseInsensitive) == 0) { side = g.side; other = g.to; }
        else if (g.to.compare(name, Qt::CaseInsensitive) == 0) { side = opposite(g.side); other = g.from; }
        const int otherIndex = indexOf(other);
        if (side < 0 || otherIndex < 0) continue;
        list << QVariantMap{{QStringLiteral("side"), side}, {QStringLiteral("name"), m_screens[otherIndex].name}, {QStringLiteral("other"), otherIndex}};
    }
    return list;
}

QVariantList LayoutModel::gapCrossings() const
{
    QVariantList list;
    for (const ScreenLink& link : links()) {
        if (!link.gap || link.from > link.to) continue;   // once per link
        auto point = [this](int index, int side, double start, double end) {
            const QRect r = m_screens[index].desktop();
            const double mid = (start + end) / 200.0;
            switch (side) {
            case Left: return QPoint(r.x(), r.y() + int(r.height() * mid));
            case Right: return QPoint(rightOf(r), r.y() + int(r.height() * mid));
            case Up: return QPoint(r.x() + int(r.width() * mid), r.y());
            default: return QPoint(r.x() + int(r.width() * mid), bottomOf(r));
            }
        };
        const QPoint a = point(link.from, link.side, link.fromStart, link.fromEnd);
        const QPoint b = point(link.to, opposite(link.side), link.toStart, link.toEnd);
        list << QVariantMap{{QStringLiteral("x1"), a.x()}, {QStringLiteral("y1"), a.y()},
                            {QStringLiteral("x2"), b.x()}, {QStringLiteral("y2"), b.y()}};
    }
    return list;
}

QJsonArray LayoutModel::gapLinksJson() const
{
    QJsonArray json;
    for (const GapLink& g : m_gapLinks) {
        QJsonObject o;
        o.insert(QStringLiteral("from"), g.from);
        o.insert(QStringLiteral("side"), g.side);
        o.insert(QStringLiteral("to"), g.to);
        json << o;
    }
    return json;
}

void LayoutModel::loadGapLinks(const QJsonArray& json)
{
    m_gapLinks.clear();
    for (const QJsonValue& value : json) {
        const QJsonObject o = value.toObject();
        GapLink g{o.value(QStringLiteral("from")).toString(), o.value(QStringLiteral("side")).toInt(-1), o.value(QStringLiteral("to")).toString()};
        if (g.from.isEmpty() || g.to.isEmpty() || g.side < Left || g.side > Down || m_gapLinks.size() >= 64) continue;
        m_gapLinks << g;
    }
}

QVector<ScreenLink> LayoutModel::touchingLinks() const
{
    QVector<ScreenLink> links;
    for (int i = 0; i < m_screens.size(); ++i) {
        const QRect a = m_screens[i].desktop();
        for (int j = 0; j < m_screens.size(); ++j) {
            if (i == j) continue;
            const QRect b = m_screens[j].desktop();
            int side, edge;
            if (rightOf(a) == b.x()) { side = Right; edge = b.x(); }
            else if (a.x() == rightOf(b)) { side = Left; edge = a.x(); }
            else if (bottomOf(a) == b.y()) { side = Down; edge = b.y(); }
            else if (a.y() == bottomOf(b)) { side = Up; edge = a.y(); }
            else continue;
            const bool upright = side == Left || side == Right;   // the edge runs top to bottom
            const QVector<QPair<int, int>> mine = edgeSpans(m_screens[i], side);
            const QVector<QPair<int, int>> theirs = edgeSpans(m_screens[j], opposite(side));
            for (const auto& m : mine) {
                for (const auto& t : theirs) {
                    const int from = qMax(m.first, t.first), to = qMin(m.second, t.second);
                    if (to <= from) continue;
                    ScreenLink link;
                    link.from = i;
                    link.to = j;
                    link.side = side;
                    const QPair<double, double> ra = ownRange(m_screens[i], side, from, to);
                    const QPair<double, double> rb = ownRange(m_screens[j], opposite(side), from, to);
                    link.fromStart = ra.first;
                    link.fromEnd = ra.second;
                    link.toStart = rb.first;
                    link.toEnd = rb.second;
                    if (link.fromStart >= link.fromEnd || link.toStart >= link.toEnd) {
                        continue;   // too thin to mean anything once rounded
                    }
                    link.line = upright ? QLine(edge, from, edge, to) : QLine(from, edge, to, edge);
                    links << link;
                }
            }
        }
    }
    return links;
}

QVariantList LayoutModel::crossings() const
{
    QVariantList list;
    for (const ScreenLink& link : links()) {
        if (link.gap || link.from > link.to) continue;   // the way back crosses at the same place
        list << QVariantMap{{QStringLiteral("x1"), link.line.x1()}, {QStringLiteral("y1"), link.line.y1()},
                            {QStringLiteral("x2"), link.line.x2()}, {QStringLiteral("y2"), link.line.y2()}};
    }
    return list;
}

QVariantList LayoutModel::linksOf(int index) const
{
    QVariantList list;
    for (const ScreenLink& link : links()) {
        if (link.from != index) continue;
        QVariantMap map;
        map.insert(QStringLiteral("name"), m_screens[link.to].name);
        map.insert(QStringLiteral("side"), link.side);
        map.insert(QStringLiteral("fromStart"), link.fromStart);
        map.insert(QStringLiteral("fromEnd"), link.fromEnd);
        map.insert(QStringLiteral("toStart"), link.toStart);
        map.insert(QStringLiteral("toEnd"), link.toEnd);
        list << map;
    }
    return list;
}

QString LayoutModel::positionText(int index) const
{
    if (index < 0 || index >= m_screens.size()) {
        return QString();
    }
    if (m_screens[index].name == m_serverName) {
        return tr("centre");
    }
    const QVector<ScreenLink> all = links();
    const ScreenLink* chosen = nullptr;
    for (const ScreenLink& link : all) {
        if (link.from != index) continue;
        if (chosen == nullptr || m_screens[link.to].name == m_serverName) {
            chosen = &link;   // the server wins when it is a neighbour
        }
    }
    if (chosen == nullptr) {
        return tr("not linked");
    }
    const QString name = m_screens[chosen->to].name;
    switch (chosen->side) {
    case Right: return tr("left of %1").arg(name);
    case Left: return tr("right of %1").arg(name);
    case Down: return tr("above %1").arg(name);
    case Up: return tr("below %1").arg(name);
    default: return QString();
    }
}

void LayoutModel::setLiveState(const QString& name, const QString& state)
{
    if (m_liveStates.value(name) == state) {
        return;
    }
    m_liveStates.insert(name, state);
    const int i = indexOf(name);
    if (i >= 0) {
        emit dataChanged(index(i), index(i), {StateRole});
    }
}

void LayoutModel::clearLiveStates()
{
    m_liveStates.clear();
    if (!m_screens.isEmpty()) {
        emit dataChanged(index(0), index(m_screens.size() - 1), {StateRole});
    }
}

void LayoutModel::ensureServerPlaced()
{
    if (m_serverName.isEmpty() || hasScreen(m_serverName)) {
        return;
    }
    addScreenAnywhere(m_serverName);
}

// ---- options -------------------------------------------------------------------

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

// ---- hotkeys -------------------------------------------------------------------

QVariantList LayoutModel::hotkeyList() const
{
    if (!m_mirrorHotkeys.isEmpty()) {
        return m_mirrorHotkeys;   // read from a file, or shared by an older build: text only
    }
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

int LayoutModel::addHotkey(const KeySequence& keys)
{
    if (!keys.valid()) {
        return -1;
    }
    for (const Hotkey& existing : m_hotkeys) {
        if (existing.keySequence().toString() == keys.toString()) {
            return -1;
        }
    }
    Hotkey hotkey;
    hotkey.setKeySequence(keys);
    m_hotkeys.push_back(hotkey);
    hotkeysEdited();
    return int(m_hotkeys.size()) - 1;
}

bool LayoutModel::addAction(int hotkeyIndex, const Action& action)
{
    if (hotkeyIndex < 0 || hotkeyIndex >= int(m_hotkeys.size())) {
        return false;
    }
    m_hotkeys[hotkeyIndex].appendAction(action);
    hotkeysEdited();
    return true;
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

// ---- persistence ---------------------------------------------------------------

void LayoutModel::load()
{
    if (m_store == nullptr) {
        return;
    }
    QSettings& s = *m_store;
    s.beginGroup(QStringLiteral("internalConfig"));

    beginResetModel();
    m_screens.clear();

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

    if (s.value(QStringLiteral("layoutVersion"), 1).toInt() < kLayoutVersion) {
        loadLegacyGrid(s);
    } else {
        const int screens = s.beginReadArray(QStringLiteral("screens"));
        for (int i = 0; i < screens; ++i) {
            s.setArrayIndex(i);
            ScreenConfig screen;
            loadScreen(screen);
            if (!screen.isNull() && !hasScreen(screen.name)) {
                m_screens << screen;
            }
        }
        s.endArray();
    }

    m_hotkeys.clear();
    const int hotkeys = s.beginReadArray(QStringLiteral("hotkeys"));
    for (int i = 0; i < hotkeys; ++i) {
        s.setArrayIndex(i);
        Hotkey hotkey;
        hotkey.loadSettings(s);
        m_hotkeys.push_back(hotkey);
    }
    s.endArray();

    loadGapLinks(QJsonDocument::fromJson(s.value(QStringLiteral("gapLinks")).toString().toUtf8()).array());
    loadSetups(s.value(QStringLiteral("setups")).toString());
    m_activeSetup = s.value(QStringLiteral("activeSetup"), -1).toInt();
    if (m_activeSetup >= m_setups.size()) {
        m_activeSetup = -1;
    }

    s.endGroup();
    endResetModel();
    emit layoutChanged();
    emit optionsChanged();
    emit hotkeysChanged();
}

// The grid era: `numColumns` x `numRows` cells, the screens array indexed by
// cell. Each occupied cell becomes a default-sized desktop at the cell's
// place, so neighbours still touch exactly as they did.
void LayoutModel::loadLegacyGrid(QSettings& s)
{
    const int columns = qMax(1, s.value(QStringLiteral("numColumns"), 5).toInt());
    const int screens = s.beginReadArray(QStringLiteral("screens"));
    for (int i = 0; i < screens; ++i) {
        s.setArrayIndex(i);
        ScreenConfig screen;
        loadScreen(screen);
        if (screen.isNull() || hasScreen(screen.name)) {
            continue;
        }
        screen.displays = {DisplayConfig()};
        screen.x = (i % columns) * kDefaultWidth;
        screen.y = (i / columns) * kDefaultHeight;
        m_screens << screen;
    }
    s.endArray();
}

void LayoutModel::loadScreen(ScreenConfig& screen)
{
    QSettings& s = *m_store;
    screen.clear();
    screen.name = s.value(QStringLiteral("name")).toString();
    if (screen.isNull()) {
        return;
    }
    screen.x = s.value(QStringLiteral("x"), 0).toInt();
    screen.y = s.value(QStringLiteral("y"), 0).toInt();
    screen.scale = qBound(kMinScale, s.value(QStringLiteral("scale"), 1000).toInt(), kMaxScale);
    screen.scaleTyped = s.value(QStringLiteral("scaleTyped"), false).toBool();
    screen.displays.clear();
    const int displays = s.beginReadArray(QStringLiteral("displayArray"));
    for (int i = 0; i < displays; ++i) {
        s.setArrayIndex(i);
        DisplayConfig d;
        d.x = s.value(QStringLiteral("x"), 0).toInt();
        d.y = s.value(QStringLiteral("y"), 0).toInt();
        d.width = s.value(QStringLiteral("width"), kDefaultWidth).toInt();
        d.height = s.value(QStringLiteral("height"), kDefaultHeight).toInt();
        d.mmWidth = qMax(0, s.value(QStringLiteral("mmWidth"), 0).toInt());
        d.mmHeight = qMax(0, s.value(QStringLiteral("mmHeight"), 0).toInt());
        screen.displays << d;
    }
    s.endArray();
    screen.normaliseDisplays();
    screen.displaysReported = s.value(QStringLiteral("displaysReported"), false).toBool();
    // absent before typing was told apart from never reported: reports win once
    screen.displaysTyped = s.value(QStringLiteral("displaysTyped"), false).toBool();
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
    s.setValue(QStringLiteral("x"), screen.x);
    s.setValue(QStringLiteral("y"), screen.y);
    s.beginWriteArray(QStringLiteral("displayArray"));
    for (int i = 0; i < screen.displays.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("x"), screen.displays[i].x);
        s.setValue(QStringLiteral("y"), screen.displays[i].y);
        s.setValue(QStringLiteral("width"), screen.displays[i].width);
        s.setValue(QStringLiteral("height"), screen.displays[i].height);
        s.setValue(QStringLiteral("mmWidth"), screen.displays[i].mmWidth);
        s.setValue(QStringLiteral("mmHeight"), screen.displays[i].mmHeight);
    }
    s.endArray();
    s.setValue(QStringLiteral("displaysReported"), screen.displaysReported);
    s.setValue(QStringLiteral("displaysTyped"), screen.displaysTyped);
    s.setValue(QStringLiteral("scale"), screen.scale);
    s.setValue(QStringLiteral("scaleTyped"), screen.scaleTyped);
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

void LayoutModel::setReadOnly(bool readOnly)
{
    if (m_readOnly != readOnly) {
        m_readOnly = readOnly;
        emit readOnlyChanged();
    }
}

void LayoutModel::save()
{
    emit edited();
    if (m_store == nullptr) {
        return;
    }
    QSettings& s = *m_store;
    s.beginGroup(QStringLiteral("internalConfig"));
    s.remove(QString());

    s.setValue(QStringLiteral("layoutVersion"), kLayoutVersion);
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
    for (int i = 0; i < m_screens.size(); ++i) {
        s.setArrayIndex(i);
        saveScreen(m_screens[i]);
    }
    s.endArray();

    s.beginWriteArray(QStringLiteral("hotkeys"));
    for (int i = 0; i < int(m_hotkeys.size()); ++i) {
        s.setArrayIndex(i);
        m_hotkeys[i].saveSettings(s);
    }
    s.endArray();

    s.setValue(QStringLiteral("gapLinks"), QString::fromUtf8(QJsonDocument(gapLinksJson()).toJson(QJsonDocument::Compact)));
    recordSetup();
    s.setValue(QStringLiteral("setups"), setupsJson());
    s.setValue(QStringLiteral("activeSetup"), m_activeSetup);

    s.endGroup();
}

// ---- setups ---------------------------------------------------------------

// The arrangement as it is now, under the setup shown (a new one when -1).
void LayoutModel::recordSetup()
{
    Setup current;
    for (const ScreenConfig& s : std::as_const(m_screens)) {
        current.insert(s.name.toLower(), s);
    }
    if (m_activeSetup < 0 || m_activeSetup >= m_setups.size()) {
        m_setups.prepend(current);
        m_activeSetup = 0;
        while (m_setups.size() > kMaxSetups) {
            m_setups.removeLast();
        }
    } else {
        m_setups[m_activeSetup] = current;
    }
}

// The stored setup where `index` has `displays` and every other computer it
// knows has the displays it has now, the one knowing the most of them; its
// desktops go back where they were. A computer it did not know stays, unless
// it now overlaps: then it goes right of everything.
bool LayoutModel::restoreSetup(int index, const QVector<DisplayConfig>& displays)
{
    const QString changedName = m_screens[index].name.toLower();
    int best = -1, bestCommon = -1;
    for (int i = 0; i < m_setups.size(); ++i) {
        if (i == m_activeSetup) continue;
        const Setup& setup = m_setups[i];
        const auto mine = setup.constFind(changedName);
        if (mine == setup.constEnd() || mine->displays != displays) continue;
        int common = 0;
        bool same = true;
        for (const ScreenConfig& s : std::as_const(m_screens)) {
            const QString key = s.name.toLower();
            if (key == changedName) continue;
            const auto other = setup.constFind(key);
            if (other == setup.constEnd()) continue;
            if (other->displays != s.displays) {
                same = false;
                break;
            }
            ++common;
        }
        if (same && common > bestCommon) {
            best = i;
            bestCommon = common;
        }
    }
    if (best < 0) {
        return false;
    }
    const Setup setup = m_setups[best];
    QVector<int> unknown;
    for (int i = 0; i < m_screens.size(); ++i) {
        ScreenConfig& s = m_screens[i];
        const auto stored = setup.constFind(s.name.toLower());
        if (stored == setup.constEnd()) {
            unknown << i;
            continue;
        }
        s.x = stored->x;
        s.y = stored->y;
        s.displays = stored->displays;
        s.displaysReported = stored->displaysReported;
        s.displaysTyped = stored->displaysTyped;
        s.scale = stored->scale;
        s.scaleTyped = stored->scaleTyped;
    }
    for (int i : std::as_const(unknown)) {
        if (overlapsAnother(i, m_screens[i].desktop())) {
            QRect box;
            for (int j = 0; j < m_screens.size(); ++j) {
                if (j != i) box = box.isNull() ? m_screens[j].desktop() : box.united(m_screens[j].desktop());
            }
            m_screens[i].x = rightOf(box);
            m_screens[i].y = box.y();
        }
    }
    m_activeSetup = best;
    save();
    changed(0, m_screens.size() - 1);
    return true;
}

QString LayoutModel::setupsJson() const
{
    QJsonArray setups;
    for (const Setup& setup : m_setups) {
        QJsonArray screens;
        for (const ScreenConfig& s : setup) {
            QJsonObject o;
            o.insert(QStringLiteral("name"), s.name);
            o.insert(QStringLiteral("x"), s.x);
            o.insert(QStringLiteral("y"), s.y);
            o.insert(QStringLiteral("displays"), displaysToJson(s.displays));
            o.insert(QStringLiteral("displaysReported"), s.displaysReported);
            o.insert(QStringLiteral("displaysTyped"), s.displaysTyped);
            o.insert(QStringLiteral("scale"), s.scale);
            o.insert(QStringLiteral("scaleTyped"), s.scaleTyped);
            screens << o;
        }
        setups << screens;
    }
    return QString::fromUtf8(QJsonDocument(setups).toJson(QJsonDocument::Compact));
}

void LayoutModel::loadSetups(const QString& json)
{
    m_setups.clear();
    const QJsonArray setups = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const QJsonValue& value : setups) {
        Setup setup;
        for (const QJsonValue& screen : value.toArray()) {
            const QJsonObject o = screen.toObject();
            ScreenConfig s;
            s.name = o.value(QStringLiteral("name")).toString();
            if (s.isNull()) continue;
            s.x = o.value(QStringLiteral("x")).toInt();
            s.y = o.value(QStringLiteral("y")).toInt();
            const QVector<DisplayConfig> displays = displaysFromJson(o.value(QStringLiteral("displays")).toArray());
            if (!displays.isEmpty()) s.displays = displays;
            s.displaysReported = o.value(QStringLiteral("displaysReported")).toBool();
            s.displaysTyped = o.value(QStringLiteral("displaysTyped")).toBool();
            s.scale = qBound(kMinScale, o.value(QStringLiteral("scale")).toInt(1000), kMaxScale);
            s.scaleTyped = o.value(QStringLiteral("scaleTyped")).toBool();
            setup.insert(s.name.toLower(), s);
        }
        if (!setup.isEmpty() && m_setups.size() < kMaxSetups) {
            m_setups << setup;
        }
    }
}

// ---- JSON (the settings channel) -----------------------------------------------

QJsonObject LayoutModel::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("serverName"), m_serverName);
    QJsonArray screens;
    for (const ScreenConfig& s : m_screens) {
        QJsonObject screen;
        screen.insert(QStringLiteral("name"), s.name);
        screen.insert(QStringLiteral("x"), s.x);
        screen.insert(QStringLiteral("y"), s.y);
        QJsonArray displays;
        for (const DisplayConfig& d : s.displays) {
            displays << QJsonObject::fromVariantMap(displayMap(d));
        }
        screen.insert(QStringLiteral("displays"), displays);
        screen.insert(QStringLiteral("displaysReported"), s.displaysReported);
        screen.insert(QStringLiteral("displaysTyped"), s.displaysTyped);
        screen.insert(QStringLiteral("scale"), s.scale);
        screen.insert(QStringLiteral("scaleTyped"), s.scaleTyped);
        screen.insert(QStringLiteral("aliases"), QJsonArray::fromStringList(s.aliases));
        QJsonArray modifiers, corners, fixes;
        for (int m : s.modifiers) modifiers << m;
        for (bool c : s.corners) corners << c;
        for (bool f : s.fixes) fixes << f;
        screen.insert(QStringLiteral("modifiers"), modifiers);
        screen.insert(QStringLiteral("corners"), corners);
        screen.insert(QStringLiteral("cornerSize"), s.cornerSize);
        screen.insert(QStringLiteral("fixes"), fixes);
        screens << screen;
    }
    json.insert(QStringLiteral("screens"), screens);
    json.insert(QStringLiteral("gapLinks"), gapLinksJson());

    QJsonObject options;
    options.insert(QStringLiteral("hasHeartbeat"), m_hasHeartbeat);
    options.insert(QStringLiteral("heartbeat"), m_heartbeat);
    options.insert(QStringLiteral("relativeMouseMoves"), m_relativeMouseMoves);
    options.insert(QStringLiteral("screenSaverSync"), m_screenSaverSync);
    options.insert(QStringLiteral("win32KeepForeground"), m_win32KeepForeground);
    options.insert(QStringLiteral("hasSwitchDelay"), m_hasSwitchDelay);
    options.insert(QStringLiteral("switchDelay"), m_switchDelay);
    options.insert(QStringLiteral("hasSwitchDoubleTap"), m_hasSwitchDoubleTap);
    options.insert(QStringLiteral("switchDoubleTap"), m_switchDoubleTap);
    options.insert(QStringLiteral("switchCornerSize"), m_switchCornerSize);
    QJsonArray switchCorners;
    for (bool c : m_switchCorners) switchCorners << c;
    options.insert(QStringLiteral("switchCorners"), switchCorners);
    options.insert(QStringLiteral("ignoreAutoConfigClient"), m_ignoreAutoConfigClient);
    options.insert(QStringLiteral("enableDragAndDrop"), m_enableDragAndDrop);
    options.insert(QStringLiteral("clipboardSharing"), m_clipboardSharing);
    options.insert(QStringLiteral("clipboardSharingSizeMb"), clipboardSharingSizeMb());
    options.insert(QStringLiteral("dimmingEnabled"), m_dimmingEnabled);
    options.insert(QStringLiteral("dimmingPercentage"), m_dimmingPercentage);
    json.insert(QStringLiteral("options"), options);

    json.insert(QStringLiteral("hotkeys"), QJsonArray::fromVariantList(hotkeyList()));
    if (m_mirrorHotkeys.isEmpty()) {
        QJsonArray hotkeyData;   // the whole hotkeys, so a client can edit them
        for (const Hotkey& hotkey : m_hotkeys) {
            hotkeyData << hotkey.toJson();
        }
        json.insert(QStringLiteral("hotkeyData"), hotkeyData);
    }
    return json;
}

bool LayoutModel::replaceWith(const QJsonObject& json, QString* error)
{
    static const QRegularExpression namePattern(QStringLiteral("^[A-Za-z0-9._-]{1,255}$"));
    auto refuse = [error](const QString& why) {
        if (error != nullptr) *error = why;
        return false;
    };
    const QJsonArray screens = json.value(QStringLiteral("screens")).toArray();
    if (!json.value(QStringLiteral("screens")).isArray() || !json.value(QStringLiteral("options")).isObject()) {
        return refuse(tr("not a server configuration"));
    }
    if (screens.size() > 64) {
        return refuse(tr("%n computers is too many", "", int(screens.size())));
    }
    for (const QJsonValue& value : screens) {
        const QString name = value.toObject().value(QStringLiteral("name")).toString();
        if (!namePattern.match(name).hasMatch()) {
            return refuse(tr("\"%1\" is not a valid screen name").arg(name.left(40)));
        }
        if (value.toObject().value(QStringLiteral("displays")).toArray().size() > 16) {
            return refuse(tr("%1 has too many displays").arg(name));
        }
    }
    if (json.value(QStringLiteral("hotkeyData")).toArray().size() > 256) {
        return refuse(tr("too many hotkeys"));
    }
    const QString server = m_serverName;
    loadJson(json);
    m_serverName = server;
    save();
    changed(0, m_screens.size() - 1);
    return true;
}

void LayoutModel::loadJson(const QJsonObject& json)
{
    beginResetModel();
    m_screens.clear();
    m_serverName = json.value(QStringLiteral("serverName")).toString();
    const QJsonArray screens = json.value(QStringLiteral("screens")).toArray();
    for (const QJsonValue& value : screens) {
        const QJsonObject o = value.toObject();
        ScreenConfig s;
        s.name = o.value(QStringLiteral("name")).toString();
        if (s.isNull() || hasScreen(s.name)) continue;
        s.x = o.value(QStringLiteral("x")).toInt();
        s.y = o.value(QStringLiteral("y")).toInt();
        s.scale = qBound(kMinScale, o.value(QStringLiteral("scale")).toInt(1000), kMaxScale);
        s.scaleTyped = o.value(QStringLiteral("scaleTyped")).toBool(false);
        s.displays.clear();
        const QJsonArray displays = o.value(QStringLiteral("displays")).toArray();
        for (const QJsonValue& dv : displays) {
            const QJsonObject d = dv.toObject();
            DisplayConfig display;
            display.x = d.value(QStringLiteral("x")).toInt();
            display.y = d.value(QStringLiteral("y")).toInt();
            display.width = d.value(QStringLiteral("width")).toInt(kDefaultWidth);
            display.height = d.value(QStringLiteral("height")).toInt(kDefaultHeight);
            display.mmWidth = qBound(0, d.value(QStringLiteral("mmWidth")).toInt(), 10000);
            display.mmHeight = qBound(0, d.value(QStringLiteral("mmHeight")).toInt(), 10000);
            s.displays << display;
        }
        s.normaliseDisplays();
        s.displaysReported = o.value(QStringLiteral("displaysReported")).toBool(false);
        s.displaysTyped = o.value(QStringLiteral("displaysTyped")).toBool(false);
        for (const QJsonValue& a : o.value(QStringLiteral("aliases")).toArray()) s.aliases << a.toString();
        const QJsonArray modifiers = o.value(QStringLiteral("modifiers")).toArray();
        for (int i = 0; i < modifiers.size() && i < s.modifiers.size(); ++i) s.modifiers[i] = qBound(0, modifiers[i].toInt(), int(ScreenConfig::None));
        const QJsonArray corners = o.value(QStringLiteral("corners")).toArray();
        for (int i = 0; i < corners.size() && i < s.corners.size(); ++i) s.corners[i] = corners[i].toBool();
        s.cornerSize = qMax(0, o.value(QStringLiteral("cornerSize")).toInt());
        const QJsonArray fixes = o.value(QStringLiteral("fixes")).toArray();
        for (int i = 0; i < fixes.size() && i < s.fixes.size(); ++i) s.fixes[i] = fixes[i].toBool();
        m_screens << s;
    }

    const QJsonObject options = json.value(QStringLiteral("options")).toObject();
    m_hasHeartbeat = options.value(QStringLiteral("hasHeartbeat")).toBool(false);
    m_heartbeat = options.value(QStringLiteral("heartbeat")).toInt(5000);
    m_relativeMouseMoves = options.value(QStringLiteral("relativeMouseMoves")).toBool(false);
    m_screenSaverSync = options.value(QStringLiteral("screenSaverSync")).toBool(true);
    m_win32KeepForeground = options.value(QStringLiteral("win32KeepForeground")).toBool(false);
    m_hasSwitchDelay = options.value(QStringLiteral("hasSwitchDelay")).toBool(false);
    m_switchDelay = options.value(QStringLiteral("switchDelay")).toInt(250);
    m_hasSwitchDoubleTap = options.value(QStringLiteral("hasSwitchDoubleTap")).toBool(false);
    m_switchDoubleTap = options.value(QStringLiteral("switchDoubleTap")).toInt(250);
    m_switchCornerSize = options.value(QStringLiteral("switchCornerSize")).toInt(0);
    const QJsonArray switchCorners = options.value(QStringLiteral("switchCorners")).toArray();
    for (int i = 0; i < m_switchCorners.size(); ++i) m_switchCorners[i] = i < switchCorners.size() && switchCorners[i].toBool();
    m_ignoreAutoConfigClient = options.value(QStringLiteral("ignoreAutoConfigClient")).toBool(false);
    m_enableDragAndDrop = options.value(QStringLiteral("enableDragAndDrop")).toBool(true);
    m_clipboardSharing = options.value(QStringLiteral("clipboardSharing")).toBool(true);
    m_clipboardSharingSize = qulonglong(qMax(1, options.value(QStringLiteral("clipboardSharingSizeMb")).toInt(100))) * 1000 * 1000;
    m_dimmingEnabled = options.value(QStringLiteral("dimmingEnabled")).toBool(false);
    m_dimmingPercentage = qBound(0, options.value(QStringLiteral("dimmingPercentage")).toInt(30), 100);

    m_hotkeys.clear();
    m_mirrorHotkeys.clear();
    loadGapLinks(json.value(QStringLiteral("gapLinks")).toArray());
    if (json.value(QStringLiteral("hotkeyData")).isArray()) {
        for (const QJsonValue& value : json.value(QStringLiteral("hotkeyData")).toArray()) {
            Hotkey hotkey;
            hotkey.loadJson(value.toObject());
            if (hotkey.keySequence().valid() && !hotkey.keySequence().sequence().isEmpty()) {
                m_hotkeys.push_back(hotkey);
            }
        }
    } else {
        m_mirrorHotkeys = json.value(QStringLiteral("hotkeys")).toArray().toVariantList();
    }
    endResetModel();
    emit layoutChanged();
    emit optionsChanged();
    emit hotkeysChanged();
}

// ---- a .conf of the core's format --------------------------------------------------

void LayoutModel::loadConfigText(const QString& text, const QString& serverName)
{
    struct RawLink { int from; int side; int a1; int a2; QString to; int b1; int b2; };
    QVector<ScreenConfig> screens;
    QVector<RawLink> links;
    QVariantList hotkeys;
    auto indexNamed = [&screens](const QString& name) {
        for (int i = 0; i < screens.size(); ++i) {
            if (screens[i].name.compare(name, Qt::CaseInsensitive) == 0) return i;
        }
        return -1;
    };
    auto ensureScreen = [&](const QString& name) {
        int i = indexNamed(name);
        if (i < 0) {
            ScreenConfig s;
            s.name = name;
            screens << s;
            i = screens.size() - 1;
        }
        return i;
    };

    // options come back as defaults first
    m_hasHeartbeat = false; m_heartbeat = 5000; m_relativeMouseMoves = false; m_screenSaverSync = true;
    m_win32KeepForeground = false; m_hasSwitchDelay = false; m_switchDelay = 250; m_hasSwitchDoubleTap = false;
    m_switchDoubleTap = 250; m_switchCornerSize = 0; m_switchCorners.fill(false); m_ignoreAutoConfigClient = false;
    m_enableDragAndDrop = true; m_clipboardSharing = true; m_clipboardSharingSize = 100ull * 1000 * 1000;
    m_dimmingEnabled = false; m_dimmingPercentage = 30;

    static const QRegularExpression linkPattern(QStringLiteral("^(left|right|up|down)\\s*(?:\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\))?\\s*=\\s*([^\\s(]+)\\s*(?:\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\))?\\s*$"));
    static const QStringList modifierNames{QStringLiteral("shift"), QStringLiteral("ctrl"), QStringLiteral("alt"), QStringLiteral("meta"), QStringLiteral("super"), QStringLiteral("none")};
    static const QStringList fixNames{QStringLiteral("halfDuplexCapsLock"), QStringLiteral("halfDuplexNumLock"), QStringLiteral("halfDuplexScrollLock"), QStringLiteral("xtestIsXineramaUnaware"), QStringLiteral("preserveFocus")};
    static const QStringList cornerNames{QStringLiteral("top-left"), QStringLiteral("top-right"), QStringLiteral("bottom-left"), QStringLiteral("bottom-right")};
    auto parseCorners = [&](const QString& value, QVector<bool>& corners) {
        const QStringList parts = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString& part : parts) {
            if (part == QLatin1String("all")) corners.fill(true);
            else if (part == QLatin1String("none")) corners.fill(false);
            else if (part.startsWith(QLatin1Char('+')) || part.startsWith(QLatin1Char('-'))) {
                const int c = cornerNames.indexOf(part.mid(1));
                if (c >= 0) corners[c] = part.startsWith(QLatin1Char('+'));
            }
        }
    };
    auto isTrue = [](const QString& v) { return v == QLatin1String("true") || v == QLatin1String("yes") || v == QLatin1String("on") || v == QLatin1String("1"); };

    QString section;
    int current = -1;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        const int hash = line.indexOf(QLatin1Char('#'));
        if (hash >= 0) line.truncate(hash);
        line = line.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith(QLatin1String("section:"))) {
            section = line.mid(8).trimmed();
            current = -1;
            continue;
        }
        if (line == QLatin1String("end")) { section.clear(); current = -1; continue; }
        const bool header = line.endsWith(QLatin1Char(':')) && !line.contains(QLatin1Char('='));
        if (section == QLatin1String("screens")) {
            if (header) { current = ensureScreen(line.chopped(1).trimmed()); continue; }
            if (current < 0) continue;
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq < 0) continue;
            const QString key = line.left(eq).trimmed();
            const QString value = line.mid(eq + 1).trimmed();
            ScreenConfig& s = screens[current];
            const int modifier = modifierNames.indexOf(key);
            const int fix = fixNames.indexOf(key);
            if (modifier >= 0 && modifier < ScreenConfig::None) {
                const int to = modifierNames.indexOf(value);
                if (to >= 0) s.modifiers[modifier] = to;
            } else if (fix >= 0) {
                s.fixes[fix] = isTrue(value);
            } else if (key == QLatin1String("switchCorners")) {
                parseCorners(value, s.corners);
            } else if (key == QLatin1String("switchCornerSize")) {
                s.cornerSize = value.toInt();
            }
        } else if (section == QLatin1String("links")) {
            if (header) { current = ensureScreen(line.chopped(1).trimmed()); continue; }
            if (current < 0) continue;
            const QRegularExpressionMatch m = linkPattern.match(line);
            if (!m.hasMatch()) continue;
            RawLink link;
            link.from = current;
            const QString dir = m.captured(1);
            link.side = dir == QLatin1String("left") ? Left : dir == QLatin1String("right") ? Right : dir == QLatin1String("up") ? Up : Down;
            link.a1 = m.captured(2).isEmpty() ? 0 : m.captured(2).toInt();
            link.a2 = m.captured(3).isEmpty() ? 100 : m.captured(3).toInt();
            link.to = m.captured(4);
            link.b1 = m.captured(5).isEmpty() ? 0 : m.captured(5).toInt();
            link.b2 = m.captured(6).isEmpty() ? 100 : m.captured(6).toInt();
            ensureScreen(link.to);
            links << link;
        } else if (section == QLatin1String("aliases")) {
            if (header) { current = ensureScreen(line.chopped(1).trimmed()); continue; }
            if (current >= 0 && !screens[current].aliases.contains(line)) screens[current].aliases << line;
        } else if (section == QLatin1String("options")) {
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq < 0) continue;
            const QString key = line.left(eq).trimmed();
            const QString value = line.mid(eq + 1).trimmed();
            if (key.startsWith(QLatin1String("keystroke(")) || key.startsWith(QLatin1String("mousebutton("))) {
                QVariantMap hotkey;
                hotkey.insert(QStringLiteral("key"), key);
                hotkey.insert(QStringLiteral("actions"), QVariantList{value});
                hotkey.insert(QStringLiteral("text"), line);
                hotkeys << hotkey;
            } else if (key == QLatin1String("heartbeat")) { m_hasHeartbeat = true; m_heartbeat = value.toInt(); }
            else if (key == QLatin1String("relativeMouseMoves")) m_relativeMouseMoves = isTrue(value);
            else if (key == QLatin1String("screenSaverSync")) m_screenSaverSync = isTrue(value);
            else if (key == QLatin1String("win32KeepForeground")) m_win32KeepForeground = isTrue(value);
            else if (key == QLatin1String("switchDelay")) { m_hasSwitchDelay = true; m_switchDelay = value.toInt(); }
            else if (key == QLatin1String("switchDoubleTap")) { m_hasSwitchDoubleTap = true; m_switchDoubleTap = value.toInt(); }
            else if (key == QLatin1String("switchCorners")) parseCorners(value, m_switchCorners);
            else if (key == QLatin1String("switchCornerSize")) m_switchCornerSize = value.toInt();
            else if (key == QLatin1String("clipboardSharing")) m_clipboardSharing = isTrue(value);
            else if (key == QLatin1String("clipboardSharingSize")) m_clipboardSharingSize = value.toULongLong();
            else if (key == QLatin1String("screenDimmingEnabled")) m_dimmingEnabled = isTrue(value);
            else if (key == QLatin1String("screenDimmingPercentage")) m_dimmingPercentage = qBound(0, value.toInt(), 100);
        }
    }

    // the file has no sizes: a computer that reported its displays this
    // session is drawn with them, like in a generated layout
    for (ScreenConfig& s : screens) {
        const QVector<DisplayConfig> reported = reportedFor(s.name);
        if (reported.isEmpty()) continue;
        s.displays = reported;
        s.normaliseDisplays();
        const int measured = measuredScale(s.displays);
        if (measured > 0) s.scale = measured;
        s.displaysReported = true;
    }

    // Desktops from the links: start at the server, walk outwards; a link's
    // ranges size the neighbour along the shared edge, unless it reported
    // its own displays.
    QVector<bool> placed(screens.size(), false);
    int start = indexNamed(serverName);
    if (start < 0 && !screens.isEmpty()) start = 0;
    auto place = [&](int a, int side, int a1, int a2, int b, int b1, int b2) {
        ScreenConfig& A = screens[a];
        ScreenConfig& B = screens[b];
        const QRect ra = A.desktop();
        DisplayConfig d;
        const bool horizontal = side == Left || side == Right;
        const int edgeA = horizontal ? ra.height() : ra.width();
        const int spanA = a2 - a1, spanB = b2 - b1;
        if (spanA > 0 && spanB > 0) {
            const int edgeB = qMax(100, int(qint64(edgeA) * spanA / spanB));
            if (horizontal) d.height = edgeB; else d.width = edgeB;
        }
        if (!B.displaysReported) {
            B.displays = {d};
        }
        const QRect rb = B.desktop();
        if (horizontal) {
            B.x = side == Right ? rightOf(ra) : ra.x() - rb.width();
            B.y = ra.y() + int(qint64(ra.height()) * a1 / 100) - int(qint64(rb.height()) * b1 / 100);
        } else {
            B.y = side == Down ? bottomOf(ra) : ra.y() - rb.height();
            B.x = ra.x() + int(qint64(ra.width()) * a1 / 100) - int(qint64(rb.width()) * b1 / 100);
        }
        placed[b] = true;
    };
    if (start >= 0) {
        screens[start].x = 0;
        screens[start].y = 0;
        placed[start] = true;
        bool progress = true;
        while (progress) {
            progress = false;
            for (const RawLink& l : std::as_const(links)) {
                const int to = indexNamed(l.to);
                if (to < 0 || to == l.from) continue;
                if (placed[l.from] && !placed[to]) {
                    place(l.from, l.side, l.a1, l.a2, to, l.b1, l.b2);
                    progress = true;
                } else if (!placed[l.from] && placed[to]) {
                    const int opposite = l.side == Left ? Right : l.side == Right ? Left : l.side == Up ? Down : Up;
                    place(to, opposite, l.b1, l.b2, l.from, l.a1, l.a2);
                    progress = true;
                }
            }
        }
    }
    // whatever no link reaches goes to the right end
    for (int i = 0; i < screens.size(); ++i) {
        if (placed[i]) continue;
        int right = 0;
        for (int j = 0; j < screens.size(); ++j) {
            if (placed[j]) right = qMax(right, rightOf(screens[j].desktop()));
        }
        screens[i].x = right;
        screens[i].y = 0;
        placed[i] = true;
    }

    beginResetModel();
    m_screens = screens;
    m_serverName = start >= 0 ? screens[start].name : serverName;
    m_mirrorHotkeys = hotkeys;
    endResetModel();
    emit layoutChanged();
    emit optionsChanged();
    emit hotkeysChanged();
}

// ---- the .conf -----------------------------------------------------------------

QString LayoutModel::configText() const
{
    QString text;
    QTextStream out(&text);

    out << "section: screens\n";
    for (const ScreenConfig& s : m_screens) {
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
    for (const ScreenConfig& s : m_screens) {
        if (s.aliases.isEmpty()) continue;
        out << "\t" << s.name << ":\n";
        for (const QString& alias : s.aliases) {
            out << "\t\t" << alias << "\n";
        }
    }
    out << "end\n\n";

    // Touching edges become links; a part of an edge becomes a range, the
    // percentages the core's `left(a,b) = name(c,d)` syntax expects.
    const QVector<ScreenLink> all = links();
    out << "section: links\n";
    for (int i = 0; i < m_screens.size(); ++i) {
        out << "\t" << m_screens[i].name << ":\n";
        for (const ScreenLink& link : all) {
            if (link.from != i) continue;
            out << "\t\t" << kSideNames[link.side];
            if (link.fromStart != 0 || link.fromEnd != 100) {
                out << "(" << percentText(link.fromStart) << "," << percentText(link.fromEnd) << ")";
            }
            out << " = " << m_screens[link.to].name;
            if (link.toStart != 0 || link.toEnd != 100) {
                out << "(" << percentText(link.toStart) << "," << percentText(link.toEnd) << ")";
            }
            out << "\n";
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
