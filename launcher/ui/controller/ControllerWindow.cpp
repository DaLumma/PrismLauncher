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

#include "ControllerWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "InstanceList.h"
#include "InstanceTask.h"
#include "QObjectPtr.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "settings/SettingsObject.h"
#include "tasks/Task.h"
#include "ui/MainWindow.h"
#include "ui/dialogs/ProgressDialog.h"

#include "ControllerAccountsPage.h"
#include "ControllerChipBar.h"
#include "ControllerHintBar.h"
#include "ControllerLibraryPage.h"
#include "ControllerMenu.h"
#include "ControllerNavigator.h"
#include "ControllerOverlay.h"
#include "ControllerSettingsPage.h"
#include "ControllerStyle.h"
#include "GamepadInput.h"

ControllerWindow::ControllerWindow(QWidget* parent) : QWidget(parent)
{
    setObjectName("ControllerRoot");
    setProperty("controllerUi", true);
    setAttribute(Qt::WA_StyledBackground);
    setWindowTitle(BuildConfig.LAUNCHER_DISPLAYNAME);
    setWindowIcon(APPLICATION->logo());
    setMinimumSize(800, 500);

    m_toastTimer.setSingleShot(true);
    connect(&m_toastTimer, &QTimer::timeout, this, [this] {
        if (m_toast) {
            m_toast->hide();
        }
    });

    m_clockTimer.setInterval(5000);
    connect(&m_clockTimer, &QTimer::timeout, this, [this] {
        updateHeader();
        updateStatus();
    });
    m_clockTimer.start();

    connect(APPLICATION->accounts(), &AccountList::defaultAccountChanged, this, &ControllerWindow::updateHeader);
    connect(APPLICATION->accounts(), &AccountList::listChanged, this, &ControllerWindow::updateHeader);
    connect(GamepadInput::instance(), &GamepadInput::gamepadsChanged, this, [this] {
        updateStatus();
        updateHints();
    });

    buildUi();
}

ControllerWindow::~ControllerWindow() = default;

void ControllerWindow::buildUi()
{
    m_appliedScale = APPLICATION->settings()->get("ControllerUiScale").toInt();
    ControllerStyle::apply(this);

    m_content = new QWidget(this);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_content);

    auto* root = new QVBoxLayout(m_content);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- header -------------------------------------------------------------------------------------------------
    auto* header = new QFrame(m_content);
    header->setObjectName("ControllerHeader");
    auto* headerLayout = new QHBoxLayout(header);
    const int hPad = ControllerStyle::px(26);
    headerLayout->setContentsMargins(hPad, ControllerStyle::px(12), hPad, ControllerStyle::px(12));
    headerLayout->setSpacing(ControllerStyle::px(16));

    auto* logo = new QLabel(header);
    const int logoSize = ControllerStyle::px(40);
    logo->setPixmap(APPLICATION->logo().pixmap(logoSize, logoSize));
    headerLayout->addWidget(logo);

    m_sections = new ControllerChipBar(ControllerAction::PrevTab, ControllerAction::NextTab, 19, header);
    connect(m_sections, &ControllerChipBar::chipClicked, this, &ControllerWindow::setPage);
    headerLayout->addWidget(m_sections, 1);

    m_accountFace = new QLabel(header);
    headerLayout->addWidget(m_accountFace);
    m_accountName = new QLabel(header);
    m_accountName->setFont(ControllerStyle::font(16, QFont::DemiBold));
    headerLayout->addWidget(m_accountName);

    m_clock = new QLabel(header);
    m_clock->setFont(ControllerStyle::font(19, QFont::Bold));
    headerLayout->addSpacing(ControllerStyle::px(10));
    headerLayout->addWidget(m_clock);
    root->addWidget(header);

    // ---- pages --------------------------------------------------------------------------------------------------
    m_stack = new QStackedWidget(m_content);
    auto* stackHolder = new QWidget(m_content);
    auto* stackLayout = new QVBoxLayout(stackHolder);
    const int pad = ControllerStyle::px(24);
    stackLayout->setContentsMargins(pad, ControllerStyle::px(18), pad, ControllerStyle::px(14));
    stackLayout->addWidget(m_stack);
    root->addWidget(stackHolder, 1);

    m_library = new ControllerLibraryPage(this);
    m_accounts = new ControllerAccountsPage(this);
    m_pages = { m_library, m_accounts, new ControllerSettingsPage(this) };
    QStringList titles;
    for (auto* page : m_pages) {
        m_stack->addWidget(page);
        titles.append(page->title());
        connect(page, &ControllerPage::hintsChanged, this, [this, page] {
            if (page == currentPage()) {
                updateHints();
            }
        });
    }
    m_sections->setItems(titles);

    // ---- footer -------------------------------------------------------------------------------------------------
    auto* footer = new QFrame(m_content);
    footer->setObjectName("ControllerFooter");
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(hPad, ControllerStyle::px(8), hPad, ControllerStyle::px(8));
    m_status = new QLabel(footer);
    m_status->setProperty("dim", true);
    m_status->setFont(ControllerStyle::font(15));
    footerLayout->addWidget(m_status);
    m_hints = new ControllerHintBar(footer);
    connect(m_hints, &ControllerHintBar::hintClicked, this,
            [](ControllerAction action) { ControllerNavigator::instance()->dispatch(action); });
    footerLayout->addWidget(m_hints, 1);
    root->addWidget(footer);

    // ---- toast --------------------------------------------------------------------------------------------------
    m_toast = new QLabel(m_content);
    m_toast->setAlignment(Qt::AlignCenter);
    m_toast->setFont(ControllerStyle::font(17, QFont::DemiBold));
    m_toast->setStyleSheet(QString("background: %1; color: %2; border-radius: %3px; padding: %4px %5px;")
                               .arg(ControllerStyle::surfaceRaised().name(), ControllerStyle::text().name())
                               .arg(ControllerStyle::px(12))
                               .arg(ControllerStyle::px(12))
                               .arg(ControllerStyle::px(22)));
    m_toast->hide();

    setPage(std::clamp(m_currentPage, 0, static_cast<int>(m_pages.size()) - 1));
    updateHeader();
    updateStatus();
}

void ControllerWindow::rebuildUi()
{
    const bool hadFocus = isActiveWindow();
    delete layout();
    delete m_content;
    m_content = nullptr;
    m_pages.clear();
    buildUi();
    if (hadFocus) {
        if (auto* page = currentPage()) {
            page->pageShown();
        }
    }
}

void ControllerWindow::showWindow()
{
    m_closing = false;
    m_modeSwitch = false;
    if (APPLICATION->settings()->get("ControllerFullscreen").toBool()) {
        showFullScreen();
    } else {
        showMaximized();
    }
    raise();
    activateWindow();
    if (auto* page = currentPage()) {
        page->pageShown();
    }
}

ControllerPage* ControllerWindow::currentPage() const
{
    if (m_currentPage < 0 || m_currentPage >= m_pages.size()) {
        return nullptr;
    }
    return m_pages[m_currentPage];
}

void ControllerWindow::setPage(int index)
{
    if (index < 0 || index >= m_pages.size()) {
        return;
    }
    m_currentPage = index;
    m_stack->setCurrentIndex(index);
    m_sections->setCurrentIndex(index);
    m_pages[index]->pageShown();
    updateHints();
}

void ControllerWindow::updateHints()
{
    if (!m_hints) {
        return;
    }
    ControllerHints hints;
    if (auto* page = currentPage()) {
        hints = page->hints();
    }
    hints.append({ ControllerAction::Menu, tr("Menu") });
    m_hints->setHints(hints);
}

void ControllerWindow::updateHeader()
{
    if (!m_clock) {
        return;
    }
    m_clock->setText(QLocale().toString(QTime::currentTime(), QLocale::ShortFormat));

    const auto account = APPLICATION->accounts()->defaultAccount();
    const int faceSize = ControllerStyle::px(34);
    if (account) {
        m_accountFace->setPixmap(account->getFace(faceSize, faceSize));
        m_accountName->setText(account->profileName());
    } else {
        m_accountFace->setPixmap(QPixmap());
        m_accountName->setText(APPLICATION->accounts()->count() > 0 ? tr("No default account") : tr("Not signed in"));
    }
}

void ControllerWindow::updateStatus()
{
    if (!m_status) {
        return;
    }
    auto* instances = APPLICATION->instances();
    QStringList running;
    for (int i = 0; i < instances->count(); i++) {
        if (instances->at(i)->isRunning()) {
            running.append(instances->at(i)->name());
        }
    }
    if (!running.isEmpty()) {
        m_status->setText(QStringLiteral("● ") + tr("Running: %1").arg(running.join(", ")));
    } else if (GamepadInput::isSupported() && GamepadInput::instance()->hasGamepad()) {
        m_status->setText(GamepadInput::instance()->gamepadName());
    } else if (GamepadInput::isSupported()) {
        m_status->setText(tr("No controller detected"));
    } else {
        m_status->setText(QString());
    }
}

void ControllerWindow::showToast(const QString& text)
{
    if (!m_toast) {
        return;
    }
    m_toast->setText(text);
    m_toast->adjustSize();
    const int bottomMargin = ControllerStyle::px(90);
    m_toast->move((m_content->width() - m_toast->width()) / 2, m_content->height() - m_toast->height() - bottomMargin);
    m_toast->raise();
    m_toast->show();
    m_toastTimer.start(2800);
}

bool ControllerWindow::handleControllerAction(ControllerAction action)
{
    switch (action) {
        case ControllerAction::PrevTab:
            setPage((m_currentPage - 1 + static_cast<int>(m_pages.size())) % static_cast<int>(m_pages.size()));
            return true;
        case ControllerAction::NextTab:
            setPage((m_currentPage + 1) % static_cast<int>(m_pages.size()));
            return true;
        case ControllerAction::Menu:
        case ControllerAction::View:
            showMainMenu();
            return true;
        case ControllerAction::Back:
            if (m_currentPage != 0) {
                setPage(0);
            } else {
                showExitMenu();
            }
            return true;
        default:
            // make sure the current page has focus again, e.g. after clicking somewhere with the mouse
            if (overlayActive()) {
                return true;
            }
            if (auto* page = currentPage(); page && !page->isAncestorOf(QApplication::focusWidget())) {
                page->pageShown();
                return page->handleControllerAction(action);
            }
            return true;
    }
}

bool ControllerWindow::overlayActive() const
{
    const auto overlays = findChildren<ControllerOverlay*>();
    for (auto* overlay : overlays) {
        if (overlay->isVisible()) {
            overlay->setFocus(Qt::OtherFocusReason);
            return true;
        }
    }
    return false;
}

bool ControllerWindow::focusNextPrevChild(bool next)
{
    handleControllerAction(next ? ControllerAction::NextTab : ControllerAction::PrevTab);
    return true;
}

void ControllerWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        // coming back from the game or from a dialog
        updateStatus();
        updateHeader();
        if (auto* page = currentPage(); page && !overlayActive() && !page->isAncestorOf(QApplication::focusWidget())) {
            page->pageShown();
        }
    }
    QWidget::changeEvent(event);
}

void ControllerWindow::showMainMenu()
{
    QList<ControllerMenu::Item> items;
    items.append({ "add", tr("Add instance"), tr("Create a new instance or import a modpack") });
    items.append({ "accounts", tr("Accounts") });
    items.append({ "reload", tr("Reload instances") });
    if (APPLICATION->updaterEnabled()) {
        items.append({ "update", tr("Check for updates") });
    }
    items.append({ "log", tr("View launcher log") });
    const bool fullscreen = isFullScreen();
    items.append({ "fullscreen", fullscreen ? tr("Leave fullscreen") : tr("Fullscreen") });
    items.append({ "desktop", tr("Switch to desktop mode"), tr("Use the regular mouse & keyboard interface") });
    items.append({ "quit", tr("Quit Prism Launcher") });

    const QString choice = ControllerMenu::choose(this, tr("Menu"), items);
    if (choice == "add") {
        setPage(0);
        m_library->addInstance();
    } else if (choice == "accounts") {
        setPage(static_cast<int>(m_pages.indexOf(m_accounts)));
    } else if (choice == "reload") {
        APPLICATION->instances()->loadList();
        showToast(tr("Instances reloaded"));
    } else if (choice == "update") {
        APPLICATION->triggerUpdateCheck();
    } else if (choice == "log") {
        APPLICATION->showLogWindow();
    } else if (choice == "fullscreen") {
        toggleFullscreen();
    } else if (choice == "desktop") {
        APPLICATION->setControllerMode(false);
    } else if (choice == "quit") {
        requestQuit();
    }
}

void ControllerWindow::showExitMenu()
{
    const QString choice = ControllerMenu::choose(
        this, tr("Leave controller mode?"),
        { { "stay", tr("Stay here") }, { "desktop", tr("Switch to desktop mode") }, { "quit", tr("Quit Prism Launcher") } });
    if (choice == "desktop") {
        APPLICATION->setControllerMode(false);
    } else if (choice == "quit") {
        requestQuit();
    }
}

void ControllerWindow::toggleFullscreen()
{
    const bool fullscreen = !isFullScreen();
    APPLICATION->settings()->set("ControllerFullscreen", fullscreen);
    if (fullscreen) {
        showFullScreen();
    } else {
        showMaximized();
    }
}

void ControllerWindow::settingsChanged()
{
    if (APPLICATION->settings()->get("ControllerUiScale").toInt() != m_appliedScale) {
        // rebuild once the current event (coming from inside the page) is done
        QTimer::singleShot(0, this, &ControllerWindow::rebuildUi);
    }
}

void ControllerWindow::runInstanceTask(InstanceTask* rawTask)
{
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(rawTask));
    QString error;
    QStringList warnings;
    connect(task.get(), &Task::failed, this, [&error](QString reason) { error = reason; });
    connect(task.get(), &Task::succeeded, this, [&warnings, &task] { warnings = task->warnings(); });

    ProgressDialog dialog(this);
    dialog.showSkipButton();
    dialog.execWithTask(task.get());

    if (!error.isEmpty()) {
        ControllerMenu::information(this, tr("Error"), error);
    } else if (!warnings.isEmpty()) {
        ControllerMenu::information(this, tr("Warnings"), warnings.join('\n'));
    }
}

void ControllerWindow::requestQuit()
{
    if (!APPLICATION->updatesAreAllowed()) {
        // a game is running
        if (!ControllerMenu::confirm(this, tr("Quit Prism Launcher?"),
                                     tr("A game is still running. The launcher will exit once it is closed."), tr("Quit"), tr("Cancel"))) {
            return;
        }
    }
    m_quitting = true;
    m_closing = true;
    close();
    if (auto* mainWindow = APPLICATION->mainWindow()) {
        mainWindow->close();
    } else {
        APPLICATION->quit();
    }
}

void ControllerWindow::closeForModeSwitch()
{
    m_modeSwitch = true;
    m_closing = true;
    close();
}

void ControllerWindow::closeEvent(QCloseEvent* event)
{
    event->accept();
    m_closing = true;
    if (m_modeSwitch || m_quitting) {
        return;
    }
    // Closed by the window manager, or by the "hide launcher while playing" option.
    // While a game is running we just hide, the window will come back when the game exits.
    if (APPLICATION->updatesAreAllowed()) {
        m_quitting = true;
        QTimer::singleShot(0, APPLICATION, [] {
            if (auto* mainWindow = APPLICATION->mainWindow()) {
                mainWindow->close();
            } else {
                APPLICATION->quit();
            }
        });
    }
}
