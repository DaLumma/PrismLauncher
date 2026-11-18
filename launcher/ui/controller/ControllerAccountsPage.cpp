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

#include "ControllerAccountsPage.h"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

#include "Application.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "tasks/Task.h"
#include "ui/dialogs/MSALoginDialog.h"

#include "ControllerKeyboard.h"
#include "ControllerListDelegate.h"
#include "ControllerMenu.h"
#include "ControllerStyle.h"
#include "ControllerWindow.h"
#include "GamepadInput.h"

namespace {
QString stateText(const MinecraftAccountPtr& account)
{
    switch (account->accountState()) {
        case AccountState::Unchecked:
            return QCoreApplication::translate("ControllerAccountsPage", "Not checked yet");
        case AccountState::Offline:
            return QCoreApplication::translate("ControllerAccountsPage", "Offline");
        case AccountState::Working:
            return QCoreApplication::translate("ControllerAccountsPage", "Working…");
        case AccountState::Online:
            return QCoreApplication::translate("ControllerAccountsPage", "Ready");
        case AccountState::Disabled:
            return QCoreApplication::translate("ControllerAccountsPage", "Disabled");
        case AccountState::Errored:
            return QCoreApplication::translate("ControllerAccountsPage", "Error");
        case AccountState::Expired:
            return QCoreApplication::translate("ControllerAccountsPage", "Expired, refresh needed");
        case AccountState::Gone:
            return QCoreApplication::translate("ControllerAccountsPage", "No longer exists");
    }
    return {};
}
}  // namespace

ControllerAccountsPage::ControllerAccountsPage(ControllerWindow* window) : ControllerPage(window)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(ControllerStyle::px(12));

    auto* panel = new QFrame(this);
    panel->setObjectName("ControllerPanel");
    auto* panelLayout = new QVBoxLayout(panel);
    const int padding = ControllerStyle::px(18);
    panelLayout->setContentsMargins(padding, padding, padding, padding);

    m_list = new QListWidget(panel);
    m_list->setItemDelegate(new ControllerListDelegate(m_list, 78));
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setMouseTracking(true);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] { setDefaultToCurrent(); });
    connect(m_list, &QListWidget::currentRowChanged, this, &ControllerPage::hintsChanged);
    panelLayout->addWidget(m_list);

    m_emptyLabel = new QLabel(tr("No accounts yet.\nPress %1 to sign in with your Microsoft account.")
                                  .arg(GamepadInput::instance()->glyph(ControllerAction::Search)),
                              panel);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setProperty("dim", true);
    m_emptyLabel->setFont(ControllerStyle::font(19));
    panelLayout->addWidget(m_emptyLabel);
    layout->addWidget(panel, 1);

    auto* accounts = APPLICATION->accounts();
    connect(accounts, &AccountList::listChanged, this, &ControllerAccountsPage::rebuild);
    connect(accounts, &AccountList::defaultAccountChanged, this, &ControllerAccountsPage::rebuild);
    connect(accounts, &AccountList::listActivityChanged, this, &ControllerAccountsPage::rebuild);
    connect(accounts, &QAbstractItemModel::dataChanged, this, &ControllerAccountsPage::rebuild);
    rebuild();
}

QString ControllerAccountsPage::currentAccountId() const
{
    auto* item = m_list->currentItem();
    return item ? item->data(ControllerListDelegate::IdRole).toString() : QString();
}

void ControllerAccountsPage::rebuild()
{
    const QString previous = currentAccountId();
    auto* accounts = APPLICATION->accounts();
    const auto defaultAccount = accounts->defaultAccount();

    m_list->clear();
    for (int i = 0; i < accounts->count(); i++) {
        const auto account = accounts->at(i);
        const QString name = account->hasProfile() ? account->profileName() : tr("No Minecraft profile");
        auto* item = new QListWidgetItem(QIcon(account->getFace(64, 64)), name, m_list);
        item->setData(ControllerListDelegate::IdRole, account->internalId());
        item->setData(ControllerListDelegate::SubtitleRole, QString("%1 · %2").arg(account->typeString(), stateText(account)));
        if (account == defaultAccount) {
            item->setData(ControllerListDelegate::ValueRole, tr("★ Default"));
        }
        if (account->internalId() == previous) {
            m_list->setCurrentItem(item);
        }
    }
    if (!m_list->currentItem() && m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    m_list->setVisible(m_list->count() > 0);
    m_emptyLabel->setVisible(m_list->count() == 0);
    emit hintsChanged();
}

ControllerHints ControllerAccountsPage::hints() const
{
    ControllerHints result;
    if (m_list->currentItem()) {
        result.append({ ControllerAction::Accept, tr("Use this account") });
        result.append({ ControllerAction::Option, tr("Options") });
    }
    result.append({ ControllerAction::Search, tr("Add account") });
    return result;
}

void ControllerAccountsPage::pageShown()
{
    m_list->setFocus(Qt::OtherFocusReason);
}

bool ControllerAccountsPage::handleControllerAction(ControllerAction action)
{
    const int count = m_list->count();
    switch (action) {
        case ControllerAction::Up:
            if (count > 0) {
                m_list->setCurrentRow(std::max(0, m_list->currentRow() - 1));
            }
            return true;
        case ControllerAction::Down:
            if (count > 0) {
                m_list->setCurrentRow(std::min(count - 1, m_list->currentRow() + 1));
            }
            return true;
        case ControllerAction::Left:
        case ControllerAction::Right:
            return true;
        case ControllerAction::Accept:
            setDefaultToCurrent();
            return true;
        case ControllerAction::Option:
            showAccountMenu();
            return true;
        case ControllerAction::Search:
            addAccount();
            return true;
        default:
            return false;
    }
}

void ControllerAccountsPage::setDefaultToCurrent()
{
    auto* accounts = APPLICATION->accounts();
    const QString id = currentAccountId();
    for (int i = 0; i < accounts->count(); i++) {
        if (accounts->at(i)->internalId() == id) {
            accounts->setDefaultAccount(accounts->at(i));
            m_window->showToast(tr("%1 is now the default account").arg(m_list->currentItem()->text()));
            return;
        }
    }
}

void ControllerAccountsPage::showAccountMenu()
{
    const QString id = currentAccountId();
    if (id.isEmpty()) {
        return;
    }
    auto* accounts = APPLICATION->accounts();
    int row = -1;
    for (int i = 0; i < accounts->count(); i++) {
        if (accounts->at(i)->internalId() == id) {
            row = i;
        }
    }
    if (row < 0) {
        return;
    }
    const auto account = accounts->at(row);
    const bool isDefault = account == accounts->defaultAccount();

    QList<ControllerMenu::Item> items;
    if (isDefault) {
        items.append({ "nodefault", tr("Stop using as default") });
    } else {
        items.append({ "default", tr("Use as default account") });
    }
    if (account->accountType() == AccountType::MSA) {
        items.append({ "refresh", tr("Refresh sign-in") });
    }
    items.append({ "remove", tr("Remove account"), {}, {}, true, true });

    const QString choice = ControllerMenu::choose(m_window, m_list->currentItem()->text(), items);
    if (choice == "default") {
        setDefaultToCurrent();
    } else if (choice == "nodefault") {
        accounts->setDefaultAccount(nullptr);
    } else if (choice == "refresh") {
        accounts->requestRefresh(id);
        m_window->showToast(tr("Refreshing account…"));
    } else if (choice == "remove") {
        if (ControllerMenu::confirm(m_window, tr("Remove account?"), tr("You can sign in again at any time."), tr("Remove"), tr("Cancel"),
                                    true)) {
            for (int i = 0; i < accounts->count(); i++) {
                if (accounts->at(i)->internalId() == id) {
                    accounts->removeAccount(accounts->index(i, 0));
                    break;
                }
            }
        }
    }
}

void ControllerAccountsPage::addAccount()
{
    auto* accounts = APPLICATION->accounts();
    const QString choice = ControllerMenu::choose(m_window, tr("Add account"),
                                                  { { "msa", tr("Microsoft account"), tr("Sign in with a code or QR code on your phone") },
                                                    { "offline",
                                                      tr("Offline account"),
                                                      tr("Requires a Microsoft account that owns Minecraft"),
                                                      {},
                                                      accounts->anyAccountIsValid() } });

    if (choice == "msa") {
        if (auto account = MSALoginDialog::newAccount(m_window)) {
            accounts->addAccount(account);
            if (accounts->count() == 1) {
                accounts->setDefaultAccount(account);
            }
        }
    } else if (choice == "offline") {
        auto name = ControllerKeyboard::getText(m_window, tr("Offline player name"));
        if (!name || name->trimmed().isEmpty()) {
            return;
        }
        if (const MinecraftAccountPtr account = MinecraftAccount::createOffline(name->trimmed())) {
            account->login()->start();
            accounts->addAccount(account);
            if (accounts->count() == 1) {
                accounts->setDefaultAccount(account);
            }
        }
    }
}
