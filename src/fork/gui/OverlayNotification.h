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
#include <QFont>
#include <QString>
#include <functional>

class QPropertyAnimation;
class QTimer;

namespace inputleap {
namespace fork_gui {

/* HUD-style overlay notification (macOS keyboard-battery look): a small
 * rounded translucent panel at the bottom-center of the primary screen,
 * fading in and out. Never takes focus, never appears in the taskbar/dock.
 *
 * Everything (background, accent, text) is painted in paintEvent with the
 * fade applied as painter opacity. Deliberately NO setWindowOpacity and NO
 * QGraphicsOpacityEffect: on Windows, whole-window opacity
 * (SetLayeredWindowAttributes) and per-pixel alpha (UpdateLayeredWindow)
 * are mutually exclusive, and opacity effects don't compose with
 * translucent top-levels.
 *
 * One panel instance shows one message at a time; a new message replaces
 * the current one. Messages are transient (auto-dismiss) or persistent
 * (keyed; stay until dismissKey()). Clicking runs the optional action,
 * otherwise dismisses.
 */
class OverlayNotification : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal hudOpacity READ hudOpacity WRITE setHudOpacity)
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
    // Returns true when a matching persistent message was actually showing.
    bool dismissKey(const QString& key);

    qreal hudOpacity() const { return m_opacity; }
    void setHudOpacity(qreal opacity);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    OverlayNotification();

    void presentText(const QString& text, Tone tone);
    void animateTo(qreal target, int durationMs, bool hideAtEnd);
    void repositionToBottomCenter();

    QString m_text;
    QFont m_font;
    QTimer* m_hideTimer;
    QPropertyAnimation* m_anim;
    Tone m_tone;
    qreal m_opacity;
    QString m_persistentKey;   // non-empty while a persistent message shows
    std::function<void()> m_onClick;
    QString m_lastText;
    qint64 m_lastShownMs;
};

} // namespace fork_gui
} // namespace inputleap
