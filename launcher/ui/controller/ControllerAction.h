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

#include <QList>
#include <QPair>
#include <QString>

/**
 * High level input actions used by the controller ("Steam Deck") UI.
 *
 * Gamepad buttons are mapped positionally, i.e. "Accept" is always the bottom face button
 * (A on Xbox / Steam Deck, Cross on PlayStation), "Back" is the right face button, and so on.
 */
enum class ControllerAction {
    Up,
    Down,
    Left,
    Right,
    Accept,    // A / Cross
    Back,      // B / Circle
    Option,    // X / Square
    Search,    // Y / Triangle
    PrevTab,   // LB / L1
    NextTab,   // RB / R1
    PrevPage,  // LT / L2
    NextPage,  // RT / R2
    Menu,      // Start / Menu (☰)
    View,      // Back / Select / View (⧉)
};

/**
 * Implemented by widgets that want to handle controller actions themselves.
 *
 * The ControllerNavigator walks up the widget tree starting at the focused widget and offers the action
 * to every widget implementing this interface. Returning true stops the propagation.
 */
class ControllerActionHandler {
   public:
    virtual ~ControllerActionHandler() = default;
    virtual bool handleControllerAction(ControllerAction action) = 0;
};

/// A single entry of the button hint bar: which button, and what it does in the current context.
using ControllerHint = QPair<ControllerAction, QString>;
using ControllerHints = QList<ControllerHint>;
