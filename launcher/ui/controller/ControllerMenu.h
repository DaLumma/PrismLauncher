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

#include <QIcon>

#include "ControllerOverlay.h"

class QListWidget;

/// A vertical list of choices in a modal panel, e.g. the instance options or a confirmation prompt.
class ControllerMenu : public ControllerOverlay {
    Q_OBJECT

   public:
    struct Item {
        QString id;
        QString text;
        QString subtitle = {};
        QIcon icon = {};
        bool enabled = true;
        bool danger = false;
    };

    /// Shows the menu and returns the id of the chosen item, or an empty string when cancelled
    static QString choose(QWidget* parent,
                          const QString& title,
                          const QList<Item>& items,
                          const QString& message = QString(),
                          int current = 0);

    /// Yes / no question. The "no" option is selected by default.
    static bool confirm(QWidget* parent,
                        const QString& title,
                        const QString& message,
                        const QString& yesText,
                        const QString& noText,
                        bool dangerous = false);

    /// Simple message with an "OK" button
    static void information(QWidget* parent, const QString& title, const QString& message);

    bool handleControllerAction(ControllerAction action) override;

   protected:
    QWidget* initialFocusWidget() override;

   private:
    ControllerMenu(QWidget* parent, const QString& title, const QList<Item>& items, const QString& message, int current);
    void activateCurrent();

   private:
    QListWidget* m_list = nullptr;
    QString m_chosen;
};
