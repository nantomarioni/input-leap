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

#include <QApplication>
#include <QDateTime>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
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
const int kFadeInMs = 180;
const int kFadeOutMs = 280;

QString toneDot(OverlayNotification::Tone tone) {
    switch (tone) {
    case OverlayNotification::Tone::Success: return QStringLiteral("\u25CF "); // ●
    case OverlayNotification::Tone::Warning: return QStringLiteral("\u25B2 "); // ▲
    default:                                 return QStringLiteral("\u25CF ");
    }
}

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
    m_label(new QLabel(this)),
    m_hideTimer(new QTimer(this)),
    m_anim(nullptr),
    m_tone(Tone::Info),
    m_lastShownMs(0)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(22, 12, 22, 13);
    layout->addWidget(m_label);

    QFont f = m_label->font();
    f.setPointSizeF(f.pointSizeF() + 1.5);
    f.setWeight(QFont::Medium);
    m_label->setFont(f);
    m_label->setStyleSheet(QStringLiteral("color: rgba(255,255,255,235); background: transparent;"));

    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, [this]() { animateOut(); });

    auto* effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(0.0);
    setGraphicsEffect(effect);
    m_anim = new QPropertyAnimation(effect, "opacity", this);
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
        animateOut();
    }
}

void OverlayNotification::presentText(const QString& text, Tone tone)
{
    m_tone = tone;
    m_label->setText(toneDot(tone) + text);
    adjustSize();
    repositionToBottomCenter();
    animateIn();
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

void OverlayNotification::animateIn()
{
    show();
    raise();
    m_anim->stop();
    m_anim->setDuration(kFadeInMs);
    m_anim->setStartValue(static_cast<QGraphicsOpacityEffect*>(graphicsEffect())->opacity());
    m_anim->setEndValue(1.0);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    disconnect(m_anim, &QPropertyAnimation::finished, this, nullptr);
    m_anim->start();
}

void OverlayNotification::animateOut()
{
    m_anim->stop();
    m_anim->setDuration(kFadeOutMs);
    m_anim->setStartValue(static_cast<QGraphicsOpacityEffect*>(graphicsEffect())->opacity());
    m_anim->setEndValue(0.0);
    m_anim->setEasingCurve(QEasingCurve::InCubic);
    disconnect(m_anim, &QPropertyAnimation::finished, this, nullptr);
    connect(m_anim, &QPropertyAnimation::finished, this, [this]() { hide(); });
    m_anim->start();
}

void OverlayNotification::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(0, 0, -1, -1), kCornerRadius, kCornerRadius);

    // dark HUD glass
    p.fillPath(path, QColor(28, 28, 30, 226));
    p.setPen(QPen(QColor(255, 255, 255, 26), 1));
    p.drawPath(path);

    // tone accent: tint the leading glyph by painting over it is complex;
    // instead draw a small accent bar on the left edge.
    QPainterPath accent;
    accent.addRoundedRect(QRectF(8, height() / 2.0 - 8, 3.5, 16), 2, 2);
    p.fillPath(accent, toneColor(m_tone));
}

void OverlayNotification::mousePressEvent(QMouseEvent*)
{
    if (m_onClick) {
        auto cb = m_onClick;
        m_onClick = nullptr;
        m_hideTimer->stop();
        animateOut();
        cb();
        return;
    }
    if (m_persistentKey.isEmpty()) {
        m_hideTimer->stop();
        animateOut();
    }
}

} // namespace fork_gui
} // namespace inputleap
