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

#include "ControllerPage.h"

class QLabel;
class QListWidget;

/// Manage Minecraft accounts: choose the default one, add Microsoft / offline accounts, refresh and remove them
class ControllerAccountsPage : public ControllerPage {
    Q_OBJECT

   public:
    explicit ControllerAccountsPage(ControllerWindow* window);

    QString title() const override { return tr("Accounts"); }
    ControllerHints hints() const override;
    void pageShown() override;
    bool handleControllerAction(ControllerAction action) override;

    void addAccount();

   private:
    void rebuild();
    void showAccountMenu();
    void setDefaultToCurrent();
    QString currentAccountId() const;

   private:
    QListWidget* m_list = nullptr;
    QLabel* m_emptyLabel = nullptr;
};
