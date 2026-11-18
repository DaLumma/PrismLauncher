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

#include "GamepadInput.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>

#include <cmath>

#ifdef PRISM_HAS_SDL3
#include <SDL3/SDL.h>
#endif

namespace {
constexpr int s_pollIntervalMs = 16;

// auto repeat timings for held directions / triggers
constexpr qint64 s_repeatDelayMs = 380;
constexpr qint64 s_repeatIntervalMs = 85;
constexpr qint64 s_pageRepeatDelayMs = 500;
constexpr qint64 s_pageRepeatIntervalMs = 220;

#ifdef PRISM_HAS_SDL3
// stick / trigger thresholds (SDL axes are in the range [-32768, 32767])
constexpr int s_stickPressThreshold = 16000;
constexpr int s_triggerPressThreshold = 16000;
constexpr int s_scrollDeadzone = 7000;
constexpr double s_maxScrollPerTick = 28.0;

constexpr quint16 s_valveVendorId = 0x28de;
constexpr quint16 s_steamDeckProductId = 0x1205;
#endif
}  // namespace

GamepadInput* GamepadInput::instance()
{
    static GamepadInput* s_instance = nullptr;
    if (!s_instance) {
        s_instance = new GamepadInput(QCoreApplication::instance());
    }
    return s_instance;
}

bool GamepadInput::isSupported()
{
#ifdef PRISM_HAS_SDL3
    return true;
#else
    return false;
#endif
}

GamepadInput::GamepadInput(QObject* parent) : QObject(parent)
{
    m_pollTimer.setInterval(s_pollIntervalMs);
    m_pollTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_pollTimer, &QTimer::timeout, this, &GamepadInput::poll);
}

GamepadInput::~GamepadInput()
{
    stop();
}

bool GamepadInput::start()
{
    if (m_running) {
        return true;
    }
#ifdef PRISM_HAS_SDL3
    // we never create SDL windows, so we always need background events
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    // don't steal SIGINT / SIGTERM from Qt
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_SetHint(SDL_HINT_APP_NAME, "Prism Launcher");

    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        qWarning() << "Failed to initialize SDL gamepad support:" << SDL_GetError();
        return false;
    }
    m_running = true;

    int count = 0;
    if (SDL_JoystickID* ids = SDL_GetGamepads(&count)) {
        for (int i = 0; i < count; i++) {
            openGamepad(ids[i]);
        }
        SDL_free(ids);
    }
    qDebug() << "Gamepad input started," << m_gamepads.size() << "gamepad(s) connected";
    m_pollTimer.start();
    return true;
#else
    return false;
#endif
}

void GamepadInput::stop()
{
    if (!m_running) {
        return;
    }
    m_pollTimer.stop();
    releaseAll();
#ifdef PRISM_HAS_SDL3
    for (auto* pad : std::as_const(m_gamepads)) {
        SDL_CloseGamepad(static_cast<SDL_Gamepad*>(pad));
    }
    m_gamepads.clear();
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
#endif
    m_running = false;
    emit gamepadsChanged();
}

bool GamepadInput::hasGamepad() const
{
    return !m_gamepads.isEmpty();
}

QString GamepadInput::gamepadName() const
{
#ifdef PRISM_HAS_SDL3
    if (!m_gamepads.isEmpty()) {
        if (const char* name = SDL_GetGamepadName(static_cast<SDL_Gamepad*>(m_gamepads.cbegin().value()))) {
            return QString::fromUtf8(name);
        }
    }
#endif
    return {};
}

void GamepadInput::openGamepad(quint32 id)
{
#ifdef PRISM_HAS_SDL3
    if (m_gamepads.contains(id)) {
        return;
    }
    SDL_Gamepad* pad = SDL_OpenGamepad(id);
    if (!pad) {
        qWarning() << "Failed to open gamepad" << id << SDL_GetError();
        return;
    }
    m_gamepads.insert(id, pad);
    qDebug() << "Gamepad connected:" << SDL_GetGamepadName(pad);
    updateGlyphStyle();
    emit gamepadsChanged();
#else
    Q_UNUSED(id)
#endif
}

void GamepadInput::closeGamepad(quint32 id)
{
#ifdef PRISM_HAS_SDL3
    auto it = m_gamepads.find(id);
    if (it == m_gamepads.end()) {
        return;
    }
    SDL_CloseGamepad(static_cast<SDL_Gamepad*>(it.value()));
    m_gamepads.erase(it);
    qDebug() << "Gamepad disconnected";
    releaseAll();
    updateGlyphStyle();
    emit gamepadsChanged();
#else
    Q_UNUSED(id)
#endif
}

void GamepadInput::updateGlyphStyle()
{
    // On a Steam Deck, Steam always presents its own controller layout
    if (qEnvironmentVariable("SteamDeck") == "1") {
        m_glyphStyle = GlyphStyle::SteamDeck;
        return;
    }
    m_glyphStyle = GlyphStyle::Xbox;
#ifdef PRISM_HAS_SDL3
    if (m_gamepads.isEmpty()) {
        return;
    }
    auto* pad = static_cast<SDL_Gamepad*>(m_gamepads.cbegin().value());
    if (SDL_GetGamepadVendor(pad) == s_valveVendorId && SDL_GetGamepadProduct(pad) == s_steamDeckProductId) {
        m_glyphStyle = GlyphStyle::SteamDeck;
        return;
    }
    switch (SDL_GetGamepadType(pad)) {
        case SDL_GAMEPAD_TYPE_PS3:
        case SDL_GAMEPAD_TYPE_PS4:
        case SDL_GAMEPAD_TYPE_PS5:
            m_glyphStyle = GlyphStyle::PlayStation;
            break;
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            m_glyphStyle = GlyphStyle::Nintendo;
            break;
        default:
            break;
    }
#endif
}

bool GamepadInput::inputAllowed() const
{
    // Don't react to the controller while the user is playing (or using another application)
    return QGuiApplication::applicationState() == Qt::ApplicationActive;
}

void GamepadInput::releaseAll()
{
    m_buttonHeld.fill(0);
    m_held.fill(false);
    m_scrollRemainderX = 0;
    m_scrollRemainderY = 0;
}

void GamepadInput::updateHeld(HeldInput input, bool down)
{
    static constexpr std::array<ControllerAction, HeldCount> s_actions{ ControllerAction::Up,       ControllerAction::Down,
                                                                        ControllerAction::Left,     ControllerAction::Right,
                                                                        ControllerAction::PrevPage, ControllerAction::NextPage };
    const bool isPage = input == HeldPrevPage || input == HeldNextPage;
    const qint64 delay = isPage ? s_pageRepeatDelayMs : s_repeatDelayMs;
    const qint64 interval = isPage ? s_pageRepeatIntervalMs : s_repeatIntervalMs;

    if (!down) {
        m_held[input] = false;
        return;
    }
    if (!m_held[input]) {
        m_held[input] = true;
        m_heldSince[input].start();
        m_lastRepeat[input] = 0;
        emit actionTriggered(s_actions[input]);
        return;
    }
    const qint64 elapsed = m_heldSince[input].elapsed();
    if (elapsed < delay) {
        return;
    }
    if (m_lastRepeat[input] == 0 || elapsed - m_lastRepeat[input] >= interval) {
        m_lastRepeat[input] = elapsed;
        emit actionTriggered(s_actions[input]);
    }
}

void GamepadInput::poll()
{
#ifdef PRISM_HAS_SDL3
    const bool allowed = inputAllowed();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_GAMEPAD_ADDED:
                openGamepad(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                closeGamepad(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
                const int delta = down ? 1 : -1;
                auto adjustHeld = [this, delta](HeldInput input) { m_buttonHeld[input] = std::max(0, m_buttonHeld[input] + delta); };
                switch (event.gbutton.button) {
                    case SDL_GAMEPAD_BUTTON_DPAD_UP:
                        adjustHeld(HeldUp);
                        break;
                    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
                        adjustHeld(HeldDown);
                        break;
                    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
                        adjustHeld(HeldLeft);
                        break;
                    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
                        adjustHeld(HeldRight);
                        break;
                    default:
                        break;
                }
                if (!down || !allowed) {
                    break;
                }
                switch (event.gbutton.button) {
                    case SDL_GAMEPAD_BUTTON_SOUTH:
                        emit actionTriggered(ControllerAction::Accept);
                        break;
                    case SDL_GAMEPAD_BUTTON_EAST:
                        emit actionTriggered(ControllerAction::Back);
                        break;
                    case SDL_GAMEPAD_BUTTON_WEST:
                        emit actionTriggered(ControllerAction::Option);
                        break;
                    case SDL_GAMEPAD_BUTTON_NORTH:
                        emit actionTriggered(ControllerAction::Search);
                        break;
                    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
                        emit actionTriggered(ControllerAction::PrevTab);
                        break;
                    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
                        emit actionTriggered(ControllerAction::NextTab);
                        break;
                    case SDL_GAMEPAD_BUTTON_START:
                        emit actionTriggered(ControllerAction::Menu);
                        break;
                    case SDL_GAMEPAD_BUTTON_BACK:
                        emit actionTriggered(ControllerAction::View);
                        break;
                    default:
                        break;
                }
                break;
            }
            default:
                break;
        }
    }

    if (!allowed || m_gamepads.isEmpty()) {
        releaseAll();
        return;
    }

    // Combine the analog inputs of all connected gamepads
    int leftX = 0;
    int leftY = 0;
    int rightX = 0;
    int rightY = 0;
    int leftTrigger = 0;
    int rightTrigger = 0;
    auto strongest = [](int current, int value) { return std::abs(value) > std::abs(current) ? value : current; };
    for (auto* handle : std::as_const(m_gamepads)) {
        auto* pad = static_cast<SDL_Gamepad*>(handle);
        leftX = strongest(leftX, SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX));
        leftY = strongest(leftY, SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));
        rightX = strongest(rightX, SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX));
        rightY = strongest(rightY, SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY));
        leftTrigger = std::max(leftTrigger, static_cast<int>(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)));
        rightTrigger = std::max(rightTrigger, static_cast<int>(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)));
    }

    // Only use the dominant stick axis, so diagonals don't trigger two directions at once
    const bool horizontal = std::abs(leftX) > std::abs(leftY);
    const bool stickUp = !horizontal && leftY < -s_stickPressThreshold;
    const bool stickDown = !horizontal && leftY > s_stickPressThreshold;
    const bool stickLeft = horizontal && leftX < -s_stickPressThreshold;
    const bool stickRight = horizontal && leftX > s_stickPressThreshold;

    updateHeld(HeldUp, m_buttonHeld[HeldUp] > 0 || stickUp);
    updateHeld(HeldDown, m_buttonHeld[HeldDown] > 0 || stickDown);
    updateHeld(HeldLeft, m_buttonHeld[HeldLeft] > 0 || stickLeft);
    updateHeld(HeldRight, m_buttonHeld[HeldRight] > 0 || stickRight);
    updateHeld(HeldPrevPage, leftTrigger > s_triggerPressThreshold);
    updateHeld(HeldNextPage, rightTrigger > s_triggerPressThreshold);

    // Right stick scrolls, with a quadratic response curve for finer control
    auto scrollAmount = [](int value) -> double {
        if (std::abs(value) < s_scrollDeadzone) {
            return 0.0;
        }
        const double normalized = (std::abs(value) - s_scrollDeadzone) / (32767.0 - s_scrollDeadzone);
        return std::copysign(normalized * normalized * s_maxScrollPerTick, static_cast<double>(value));
    };
    m_scrollRemainderX += scrollAmount(rightX);
    m_scrollRemainderY += scrollAmount(rightY);
    const int dx = static_cast<int>(m_scrollRemainderX);
    const int dy = static_cast<int>(m_scrollRemainderY);
    if (dx != 0 || dy != 0) {
        m_scrollRemainderX -= dx;
        m_scrollRemainderY -= dy;
        emit scrollRequested(dx, dy);
    }
#endif
}

QString GamepadInput::glyph(ControllerAction action, GlyphStyle style)
{
    auto pick = [style](const char* xbox, const char* deck, const char* playstation, const char* nintendo) {
        switch (style) {
            case GlyphStyle::SteamDeck:
                return QString::fromUtf8(deck);
            case GlyphStyle::PlayStation:
                return QString::fromUtf8(playstation);
            case GlyphStyle::Nintendo:
                return QString::fromUtf8(nintendo);
            case GlyphStyle::Xbox:
                break;
        }
        return QString::fromUtf8(xbox);
    };

    switch (action) {
        case ControllerAction::Up:
        case ControllerAction::Down:
        case ControllerAction::Left:
        case ControllerAction::Right:
            return QStringLiteral("✛");
        case ControllerAction::Accept:
            return pick("A", "A", "✕", "B");
        case ControllerAction::Back:
            return pick("B", "B", "○", "A");
        case ControllerAction::Option:
            return pick("X", "X", "□", "Y");
        case ControllerAction::Search:
            return pick("Y", "Y", "△", "X");
        case ControllerAction::PrevTab:
            return pick("LB", "L1", "L1", "L");
        case ControllerAction::NextTab:
            return pick("RB", "R1", "R1", "R");
        case ControllerAction::PrevPage:
            return pick("LT", "L2", "L2", "ZL");
        case ControllerAction::NextPage:
            return pick("RT", "R2", "R2", "ZR");
        case ControllerAction::Menu:
            return pick("☰", "☰", "☰", "+");
        case ControllerAction::View:
            return pick("⧉", "⧉", "⧉", "−");
    }
    return {};
}
