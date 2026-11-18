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

#include "ControllerSettingsPage.h"

#include <QListWidget>
#include <QVBoxLayout>

#include "Application.h"
#include "HardwareInfo.h"
#include "settings/SettingsObject.h"

#include "ControllerListDelegate.h"
#include "ControllerMenu.h"
#include "ControllerNavigator.h"
#include "ControllerStyle.h"
#include "ControllerWindow.h"
#include "GamepadInput.h"

ControllerSettingsPage::ControllerSettingsPage(ControllerWindow* window) : ControllerPage(window)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* panel = new QFrame(this);
    panel->setObjectName("ControllerPanel");
    auto* panelLayout = new QVBoxLayout(panel);
    const int padding = ControllerStyle::px(18);
    panelLayout->setContentsMargins(padding, padding, padding, padding);

    m_list = new QListWidget(panel);
    m_list->setItemDelegate(new ControllerListDelegate(m_list, 62));
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setMouseTracking(true);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const int row = m_list->row(item);
        if (row >= 0 && row < m_rows.size()) {
            handleControllerAction(ControllerAction::Accept);
        }
    });
    connect(m_list, &QListWidget::currentRowChanged, this, &ControllerPage::hintsChanged);
    panelLayout->addWidget(m_list);
    layout->addWidget(panel, 1);

    buildRows();
    refresh();
    m_list->setCurrentRow(0);

    connect(APPLICATION, &Application::globalSettingsApplied, this, &ControllerSettingsPage::refresh);
}

void ControllerSettingsPage::addToggle(const QString& title, const QString& subtitle, const QString& setting)
{
    Row row;
    row.title = title;
    row.subtitle = subtitle;
    row.value = [setting] { return APPLICATION->settings()->get(setting).toBool() ? tr("On") : tr("Off"); };
    row.change = [setting](int) { APPLICATION->settings()->set(setting, !APPLICATION->settings()->get(setting).toBool()); };
    m_rows.append(row);
}

void ControllerSettingsPage::addChoice(const QString& title,
                                       const QString& subtitle,
                                       const QString& setting,
                                       const QList<QPair<QVariant, QString>>& choices)
{
    Row row;
    row.title = title;
    row.subtitle = subtitle;
    auto currentIndex = [setting, choices] {
        const QVariant current = APPLICATION->settings()->get(setting);
        for (int i = 0; i < choices.size(); i++) {
            if (choices[i].first.toString() == current.toString()) {
                return i;
            }
        }
        return 0;
    };
    row.value = [choices, currentIndex] { return choices.value(currentIndex()).second; };
    row.change = [setting, choices, currentIndex](int delta) {
        const int count = static_cast<int>(choices.size());
        const int next = ((currentIndex() + delta) % count + count) % count;
        APPLICATION->settings()->set(setting, choices[next].first);
    };
    m_rows.append(row);
}

void ControllerSettingsPage::addAction(const QString& title, const QString& subtitle, std::function<void()> action)
{
    Row row;
    row.title = title;
    row.subtitle = subtitle;
    row.value = [] { return QStringLiteral("›"); };
    row.activate = std::move(action);
    m_rows.append(row);
}

void ControllerSettingsPage::buildRows()
{
    addChoice(tr("Start in controller mode"), tr("Use this interface when the launcher starts"), "ControllerModeStartup",
              { { "auto", tr("In Steam Game Mode") }, { "always", tr("Always") }, { "never", tr("Never") } });

    addChoice(tr("Interface size"), tr("Scale of the controller interface"), "ControllerUiScale",
              { { 75, "75%" },
                { 90, "90%" },
                { 100, "100%" },
                { 115, "115%" },
                { 130, "130%" },
                { 150, "150%" },
                { 175, "175%" },
                { 200, "200%" } });

    {
        Row row;
        row.title = tr("Maximum memory");
        row.subtitle = tr("How much RAM Minecraft may use");
        row.value = [] { return tr("%1 GiB").arg(APPLICATION->settings()->get("MaxMemAlloc").toInt() / 1024.0, 0, 'f', 1); };
        row.change = [](int delta) {
            auto* settings = APPLICATION->settings();
            const int total = static_cast<int>(std::max<uint64_t>(HardwareInfo::totalRamMiB(), 2048));
            const int step = 512;
            int value = settings->get("MaxMemAlloc").toInt();
            value = ((value + step / 2) / step) * step + delta * step;
            value = std::clamp(value, step, std::max(step, total - 1024));
            settings->set("MaxMemAlloc", value);
            if (settings->get("MinMemAlloc").toInt() > value) {
                settings->set("MinMemAlloc", value);
            }
        };
        m_rows.append(row);
    }

    addToggle(tr("Hide launcher while playing"), tr("Close the launcher window once the game has started"), "CloseAfterLaunch");
    addToggle(tr("Quit launcher when the game exits"), tr("Useful when starting instances directly from Steam"), "QuitAfterGameStop");
    addToggle(tr("Show game log when launching"), tr("Open the console window when a game starts"), "ShowConsole");
    addToggle(tr("Show game log on errors"), tr("Open the console window when a game crashes"), "ShowConsoleOnError");
    addToggle(tr("Start game maximized"), tr("Open the Minecraft window maximized"), "LaunchMaximized");

    {
        Row row;
        row.title = tr("Gamepad in desktop windows");
        row.subtitle = GamepadInput::isSupported()
                           ? (GamepadInput::instance()->hasGamepad() ? tr("Connected: %1").arg(GamepadInput::instance()->gamepadName())
                                                                     : tr("Navigate all launcher dialogs with a gamepad"))
                           : tr("Not available in this build");
        row.value = [] { return APPLICATION->settings()->get("GamepadNavigation").toBool() ? tr("On") : tr("Off"); };
        row.change = [](int) {
            auto* settings = APPLICATION->settings();
            settings->set("GamepadNavigation", !settings->get("GamepadNavigation").toBool());
        };
        m_rows.append(row);
    }

    addAction(tr("All launcher settings"), tr("Java, appearance, tools, network…"),
              [this] { APPLICATION->ShowGlobalSettings(m_window, "launcher-settings"); });
    addAction(tr("Java settings"), tr("Java installation and arguments"),
              [this] { APPLICATION->ShowGlobalSettings(m_window, "java-settings"); });
    addAction(tr("View launcher log"), QString(), [] { APPLICATION->showLogWindow(); });
    addAction(tr("Switch to desktop mode"), tr("Use the regular mouse & keyboard interface"),
              [] { APPLICATION->setControllerMode(false); });
    addAction(tr("Quit Prism Launcher"), QString(), [this] { m_window->requestQuit(); });
}

void ControllerSettingsPage::refresh()
{
    const int current = std::max(0, m_list->currentRow());
    m_list->clear();
    for (const auto& row : m_rows) {
        auto* item = new QListWidgetItem(row.title, m_list);
        item->setData(ControllerListDelegate::SubtitleRole, row.subtitle);
        if (row.value) {
            QString value = row.value();
            if (row.change) {
                value = QStringLiteral("◀  %1  ▶").arg(value);
            }
            item->setData(ControllerListDelegate::ValueRole, value);
        }
    }
    m_list->setCurrentRow(std::min(current, m_list->count() - 1));
}

ControllerHints ControllerSettingsPage::hints() const
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }
    if (m_rows[row].change) {
        return { { ControllerAction::Accept, tr("Change") }, { ControllerAction::Left, tr("Adjust") } };
    }
    return { { ControllerAction::Accept, tr("Open") } };
}

void ControllerSettingsPage::pageShown()
{
    refresh();
    m_list->setFocus(Qt::OtherFocusReason);
}

bool ControllerSettingsPage::handleControllerAction(ControllerAction action)
{
    const int count = m_list->count();
    const int row = m_list->currentRow();
    switch (action) {
        case ControllerAction::Up:
            m_list->setCurrentRow(std::max(0, row - 1));
            return true;
        case ControllerAction::Down:
            m_list->setCurrentRow(std::min(count - 1, row + 1));
            return true;
        case ControllerAction::Left:
        case ControllerAction::Right:
            if (row >= 0 && row < m_rows.size() && m_rows[row].change) {
                m_rows[row].change(action == ControllerAction::Right ? 1 : -1);
                m_window->settingsChanged();
                refresh();
            }
            return true;
        case ControllerAction::Accept:
            if (row >= 0 && row < m_rows.size()) {
                // copy, the action may rebuild the page
                const Row current = m_rows[row];
                if (current.activate) {
                    current.activate();
                } else if (current.change) {
                    current.change(1);
                    m_window->settingsChanged();
                }
                if (!m_window->isClosing()) {
                    refresh();
                }
            }
            return true;
        default:
            return false;
    }
}
