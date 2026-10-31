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

#include "LogModel.h"
#include "CoreEventParser.h"

namespace {
const int kMaxLines = 10000;
} // namespace

LogModel::LogModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

int LogModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant LogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) {
        return QVariant();
    }
    const Entry& e = m_entries[index.row()];
    switch (role) {
    case TimeRole: return e.time.toString(QStringLiteral("HH:mm:ss"));
    case SourceRole: return sourceName(e.source);
    case LevelRole: return e.level;
    case TextRole: return e.text;
    case LineRole:
        return e.time.toString(QStringLiteral("HH:mm:ss")) + QStringLiteral("  ")
               + sourceName(e.source).leftJustified(7) + QStringLiteral(" ")
               + e.level.leftJustified(7) + QStringLiteral(" ") + e.text;
    default: return QVariant();
    }
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    return {{TimeRole, "time"}, {SourceRole, "source"}, {LevelRole, "level"}, {TextRole, "text"}, {LineRole, "line"}};
}

void LogModel::append(Source source, const QString& level, const QString& text, const QDateTime& time)
{
    if (m_entries.size() >= kMaxLines) {
        const int drop = m_entries.size() - kMaxLines + 1;
        beginRemoveRows(QModelIndex(), 0, drop - 1);
        m_entries.remove(0, drop);
        endRemoveRows();
    }
    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    m_entries.append(Entry{time.isValid() ? time : QDateTime::currentDateTime(), source, level, text});
    endInsertRows();
    emit countChanged();
}

void LogModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
}

QString LogModel::allText() const
{
    QString out;
    for (int i = 0; i < m_entries.size(); ++i) {
        out += data(index(i), LineRole).toString() + QLatin1Char('\n');
    }
    return out;
}

QString LogModel::sourceName(Source source)
{
    switch (source) {
    case App: return QStringLiteral("app");
    case Core: return QStringLiteral("core");
    case Service: return QStringLiteral("service");
    }
    return QString();
}

// ---- filter ------------------------------------------------------------------

LogFilterModel::LogFilterModel(LogModel* model, QObject* parent) :
    QSortFilterProxyModel(parent)
{
    setSourceModel(model);
    connect(this, &QAbstractItemModel::rowsInserted, this, &LogFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &LogFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &LogFilterModel::countChanged);
}

void LogFilterModel::setSource(const QString& source)
{
    if (m_source == source) return;
    m_source = source;
    invalidateFilter();
    emit filterChanged();
}

void LogFilterModel::setMinLevel(int level)
{
    level = qBound(0, level, 6);
    if (m_minLevel == level) return;
    m_minLevel = level;
    invalidateFilter();
    emit filterChanged();
}

void LogFilterModel::setSearch(const QString& text)
{
    if (m_search == text) return;
    m_search = text;
    invalidateFilter();
    emit filterChanged();
}

bool LogFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!m_source.isEmpty() && idx.data(LogModel::SourceRole).toString() != m_source) {
        return false;
    }
    const QString level = idx.data(LogModel::LevelRole).toString();
    if (!level.isEmpty()) {
        const int li = CoreEventParser::levelIndex(level);
        if (li > m_minLevel) {
            return false;
        }
    }
    if (!m_search.isEmpty() && !idx.data(LogModel::TextRole).toString().contains(m_search, Qt::CaseInsensitive)) {
        return false;
    }
    return true;
}
