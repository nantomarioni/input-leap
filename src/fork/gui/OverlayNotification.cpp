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

#include "OverlayNotification.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>

namespace inputleap {
namespace fork_gui {

namespace {
const int kCornerRadius = 14;
const int kMarginFromBottom = 96;   // px above the bottom edge
const int kPadX = 22;
const int kPadY = 13;
const int kAccentGap = 10;          // extra room for the accent bar
const int kFadeInMs = 180;
const int kFadeOutMs = 280;

QColor toneColor(OverlayNotification::Tone tone) {
    switch (tone) {
    case OverlayNotification::Tone::Success: return QColor(0x4c, 0xd9, 0x64); // green
    case OverlayNotification::Tone::Warning: return QColor(0xff, 0xb3, 0x40); // amber
    default:                                 return QColor(0x6e, 0xb4, 0xff); // blue
    }
}
} // namespace

OverlayNotification* OverlayNotification::instance() {
    static OverlayNotification* panel = new OverlayNotification();
    return panel;
}

OverlayNotification::OverlayNotification() :
    QWidget(nullptr,
            Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool |
            Qt::WindowDoesNotAcceptFocus),
    m_hideTimer(new QTimer(this)),
    m_anim(nullptr),
    m_tone(Tone::Info),
    m_opacity(0.0),
    m_lastShownMs(0)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
#ifdef Q_OS_MACOS
    // Qt::Tool windows normally hide when the app is inactive — and the GUI
    // is nearly always inactive when an overlay fires. Keep it visible.
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#endif

    m_font = font();
    m_font.setPointSizeF(m_font.pointSizeF() + 1.5);
    m_font.setWeight(QFont::Medium);

    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this,
            [this]() { animateTo(0.0, kFadeOutMs, true); });

    m_anim = new QPropertyAnimation(this, "hudOpacity", this);
}

void OverlayNotification::setHudOpacity(qreal opacity)
{
    m_opacity = opacity;
    update();
}

void OverlayNotification::showTransient(const QString& text, Tone tone,
                                        int durationMs,
                                        std::function<void()> onClick)
{
    // A transient message never replaces an active persistent one; the
    // persistent state (e.g. "cursor locked") is the more important signal.
    if (!m_persistentKey.isEmpty()) {
        return;
    }
    // De-duplicate identical back-to-back messages (e.g. chatty log lines).
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (text == m_lastText && now - m_lastShownMs < 1500) {
        return;
    }
    m_lastText = text;
    m_lastShownMs = now;

    m_onClick = std::move(onClick);
    presentText(text, tone);
    m_hideTimer->start(durationMs);
}

void OverlayNotification::showPersistent(const QString& key, const QString& text, Tone tone)
{
    m_persistentKey = key;
    m_onClick = nullptr;
    m_hideTimer->stop();
    presentText(text, tone);
}

void OverlayNotification::dismissKey(const QString& key)
{
    if (m_persistentKey == key) {
        m_persistentKey.clear();
        animateTo(0.0, kFadeOutMs, true);
    }
}

void OverlayNotification::presentText(const QString& text, Tone tone)
{
    m_tone = tone;
    m_text = text;

    const QFontMetrics fm(m_font);
    const QSize textSize = fm.size(Qt::TextSingleLine, m_text);
    setFixedSize(textSize.width() + 2 * kPadX + kAccentGap,
                 textSize.height() + 2 * kPadY);

    repositionToBottomCenter();
    show();
    raise();
    animateTo(1.0, kFadeInMs, false);
    update();
}

void OverlayNotification::repositionToBottomCenter()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) return;
    const QRect avail = screen->availableGeometry();
    move(avail.center().x() - width() / 2,
         avail.bottom() - kMarginFromBottom - height());
}

void OverlayNotification::animateTo(qreal target, int durationMs, bool hideAtEnd)
{
    m_anim->stop();
    m_anim->setDuration(durationMs);
    m_anim->setStartValue(m_opacity);
    m_anim->setEndValue(target);
    m_anim->setEasingCurve(target > m_opacity ? QEasingCurve::OutCubic
                                              : QEasingCurve::InCubic);
    disconnect(m_anim, &QPropertyAnimation::finished, this, nullptr);
    if (hideAtEnd) {
        connect(m_anim, &QPropertyAnimation::finished, this, [this]() { hide(); });
    }
    m_anim->start();
}

void OverlayNotification::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);

    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                        kCornerRadius, kCornerRadius);

    // dark HUD glass
    p.fillPath(path, QColor(28, 28, 30, 226));
    p.setPen(QPen(QColor(255, 255, 255, 26), 1));
    p.drawPath(path);

    // tone accent bar on the left edge
    QPainterPath accent;
    accent.addRoundedRect(QRectF(9, height() / 2.0 - 8, 3.5, 16), 2, 2);
    p.fillPath(accent, toneColor(m_tone));

    // text
    p.setFont(m_font);
    p.setPen(QColor(255, 255, 255, 235));
    p.drawText(rect().adjusted(kPadX + kAccentGap, 0, -kPadX, 0),
               Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, m_text);
}

void OverlayNotification::mousePressEvent(QMouseEvent*)
{
    if (m_onClick) {
        auto cb = m_onClick;
        m_onClick = nullptr;
        m_hideTimer->stop();
        animateTo(0.0, kFadeOutMs, true);
        cb();
        return;
    }
    if (m_persistentKey.isEmpty()) {
        m_hideTimer->stop();
        animateTo(0.0, kFadeOutMs, true);
    }
}

} // namespace fork_gui
} // namespace inputleap
