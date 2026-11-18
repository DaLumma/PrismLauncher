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

#include <QSortFilterProxyModel>

#include <optional>

#include "ControllerPage.h"

class QLabel;
class QListView;
class QPushButton;
class MinecraftInstance;
class ControllerChipBar;

/// Instance list sorted by last launch, filtered by group and search text
class ControllerInstanceProxy : public QSortFilterProxyModel {
    Q_OBJECT

   public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    /// std::nullopt: all groups; empty string: ungrouped instances only
    void setGroupFilter(const std::optional<QString>& group);
    void setSearch(const QString& text);
    const QString& search() const { return m_search; }

   protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

   private:
    std::optional<QString> m_group;
    QString m_search;
};

class ControllerLibraryPage : public ControllerPage {
    Q_OBJECT

   public:
    explicit ControllerLibraryPage(ControllerWindow* window);

    QString title() const override { return tr("Library"); }
    ControllerHints hints() const override;
    void pageShown() override;
    bool handleControllerAction(ControllerAction action) override;

    MinecraftInstance* selectedInstance() const;
    void selectInstance(const QString& id);

    /// Actions also used by the main menu of the window
    void addInstance();

   private:
    void rebuildGroups();
    void setGroupIndex(int index);
    void updateDetails();
    void launchSelected();
    void showInstanceMenu();
    void showRunningMenu(MinecraftInstance* instance);
    void promptSearch();
    void renameInstance(MinecraftInstance* instance);
    void changeGroup(MinecraftInstance* instance);
    void deleteInstance(MinecraftInstance* instance);
    void copyInstance(MinecraftInstance* instance);
    void ensureSelection();

   private:
    ControllerInstanceProxy* m_proxy = nullptr;
    QListView* m_view = nullptr;
    ControllerChipBar* m_groups = nullptr;
    QList<std::optional<QString>> m_groupKeys;
    int m_groupIndex = 0;

    QLabel* m_emptyLabel = nullptr;
    QWidget* m_details = nullptr;
    QLabel* m_detailIcon = nullptr;
    QLabel* m_detailName = nullptr;
    QLabel* m_detailStatus = nullptr;
    QLabel* m_detailDescription = nullptr;
    QLabel* m_detailGroup = nullptr;
    QPushButton* m_playButton = nullptr;
};
