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

/// Draws "button glyph + label" pairs, e.g. "(A) Play  (X) Options", and makes them clickable / tappable.
class ControllerHintBar : public QWidget {
    Q_OBJECT

   public:
    explicit ControllerHintBar(QWidget* parent = nullptr);

    void setHints(const ControllerHints& hints);
    const ControllerHints& hints() const { return m_hints; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

   signals:
    void hintClicked(ControllerAction action);

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

   private:
    struct Layout {
        QRect badge;
        QRect label;
        QRect hit;
    };
    QList<Layout> computeLayout() const;

    ControllerHints m_hints;
};
