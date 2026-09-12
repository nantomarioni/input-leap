/*  InputLeap (nantomarioni fork) -- mouse and keyboard sharing utility
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

#include <QWidget>
#include <QString>
#include <functional>

class QLabel;
class QPropertyAnimation;
class QTimer;

namespace inputleap {
namespace fork_gui {

/* HUD-style overlay notification (macOS keyboard-battery look): a small
 * rounded translucent panel at the bottom-center of the primary screen,
 * fading/sliding in and out. Never takes focus, never appears in the
 * taskbar/dock.
 *
 * One panel instance shows one message at a time; a new message replaces
 * the current one. Messages can be transient (auto-dismiss) or persistent
 * (keyed; stays until dismissKey() is called). Clicking runs the optional
 * action, otherwise dismisses.
 */
class OverlayNotification : public QWidget {
    Q_OBJECT
public:
    enum class Tone { Info, Success, Warning };

    static OverlayNotification* instance();

    void showTransient(const QString& text, Tone tone = Tone::Info,
                       int durationMs = 2600,
                       std::function<void()> onClick = nullptr);

    // Persistent message: stays until dismissKey(key) (or replaced by
    // another persistent message with the same key).
    void showPersistent(const QString& key, const QString& text,
                        Tone tone = Tone::Warning);
    void dismissKey(const QString& key);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    OverlayNotification();

    void presentText(const QString& text, Tone tone);
    void animateIn();
    void animateOut();
    void repositionToBottomCenter();

    QLabel* m_label;
    QTimer* m_hideTimer;
    QPropertyAnimation* m_anim;
    Tone m_tone;
    QString m_persistentKey;   // non-empty while a persistent message shows
    std::function<void()> m_onClick;
    QString m_lastText;
    qint64 m_lastShownMs;
};

} // namespace fork_gui
} // namespace inputleap
