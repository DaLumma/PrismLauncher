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

#include "ControllerChipBar.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "ControllerStyle.h"
#include "GamepadInput.h"

ControllerChipBar::ControllerChipBar(ControllerAction previous, ControllerAction next, double fontSize, QWidget* parent)
    : QWidget(parent), m_previous(previous), m_next(next), m_fontSize(fontSize)
{
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(GamepadInput::instance(), &GamepadInput::gamepadsChanged, this, qOverload<>(&QWidget::update));
}

void ControllerChipBar::setItems(const QStringList& items)
{
    m_items = items;
    m_current = std::clamp(m_current, 0, std::max(0, static_cast<int>(items.size()) - 1));
    updateGeometry();
    update();
}

void ControllerChipBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_items.size()) {
        return;
    }
    m_current = index;
    update();
}

QSize ControllerChipBar::sizeHint() const
{
    return { ControllerStyle::px(600), ControllerStyle::px(m_fontSize * 2.4) };
}

QSize ControllerChipBar::minimumSizeHint() const
{
    return { ControllerStyle::px(200), ControllerStyle::px(m_fontSize * 2.4) };
}

QList<QRect> ControllerChipBar::chipRects(QRect* previousBadge, QRect* nextBadge) const
{
    const QFont font = ControllerStyle::font(m_fontSize, QFont::DemiBold);
    const QFont badgeFont = ControllerStyle::font(m_fontSize * 0.75, QFont::Bold);
    const QFontMetrics metrics(font);
    const QFontMetrics badgeMetrics(badgeFont);
    const int chipHeight = height() - ControllerStyle::px(6);
    const int padding = ControllerStyle::px(m_fontSize * 0.9);
    const int spacing = ControllerStyle::px(8);
    const int top = (height() - chipHeight) / 2;

    auto badgeRect = [&](ControllerAction action, int x) {
        const QString glyph = GamepadInput::instance()->glyph(action);
        const int w = badgeMetrics.horizontalAdvance(glyph) + ControllerStyle::px(14);
        const int h = badgeMetrics.height() + ControllerStyle::px(6);
        return QRect(x, (height() - h) / 2, w, h);
    };
    const QRect prevRect = badgeRect(m_previous, 0);
    QRect nextRect = badgeRect(m_next, 0);
    nextRect.moveRight(width() - 1);
    if (previousBadge) {
        *previousBadge = prevRect;
    }
    if (nextBadge) {
        *nextBadge = nextRect;
    }

    QList<QRect> rects;
    int x = prevRect.right() + spacing * 2;
    for (const auto& item : m_items) {
        const int w = metrics.horizontalAdvance(item) + padding * 2;
        rects.append(QRect(x, top, w, chipHeight));
        x += w + spacing;
    }

    // scroll so that the current chip is always visible
    const int available = nextRect.left() - spacing * 2;
    int offset = 0;
    if (m_current < rects.size() && rects[m_current].right() > available) {
        offset = rects[m_current].right() - available;
    }
    for (auto& r : rects) {
        r.translate(-offset, 0);
    }
    return rects;
}

void ControllerChipBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRect prevBadge;
    QRect nextBadge;
    const auto rects = chipRects(&prevBadge, &nextBadge);
    const QFont font = ControllerStyle::font(m_fontSize, QFont::DemiBold);
    const QFont badgeFont = ControllerStyle::font(m_fontSize * 0.75, QFont::Bold);

    p.save();
    p.setClipRect(QRect(prevBadge.right() + 4, 0, nextBadge.left() - prevBadge.right() - 8, height()));
    for (int i = 0; i < rects.size(); i++) {
        const bool current = i == m_current;
        if (current) {
            QPainterPath path;
            path.addRoundedRect(QRectF(rects[i]), rects[i].height() / 2.0, rects[i].height() / 2.0);
            p.fillPath(path, ControllerStyle::accent());
        }
        p.setFont(font);
        p.setPen(current ? ControllerStyle::accentText() : ControllerStyle::textDim());
        p.drawText(rects[i], Qt::AlignCenter, m_items[i]);
    }
    p.restore();

    if (m_items.size() > 1) {
        p.setFont(badgeFont);
        for (const auto& [rect, action] : { std::pair{ prevBadge, m_previous }, std::pair{ nextBadge, m_next } }) {
            QPainterPath path;
            path.addRoundedRect(QRectF(rect), ControllerStyle::px(5), ControllerStyle::px(5));
            p.fillPath(path, ControllerStyle::surfaceRaised());
            p.setPen(ControllerStyle::text());
            p.drawText(rect, Qt::AlignCenter, GamepadInput::instance()->glyph(action));
        }
    }
}

void ControllerChipBar::mouseReleaseEvent(QMouseEvent* event)
{
    const auto rects = chipRects(nullptr, nullptr);
    for (int i = 0; i < rects.size(); i++) {
        if (rects[i].contains(event->position().toPoint())) {
            emit chipClicked(i);
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}
