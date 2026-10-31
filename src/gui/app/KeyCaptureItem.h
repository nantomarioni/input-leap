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

#include "KeySequence.h"

#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

/* Records a key combination or a mouse button for a hotkey or an action,
 * the way the legacy KeySequenceWidget did: click to start recording, then
 * press the keys; recording ends with the first non-modifier key or a mouse
 * button.
 */
class KeyCaptureItem : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KeyCapture)

    Q_PROPERTY(QString text READ text NOTIFY changed)
    Q_PROPERTY(bool recording READ recording NOTIFY changed)
    Q_PROPERTY(bool valid READ valid NOTIFY changed)
    Q_PROPERTY(bool isMouseButton READ isMouseButton NOTIFY changed)

public:
    explicit KeyCaptureItem(QQuickItem* parent = nullptr);

    QString text() const { return m_sequence.toString(); }
    bool recording() const { return m_recording; }
    bool valid() const { return m_sequence.valid(); }
    bool isMouseButton() const { return m_sequence.isMouseButton(); }
    const KeySequence& sequence() const { return m_sequence; }

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void clear();

signals:
    void changed();
    void captured();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    KeySequence m_sequence;
    bool m_recording = false;
};
