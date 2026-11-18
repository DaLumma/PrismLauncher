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

#pragma once

#include <QWidget>

#include "ControllerAction.h"

/// Horizontal row of "pills" (tabs / filters), switched with a pair of shoulder buttons or triggers.
class ControllerChipBar : public QWidget {
    Q_OBJECT

   public:
    ControllerChipBar(ControllerAction previous, ControllerAction next, double fontSize, QWidget* parent = nullptr);

    void setItems(const QStringList& items);
    void setCurrentIndex(int index);
    int currentIndex() const { return m_current; }
    int count() const { return static_cast<int>(m_items.size()); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

   signals:
    void chipClicked(int index);

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

   private:
    QList<QRect> chipRects(QRect* previousBadge, QRect* nextBadge) const;

    ControllerAction m_previous;
    ControllerAction m_next;
    double m_fontSize;
    QStringList m_items;
    int m_current = 0;
};
