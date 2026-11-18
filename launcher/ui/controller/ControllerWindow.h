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
#include <QTimer>
#include <QWidget>

#include "ControllerAction.h"

class QLabel;
class QStackedWidget;
class InstanceTask;
class ControllerChipBar;
class ControllerHintBar;
class ControllerPage;
class ControllerLibraryPage;
class ControllerAccountsPage;

/**
 * Fullscreen, controller friendly launcher interface ("controller mode"), designed for the Steam Deck.
 *
 * It sits on top of the regular main window (which stays hidden while controller mode is active), and reuses
 * the regular dialogs for complex tasks like creating instances; those are navigable through ControllerNavigator.
 */
class ControllerWindow : public QWidget, public ControllerActionHandler {
    Q_OBJECT

   public:
    explicit ControllerWindow(QWidget* parent = nullptr);
    ~ControllerWindow() override;

    bool handleControllerAction(ControllerAction action) override;

    void showToast(const QString& text);

    /// Run an instance creation / copy task with a progress dialog
    void runInstanceTask(InstanceTask* task);

    /// Called by the settings page when a setting that affects this window changed
    void settingsChanged();

    /// Quit the launcher (after confirmation where needed)
    void requestQuit();

    /// Hide the window because the user switched to desktop mode
    void closeForModeSwitch();
    bool isClosing() const { return m_closing; }

    void showWindow();

   protected:
    void closeEvent(QCloseEvent* event) override;
    bool focusNextPrevChild(bool next) override;
    void changeEvent(QEvent* event) override;

   private:
    void buildUi();
    void rebuildUi();
    void setPage(int index);
    void updateHints();
    void updateHeader();
    void updateStatus();
    void showMainMenu();
    void showExitMenu();
    void toggleFullscreen();
    ControllerPage* currentPage() const;
    /// If a menu / keyboard overlay is open, make sure it has the focus and return true
    bool overlayActive() const;

   private:
    QWidget* m_content = nullptr;
    ControllerChipBar* m_sections = nullptr;
    QStackedWidget* m_stack = nullptr;
    QList<ControllerPage*> m_pages;
    ControllerLibraryPage* m_library = nullptr;
    ControllerAccountsPage* m_accounts = nullptr;
    ControllerHintBar* m_hints = nullptr;
    QLabel* m_clock = nullptr;
    QLabel* m_accountFace = nullptr;
    QLabel* m_accountName = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_toast = nullptr;
    QTimer m_toastTimer;
    QTimer m_clockTimer;
    int m_currentPage = 0;
    int m_appliedScale = 0;
    bool m_closing = false;
    bool m_modeSwitch = false;
    bool m_quitting = false;
};
