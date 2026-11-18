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

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>

#include <array>

#include "ControllerAction.h"

/**
 * Reads native gamepad input (through SDL3 when available) and turns it into ControllerActions.
 *
 * Directions (D-pad and left stick) and the triggers auto-repeat while held. The right stick is reported
 * as a continuous scroll request. Input is ignored while the launcher is not the active application, so
 * that playing the game with a controller does not navigate the launcher in the background.
 */
class GamepadInput : public QObject {
    Q_OBJECT

   public:
    enum class GlyphStyle { Xbox, SteamDeck, PlayStation, Nintendo };

    static GamepadInput* instance();

    /// Whether this build of the launcher has native gamepad support at all
    static bool isSupported();

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    bool hasGamepad() const;
    QString gamepadName() const;
    GlyphStyle glyphStyle() const { return m_glyphStyle; }

    /// Short text label of the physical button bound to an action, e.g. "A", "✕", "LB", "☰"
    static QString glyph(ControllerAction action, GlyphStyle style);
    QString glyph(ControllerAction action) const { return glyph(action, m_glyphStyle); }

   signals:
    void actionTriggered(ControllerAction action);
    /// Continuous scrolling, in pixels for this tick
    void scrollRequested(int dx, int dy);
    void gamepadsChanged();

   private:
    explicit GamepadInput(QObject* parent = nullptr);
    ~GamepadInput() override;

    void poll();
    void openGamepad(quint32 id);
    void closeGamepad(quint32 id);
    void updateGlyphStyle();
    void releaseAll();
    bool inputAllowed() const;

    enum HeldInput { HeldUp, HeldDown, HeldLeft, HeldRight, HeldPrevPage, HeldNextPage, HeldCount };
    void updateHeld(HeldInput input, bool down);

   private:
    QTimer m_pollTimer;
    bool m_running = false;
    QHash<quint32, void*> m_gamepads;
    GlyphStyle m_glyphStyle = GlyphStyle::Xbox;

    // D-pad buttons currently held (combined over all gamepads)
    std::array<int, HeldCount> m_buttonHeld{};
    std::array<bool, HeldCount> m_held{};
    std::array<QElapsedTimer, HeldCount> m_heldSince{};
    std::array<qint64, HeldCount> m_lastRepeat{};

    double m_scrollRemainderX = 0;
    double m_scrollRemainderY = 0;
};
