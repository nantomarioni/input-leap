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

#include <QAbstractListModel>
#include <QDateTime>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVector>
#include <QtQml/qqmlregistration.h>

/* The live log: one row per line from the app, the core or the service,
 * with the source and the core's level so the drawer can filter. Keeps the
 * last 10 000 lines.
 */
class LogModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reach it through App.logModel")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles { TimeRole = Qt::UserRole + 1, SourceRole, LevelRole, TextRole, LineRole };
    enum Source { App, Core, Service };
    Q_ENUM(Source)

    struct Entry
    {
        QDateTime time;
        Source source;
        QString level;   // "ERROR", "NOTE", ... or empty
        QString text;
    };

    explicit LogModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(Source source, const QString& level, const QString& text, const QDateTime& time = QDateTime());
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString allText() const;
    const QVector<Entry>& entries() const { return m_entries; }

    static QString sourceName(Source source);

signals:
    void countChanged();

private:
    QVector<Entry> m_entries;
};

/* The drawer's view: filters by source, minimum level and a search string. */
class LogFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reach it through App.log")
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY filterChanged)    // "", "app", "core", "service"
    Q_PROPERTY(int minLevel READ minLevel WRITE setMinLevel NOTIFY filterChanged)  // 0 ERROR .. 6 DEBUG2; 6 shows all
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filterChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    explicit LogFilterModel(LogModel* model, QObject* parent = nullptr);

    QString source() const { return m_source; }
    void setSource(const QString& source);
    int minLevel() const { return m_minLevel; }
    void setMinLevel(int level);
    QString search() const { return m_search; }
    void setSearch(const QString& text);

signals:
    void filterChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_source;
    int m_minLevel = 6;
    QString m_search;
};
