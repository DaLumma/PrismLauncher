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

class ControllerWindow;

/// A top level section of the controller UI (Library, Accounts, Settings)
class ControllerPage : public QWidget, public ControllerActionHandler {
    Q_OBJECT

   public:
    explicit ControllerPage(ControllerWindow* window) : QWidget(nullptr), m_window(window) {}

    virtual QString title() const = 0;
    virtual ControllerHints hints() const = 0;
    /// Called when the page becomes visible, should focus the main widget of the page
    virtual void pageShown() {}

   signals:
    void hintsChanged();

   protected:
    ControllerWindow* m_window;
};
