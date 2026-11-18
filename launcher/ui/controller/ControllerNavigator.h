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

#include <QObject>
#include <QPointer>
#include <QTimer>

#include "ControllerAction.h"

class ControllerFocusFrame;
class QWidget;

/**
 * Routes controller actions (from a gamepad, or keyboard shortcuts of the controller UI) to the UI.
 *
 * Widgets implementing ControllerActionHandler get the first chance to handle an action. Everything else,
 * i.e. the regular desktop dialogs of the launcher, is driven by translating actions into the equivalent
 * keyboard input (arrow keys, Tab, Return, Escape, ...), which makes the whole launcher usable with a gamepad.
 */
class ControllerNavigator : public QObject {
    Q_OBJECT

   public:
    static ControllerNavigator* instance();

    /// Start listening to gamepads (if supported and enabled in the settings)
    void startGamepadInput();
    void stopGamepadInput();

    /// Dispatch an action as if it came from a gamepad
    void dispatch(ControllerAction action);

    /// true if the most recent input came from a controller (as opposed to the mouse)
    bool controllerInUse() const { return m_controllerInUse; }

    /// Text input widgets (line edits, spin boxes, ...) can be edited with the on-screen keyboard
    static bool isTextInput(const QWidget* widget);
    static void editWithOnScreenKeyboard(QWidget* widget);

   signals:
    void controllerInUseChanged(bool inUse);

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

   private:
    explicit ControllerNavigator(QObject* parent = nullptr);

    void scroll(int dx, int dy);
    void handleGeneric(QWidget* window, QWidget* focus, ControllerAction action);
    void setControllerInUse(bool inUse);
    void updateFocusFrame();

   private:
    bool m_controllerInUse = false;
    QPointer<ControllerFocusFrame> m_focusFrame;
    QTimer m_focusFrameTimer;
};
