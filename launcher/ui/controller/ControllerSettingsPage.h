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

#include <functional>

#include "ControllerPage.h"

class QListWidget;

/// The most important launcher settings, adjustable with left / right, plus shortcuts to the full settings dialogs
class ControllerSettingsPage : public ControllerPage {
    Q_OBJECT

   public:
    explicit ControllerSettingsPage(ControllerWindow* window);

    QString title() const override { return tr("Settings"); }
    ControllerHints hints() const override;
    void pageShown() override;
    bool handleControllerAction(ControllerAction action) override;

   private:
    struct Row {
        QString title;
        QString subtitle;
        /// current value as text; empty for plain actions
        std::function<QString()> value;
        /// change the value by delta (-1 / +1); null for actions
        std::function<void(int)> change;
        /// activate (A button); defaults to change(+1)
        std::function<void()> activate;
    };

    void buildRows();
    void refresh();
    void addToggle(const QString& title, const QString& subtitle, const QString& setting);
    void addChoice(const QString& title, const QString& subtitle, const QString& setting, const QList<QPair<QVariant, QString>>& choices);
    void addAction(const QString& title, const QString& subtitle, std::function<void()> action);

   private:
    QListWidget* m_list = nullptr;
    QList<Row> m_rows;
};
