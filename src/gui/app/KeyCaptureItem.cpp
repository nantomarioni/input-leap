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

#include "KeyCaptureItem.h"

#include <QKeyEvent>
#include <QMouseEvent>

KeyCaptureItem::KeyCaptureItem(QQuickItem* parent) :
    QQuickItem(parent)
{
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemIsFocusScope, false);
}

void KeyCaptureItem::startRecording()
{
    m_sequence = KeySequence();
    m_recording = true;
    forceActiveFocus();
    emit changed();
}

void KeyCaptureItem::stopRecording()
{
    if (!m_recording) {
        return;
    }
    m_recording = false;
    emit changed();
    if (m_sequence.valid()) {
        emit captured();
    }
}

void KeyCaptureItem::clear()
{
    m_sequence = KeySequence();
    m_recording = false;
    emit changed();
}

void KeyCaptureItem::keyPressEvent(QKeyEvent* event)
{
    if (!m_recording) {
        QQuickItem::keyPressEvent(event);
        return;
    }
    event->accept();
    if (m_sequence.appendKey(event->key(), int(event->modifiers()))) {
        stopRecording();
    } else {
        emit changed();
    }
}

void KeyCaptureItem::mousePressEvent(QMouseEvent* event)
{
    event->accept();
    if (!m_recording) {
        startRecording();
        return;
    }
    if (m_sequence.appendMouseButton(int(event->button()))) {
        stopRecording();
    } else {
        emit changed();
    }
}

void KeyCaptureItem::focusOutEvent(QFocusEvent* event)
{
    QQuickItem::focusOutEvent(event);
    if (m_recording) {
        m_recording = false;
        emit changed();
    }
}
