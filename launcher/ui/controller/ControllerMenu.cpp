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

#include "ControllerMenu.h"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

#include "ControllerHintBar.h"
#include "ControllerListDelegate.h"
#include "ControllerStyle.h"

namespace {
QSize panelSizeFor(int itemCount, bool hasMessage)
{
    const int rows = std::min(itemCount, 8);
    const int height = 150 + rows * 66 + (hasMessage ? 110 : 0);
    return { ControllerStyle::px(640), ControllerStyle::px(height) };
}
}  // namespace

QString ControllerMenu::choose(QWidget* parent, const QString& title, const QList<Item>& items, const QString& message, int current)
{
    ControllerMenu menu(parent, title, items, message, current);
    if (menu.exec() != 1) {
        return {};
    }
    return menu.m_chosen;
}

bool ControllerMenu::confirm(QWidget* parent,
                             const QString& title,
                             const QString& message,
                             const QString& yesText,
                             const QString& noText,
                             bool dangerous)
{
    QList<Item> items{ { "yes", yesText, {}, {}, true, dangerous }, { "no", noText } };
    return choose(parent, title, items, message, 1) == "yes";
}

void ControllerMenu::information(QWidget* parent, const QString& title, const QString& message)
{
    choose(parent, title, { { "ok", tr("OK") } }, message);
}

ControllerMenu::ControllerMenu(QWidget* parent, const QString& title, const QList<Item>& items, const QString& message, int current)
    : ControllerOverlay(parent, panelSizeFor(static_cast<int>(items.size()), !message.isEmpty()))
{
    auto* layout = panelLayout();

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setObjectName("ControllerTitle");
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    if (!message.isEmpty()) {
        auto* messageLabel = new QLabel(message, this);
        messageLabel->setObjectName("ControllerSubtitle");
        messageLabel->setWordWrap(true);
        messageLabel->setTextFormat(Qt::PlainText);
        layout->addWidget(messageLabel);
    }

    m_list = new QListWidget(this);
    m_list->setItemDelegate(new ControllerListDelegate(m_list, 60));
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setMouseTracking(true);
    for (const auto& item : items) {
        auto* listItem = new QListWidgetItem(item.icon, item.text, m_list);
        listItem->setData(ControllerListDelegate::IdRole, item.id);
        listItem->setData(ControllerListDelegate::SubtitleRole, item.subtitle);
        listItem->setData(ControllerListDelegate::DangerRole, item.danger);
        if (!item.enabled) {
            listItem->setFlags(listItem->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        }
    }
    layout->addWidget(m_list, 1);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        m_list->setCurrentItem(item);
        activateCurrent();
    });

    // select the requested item, or the first enabled one
    int start = std::clamp(current, 0, std::max(0, static_cast<int>(items.size()) - 1));
    for (int i = 0; i < m_list->count(); i++) {
        auto* item = m_list->item((start + i) % m_list->count());
        if (item->flags() & Qt::ItemIsEnabled) {
            m_list->setCurrentItem(item);
            break;
        }
    }

    hintBar()->setHints({ { ControllerAction::Accept, tr("Select") }, { ControllerAction::Back, tr("Back") } });
}

QWidget* ControllerMenu::initialFocusWidget()
{
    return m_list;
}

void ControllerMenu::activateCurrent()
{
    auto* item = m_list->currentItem();
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
        return;
    }
    m_chosen = item->data(ControllerListDelegate::IdRole).toString();
    done(1);
}

bool ControllerMenu::handleControllerAction(ControllerAction action)
{
    const int count = m_list->count();
    auto step = [this, count](int delta) {
        if (count == 0) {
            return;
        }
        int row = m_list->currentRow();
        for (int i = 0; i < count; i++) {
            row = ((row + delta) % count + count) % count;
            if (m_list->item(row)->flags() & Qt::ItemIsEnabled) {
                m_list->setCurrentRow(row);
                return;
            }
        }
    };
    switch (action) {
        case ControllerAction::Up:
            step(-1);
            return true;
        case ControllerAction::Down:
            step(1);
            return true;
        case ControllerAction::Accept:
            activateCurrent();
            return true;
        default:
            return ControllerOverlay::handleControllerAction(action);
    }
}
