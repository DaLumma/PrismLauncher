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

#include <QPointer>
#include <QWidget>

#include "ControllerAction.h"

class QEventLoop;
class QFrame;
class QVBoxLayout;
class ControllerHintBar;

/**
 * Modal panel shown on top of a window, dimming everything behind it (used for menus, confirmations and
 * the on-screen keyboard).
 *
 * If the parent window is too small to host the panel (e.g. a small desktop dialog), the overlay becomes a
 * frameless top level window instead.
 */
class ControllerOverlay : public QWidget, public ControllerActionHandler {
    Q_OBJECT

   public:
    ControllerOverlay(QWidget* parent, QSize panelSize);
    ~ControllerOverlay() override;

    /// Show the overlay and block until done() is called. Returns the result passed to done().
    int exec();
    void done(int result);

    bool handleControllerAction(ControllerAction action) override;

   protected:
    /// Layout of the panel, for subclasses to fill
    QVBoxLayout* panelLayout() const { return m_panelLayout; }
    ControllerHintBar* hintBar() const { return m_hints; }

    /// Widget that receives focus when the overlay is shown
    virtual QWidget* initialFocusWidget() { return this; }

    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool focusNextPrevChild(bool next) override;

   private:
    void reposition();

   private:
    QPointer<QWidget> m_host;
    QPointer<QWidget> m_previousFocus;
    QFrame* m_panel = nullptr;
    QVBoxLayout* m_panelLayout = nullptr;
    ControllerHintBar* m_hints = nullptr;
    QEventLoop* m_loop = nullptr;
    QSize m_panelSize;
    bool m_topLevel = false;
    int m_result = 0;
};
