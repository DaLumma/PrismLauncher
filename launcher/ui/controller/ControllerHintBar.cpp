// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Prism Launcher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ControllerHintBar.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "ControllerStyle.h"
#include "GamepadInput.h"

ControllerHintBar::ControllerHintBar(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::NoFocus);
    connect(GamepadInput::instance(), &GamepadInput::gamepadsChanged, this, qOverload<>(&QWidget::update));
}

void ControllerHintBar::setHints(const ControllerHints& hints)
{
    m_hints = hints;
    updateGeometry();
    update();
}

QSize ControllerHintBar::sizeHint() const
{
    return { ControllerStyle::px(400), ControllerStyle::px(44) };
}

QSize ControllerHintBar::minimumSizeHint() const
{
    return { 0, ControllerStyle::px(44) };
}

QList<ControllerHintBar::Layout> ControllerHintBar::computeLayout() const
{
    QList<Layout> result;
    const QFont labelFont = ControllerStyle::font(16, QFont::DemiBold);
    const QFont glyphFont = ControllerStyle::font(14, QFont::Bold);
    const QFontMetrics labelMetrics(labelFont);
    const QFontMetrics glyphMetrics(glyphFont);
    const int badgeHeight = ControllerStyle::px(30);
    const int spacing = ControllerStyle::px(8);
    const int groupSpacing = ControllerStyle::px(26);

    // laid out right to left, like most console UIs
    int x = width() - ControllerStyle::px(4);
    const int centerY = height() / 2;
    for (const auto& [action, label] : m_hints) {
        const QString glyph = GamepadInput::instance()->glyph(action);
        const int badgeWidth = std::max(badgeHeight, glyphMetrics.horizontalAdvance(glyph) + ControllerStyle::px(16));
        const int labelWidth = labelMetrics.horizontalAdvance(label);

        Layout l;
        l.label = QRect(x - labelWidth, centerY - labelMetrics.height() / 2, labelWidth, labelMetrics.height());
        l.badge = QRect(l.label.left() - spacing - badgeWidth, centerY - badgeHeight / 2, badgeWidth, badgeHeight);
        l.hit = l.badge.united(l.label).adjusted(-spacing, -spacing, spacing, spacing);
        result.append(l);
        x = l.badge.left() - groupSpacing;
    }
    return result;
}

void ControllerHintBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto layout = computeLayout();
    const QFont labelFont = ControllerStyle::font(16, QFont::DemiBold);
    const QFont glyphFont = ControllerStyle::font(14, QFont::Bold);
    const auto style = GamepadInput::instance()->glyphStyle();
    const bool colored = style == GamepadInput::GlyphStyle::Xbox;

    for (int i = 0; i < m_hints.size(); i++) {
        const auto& l = layout[i];
        if (l.badge.left() < 0) {
            break;  // out of space
        }
        const QString glyph = GamepadInput::instance()->glyph(m_hints[i].first);
        const QColor badgeColor = colored ? ControllerStyle::glyphColor(glyph) : ControllerStyle::surfaceRaised().lighter(140);

        QPainterPath path;
        const qreal radius = l.badge.height() / 2.0;
        path.addRoundedRect(QRectF(l.badge), radius, radius);
        p.fillPath(path, badgeColor);

        p.setFont(glyphFont);
        p.setPen(Qt::white);
        p.drawText(l.badge, Qt::AlignCenter, glyph);

        p.setFont(labelFont);
        p.setPen(ControllerStyle::text());
        p.drawText(l.label, Qt::AlignVCenter | Qt::AlignLeft, m_hints[i].second);
    }
}

void ControllerHintBar::mouseReleaseEvent(QMouseEvent* event)
{
    const auto layout = computeLayout();
    for (int i = 0; i < m_hints.size(); i++) {
        if (layout[i].hit.contains(event->position().toPoint())) {
            emit hintClicked(m_hints[i].first);
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}
