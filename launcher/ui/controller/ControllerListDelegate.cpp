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

#include "ControllerListDelegate.h"

#include <QAbstractItemView>
#include <QPainter>
#include <QPainterPath>

#include "ControllerStyle.h"

ControllerListDelegate::ControllerListDelegate(QObject* parent, int rowHeight) : QStyledItemDelegate(parent), m_rowHeight(rowHeight) {}

QSize ControllerListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const bool hasSubtitle = !index.data(SubtitleRole).toString().isEmpty();
    const int height = ControllerStyle::px(hasSubtitle ? m_rowHeight + 10 : m_rowHeight);
    return { option.rect.width() > 0 ? option.rect.width() : ControllerStyle::px(400), height };
}

void ControllerListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const bool current = option.state & QStyle::State_HasFocus || option.state & QStyle::State_Selected;
    const bool enabled = (index.flags() & Qt::ItemIsEnabled) != 0;
    const bool danger = index.data(DangerRole).toBool();
    const int gap = ControllerStyle::px(3);
    const QRectF rowRect = QRectF(option.rect).adjusted(0, gap, 0, -gap);
    const qreal radius = ControllerStyle::px(10);

    QPainterPath path;
    path.addRoundedRect(rowRect, radius, radius);
    if (current) {
        painter->fillPath(path, ControllerStyle::accent());
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillPath(path, ControllerStyle::surfaceRaised());
    }

    QColor titleColor = current ? ControllerStyle::accentText() : danger ? ControllerStyle::danger() : ControllerStyle::text();
    QColor subtitleColor = current ? ControllerStyle::accentText() : ControllerStyle::textDim();
    if (!enabled) {
        titleColor.setAlphaF(0.4);
        subtitleColor.setAlphaF(0.4);
    }

    const int padding = ControllerStyle::px(16);
    QRect content = rowRect.toRect().adjusted(padding, 0, -padding, 0);

    // icon
    const QVariant decoration = index.data(Qt::DecorationRole);
    const int iconSize = ControllerStyle::px(m_rowHeight - 24);
    QIcon icon;
    if (decoration.canConvert<QIcon>()) {
        icon = decoration.value<QIcon>();
    } else if (decoration.canConvert<QPixmap>()) {
        icon = QIcon(decoration.value<QPixmap>());
    }
    if (!icon.isNull()) {
        const QRect iconRect(content.left(), content.center().y() - iconSize / 2, iconSize, iconSize);
        icon.paint(painter, iconRect, Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled);
        content.setLeft(iconRect.right() + padding);
    }

    // value (right aligned)
    const QString value = index.data(ValueRole).toString();
    if (!value.isEmpty()) {
        const QFont valueFont = ControllerStyle::font(17, QFont::DemiBold);
        painter->setFont(valueFont);
        painter->setPen(current ? ControllerStyle::accentText() : ControllerStyle::accent());
        const int valueWidth = QFontMetrics(valueFont).horizontalAdvance(value);
        const QRect valueRect(content.right() - valueWidth, content.top(), valueWidth, content.height());
        painter->drawText(valueRect, Qt::AlignVCenter | Qt::AlignRight, value);
        content.setRight(valueRect.left() - padding);
    }

    const QString title = index.data(Qt::DisplayRole).toString();
    const QString subtitle = index.data(SubtitleRole).toString();
    const QFont titleFont = ControllerStyle::font(19, current ? QFont::Bold : QFont::DemiBold);
    const QFont subtitleFont = ControllerStyle::font(14);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics subtitleMetrics(subtitleFont);

    painter->setFont(titleFont);
    painter->setPen(titleColor);
    if (subtitle.isEmpty()) {
        painter->drawText(content, Qt::AlignVCenter | Qt::AlignLeft, titleMetrics.elidedText(title, Qt::ElideRight, content.width()));
    } else {
        const int total = titleMetrics.height() + subtitleMetrics.height();
        const int top = content.center().y() - total / 2;
        painter->drawText(QRect(content.left(), top, content.width(), titleMetrics.height()), Qt::AlignVCenter | Qt::AlignLeft,
                          titleMetrics.elidedText(title, Qt::ElideRight, content.width()));
        painter->setFont(subtitleFont);
        painter->setPen(subtitleColor);
        painter->drawText(QRect(content.left(), top + titleMetrics.height(), content.width(), subtitleMetrics.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, subtitleMetrics.elidedText(subtitle, Qt::ElideRight, content.width()));
    }

    painter->restore();
}
