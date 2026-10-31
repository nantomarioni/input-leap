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

#include <QByteArray>
#include <QObject>
#include <QString>

class QLocalServer;

/* One window per user: the first process listens on a local socket, a
 * later launch hands it a message ("show") and exits. That is how the Start
 * menu, the Dock or the installer's relaunch bring the window up when the
 * tray icon is out of reach.
 */
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(const QString& key, QObject* parent = nullptr);
    ~SingleInstance() override;

    bool isPrimary() const { return m_server != nullptr; }
    QString error() const { return m_error; }
    // From a secondary: delivers the message to the primary; false when it
    // could not be reached.
    bool notifyPrimary(const QByteArray& message);

signals:
    void messageReceived(const QByteArray& message);

private:
    QString m_key;
    QString m_error;
    QLocalServer* m_server = nullptr;
};
